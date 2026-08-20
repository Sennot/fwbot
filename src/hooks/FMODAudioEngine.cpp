#include <Geode/Geode.hpp>

#include "../render/dsp.hpp"
#include "../render/renderer.hpp"

using namespace geode::prelude;

#include <Geode/modify/FMODAudioEngine.hpp>
#include <safetyhook.hpp>
#include <string>

SafetyHookMid g_hook_meteringIntervalFix;

bool shouldUpdateAudio() {
    auto audio = AudioRecorder::get();
    if (!audio->m_attached) {
        return true;
    }

    if (!audio->m_shouldUpdateFmod) {
        return false;
    }

    return true;
}

struct SLAudioEngine : Modify<SLAudioEngine, FMODAudioEngine> {
    void update(float dt) {
        if (!shouldUpdateAudio()) {
            return;
        }

        AudioRecorder::get()->m_fmodTime += dt;
        FMODAudioEngine::update(dt);
    }
};

void FMOD_System_update(FMOD::System* self) {
    if (!shouldUpdateAudio()) {
        return;
    }

    auto audio = AudioRecorder::get();
    if (!audio->m_attached) {
        self->update();
        return;
    }

    auto renderer = Renderer::get();

    unsigned int bufferLength;
    int bufferCount;
    self->getDSPBufferSize(&bufferLength, &bufferCount);

    const double requiredDt = std::max(audio->m_fmodTime - audio->m_time, 0.0);

    const int requiredSamples =
        static_cast<int>(requiredDt * audio->m_sampleRate);

    int processedSamples = 0;
    while (requiredSamples > processedSamples) {
        self->update();
        processedSamples += bufferLength;
    }

    audio->syncMixFormat();

    const auto frameSize = 1024;  // just assume 1024 for now
    const auto totalFrameSize = frameSize * audio->m_channels;

    // this only takes audio from two channels of the possible more,
    // but will delete the rest correctly
    while (audio->m_buffer.size() >= (size_t)totalFrameSize) {
        uint64_t pts = (uint64_t)(audio->m_index * frameSize);

        auto result = renderer->writeAudio(audio->m_buffer, pts);
        if (result.isErr()) {
            geode::log::error("Failed to write audio! Stopping render");
            renderer->signalStop();
            return;
        }

        // and then just clip the buffer of the first frame size
        audio->m_buffer.erase(audio->m_buffer.begin(),
                              audio->m_buffer.begin() + totalFrameSize);

        audio->m_index++;
        audio->m_time = (double)audio->m_index *
                        ((double)frameSize / (double)audio->m_sampleRate);
    }
}

$execute {
    (void)Mod::get()->hook(
        reinterpret_cast<void*>(
            geode::addresser::getNonVirtual(&FMOD::System::update)),
        &FMOD_System_update, "FMOD::System::update",
        tulip::hook::TulipConvention::Stdcall);
}
