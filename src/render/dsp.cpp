#include "dsp.hpp"

#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/fmod/fmod.hpp>

#include "bot/bot.hpp"
#include "renderer.hpp"

#ifdef SL_DEV_MODE
static int collected = 0;
static int waiting = 0;
#endif

FMOD_RESULT F_CALLBACK AudioRecorder::writeCallback(FMOD_DSP_STATE*,
                                                    float* inBuffer,
                                                    float* outBuffer,
                                                    unsigned int length,
                                                    int inChannels, int*) {
    AudioRecorder* recorder = AudioRecorder::get();

    if (!recorder->m_shouldUpdateFmod) {
        return FMOD_OK;
    }

    // std::memset(outBuffer, 0, length * inChannels * sizeof(float));
    SL_LOG_DEV("length: {}, inChannels: {}, outChannels: {}", length,
               inChannels, *outChannels);

    recorder->m_lastCollectedLength = length;
    recorder->m_mixedChannels.store(inChannels, std::memory_order_relaxed);
    recorder->haltWithData(inBuffer, length * inChannels);

    std::memset(outBuffer, 0, length * inChannels * sizeof(float));

    if (recorder->m_audioPreview->inner() && recorder->m_monSystem) {
        recorder->m_monRing.push(inBuffer, length * inChannels);
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK AudioRecorder::monitorReadCallback(
    FMOD_SOUND*, void* data, unsigned int datalen) {
    auto* recorder = AudioRecorder::get();
    auto* out = static_cast<float*>(data);
    const unsigned int n = datalen / sizeof(float);
    const size_t got = recorder->m_monRing.pop(out, n);

    const float gain = recorder->m_monVolume.load(std::memory_order_relaxed);
    float attenuation = Renderer::get()->m_fadeThreshold;

    for (size_t i = 0; i < got; ++i) {
        out[i] *= gain * attenuation;
    }

    if (got < n) {
        std::memset(out + got, 0, (n - got) * sizeof(float));
    }
    return FMOD_OK;
}

void AudioRecorder::haltWithData(float* data, unsigned int length) {
    // std::lock_guard lock(m_lock);

    SL_LOG_DEV("Collected data, unpausing... {}", collected++);

    m_buffer.insert(m_buffer.end(), data, data + length);

    float attenuation = Renderer::get()->m_fadeThreshold;

    for (size_t i = m_buffer.size() - length; i < m_buffer.size(); i++) {
        m_buffer[i] = std::clamp(m_buffer[i], -1.0f, 1.0f);
        m_buffer[i] *= attenuation;
    }

    SL_LOG_DEV("Setting collected data to true");
    m_collectedData = true;
    // m_master->setPaused(true);
    // m_cv.notify_one();
}

void AudioRecorder::init() {
    FMOD_DSP_DESCRIPTION desc = {};
    strcpy_s(desc.name, "silly dsp");
    desc.version = 0x00020000;
    desc.numinputbuffers = 1;
    desc.numoutputbuffers = 1;
    desc.read = AudioRecorder::writeCallback;
    desc.numparameters = 0;

    auto engine = FMODAudioEngine::get();
    FMOD::System* system = engine->m_system;
    system->getMasterChannelGroup(&m_master);
    system->createDSP(&desc, &m_dsp);
    system->setDSPBufferSize(1024, 2);

    // m_master->setPaused(true);

    m_time = 0.0;
    m_lastPulse = 0.0;
    m_index = 0;
    m_fmodTime = 0.0;
    m_systemTime = 0.0;
    m_buffer.clear();
}

void AudioRecorder::attach(double musicVolume, double sfxVolume) {
    int numDsps;
    m_master->getNumDSPs(&numDsps);
    m_master->addDSP(numDsps, m_dsp);
    m_dsp->setMeteringEnabled(true, false);

    auto engine = FMODAudioEngine::get();

    m_previousMusicVolume = engine->getBackgroundMusicVolume();

    m_previousSFXVolume = engine->getEffectsVolume();
    m_master->setPaused(false);
    FMODAudioEngine::get()->m_system->getSoftwareFormat(&m_sampleRate, nullptr,
                                                        &m_channels);

    engine->setEffectsVolume(sfxVolume);
    engine->setBackgroundMusicVolume(musicVolume);

    engine->m_system->setOutput(FMOD_OUTPUTTYPE_NOSOUND_NRT);

    int postRate = 0, postChannels = 0;
    engine->m_system->getSoftwareFormat(&postRate, nullptr, &postChannels);
    if (postRate != m_sampleRate || postChannels != m_channels) {
        geode::log::error(
            "FMOD software format changed: {}Hz x{} "
            "-> {}Hz x{}. The encoder is configured for {}Hz, so audio will "
            "desync! ",
            m_sampleRate, m_channels, postRate, postChannels, m_sampleRate);
    }

    if (m_audioPreview->inner()) {
        startMonitor();
        m_monVolume.store(m_previousMusicVolume, std::memory_order_relaxed);
    }

    m_attached = true;
}

void AudioRecorder::startMonitor() {
    if (m_monSystem) return;  // already running

    if (FMOD::System_Create(&m_monSystem) != FMOD_OK || !m_monSystem) {
        m_monSystem = nullptr;
        geode::log::warn("[monitor] failed to create FMOD system");
        return;
    }

    m_monSystem->setSoftwareFormat(m_sampleRate, FMOD_SPEAKERMODE_DEFAULT, 0);
    if (m_monSystem->init(32, FMOD_INIT_NORMAL, nullptr) != FMOD_OK) {
        m_monSystem->release();
        m_monSystem = nullptr;
        geode::log::warn("[monitor] failed to init FMOD system");
        return;
    }

    m_monRing.init(static_cast<size_t>(m_sampleRate) * m_channels / 2);

    FMOD_CREATESOUNDEXINFO ex = {};
    ex.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
    ex.numchannels = m_channels;
    ex.defaultfrequency = m_sampleRate;
    ex.format = FMOD_SOUND_FORMAT_PCMFLOAT;
    ex.decodebuffersize = 1024;
    ex.length =
        static_cast<unsigned int>(m_sampleRate) * m_channels * sizeof(float);
    ex.pcmreadcallback = &AudioRecorder::monitorReadCallback;

    if (m_monSystem->createSound(
            nullptr, FMOD_OPENUSER | FMOD_CREATESTREAM | FMOD_LOOP_NORMAL, &ex,
            &m_monSound) != FMOD_OK) {
        geode::log::warn("[monitor] failed to create stream");
        stopMonitor();
        return;
    }

    m_monSystem->playSound(m_monSound, nullptr, false, &m_monChannel);
    geode::log::info("[monitor] audio preview running at {}Hz x{}",
                     m_sampleRate, m_channels);
}

void AudioRecorder::stopMonitor() {
    if (m_monChannel) {
        m_monChannel->stop();
        m_monChannel = nullptr;
    }
    if (m_monSound) {
        m_monSound->release();
        m_monSound = nullptr;
    }
    if (m_monSystem) {
        m_monSystem->close();
        m_monSystem->release();
        m_monSystem = nullptr;
    }
    m_monRing.clear();
}

void AudioRecorder::detach() {
    stopMonitor();

    m_master->removeDSP(m_dsp);
    m_master->setPaused(false);

    auto engine = FMODAudioEngine::get();

    engine->setEffectsVolume(m_previousSFXVolume);
    engine->setBackgroundMusicVolume(m_previousMusicVolume);
    engine->m_system->setOutput(FMOD_OUTPUTTYPE_AUTODETECT);

    m_attached = false;
}

void AudioRecorder::wait() {
    std::unique_lock lock(m_lock);
    m_cv.wait(lock, [this] { return m_collectedData; });
}

void AudioRecorder::proceed() {
    std::lock_guard lock(m_lock);

    m_collectedData = false;
    m_master->setPaused(false);
    m_cv.notify_one();
}

void AudioRecorder::uninit() { m_dsp->release(); }

float AudioRecorder::calculateTime() {
    int sampleRate, channels;
    FMODAudioEngine::get()->m_system->getSoftwareFormat(&sampleRate, nullptr,
                                                        &channels);

    return (float)m_buffer.size() / ((float)sampleRate * (float)channels);
}

void AudioRecorder::syncMixFormat() {
    const int mixed = m_mixedChannels.load(std::memory_order_relaxed);
    if (mixed <= 0 || mixed == m_channels) {
        return;
    }

    geode::log::warn(
        "FMOD is mixing {} channels but the software format reported {} - the "
        "format is read before the output switches to NRT. Using {}.",
        mixed, m_channels, mixed);

    m_channels = mixed;
    Renderer::get()->m_channels = mixed;
}

void AudioRecorder::unpause() {
    auto renderer = Renderer::get();
    auto engine = FMODAudioEngine::get();

    m_shouldUpdateFmod = true;
    engine->update(renderer->getDt());
    m_shouldUpdateFmod = false;

    if (m_monSystem) {
        m_monSystem->update();
    }
}

std::vector<float> AudioRecorder::getBuffer() {
    std::unique_lock lock(m_lock);

    m_cv.wait(lock, [this] { return m_collectedData; });

    return m_buffer;
}
