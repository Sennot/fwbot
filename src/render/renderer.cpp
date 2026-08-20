#include "renderer.hpp"

#include <Zydis/Zydis.h>
#include <immintrin.h>

#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/ShaderLayer.hpp>
#include <iostream>
#include <safetyhook.hpp>
#include <string>
#include <thread>

#include "Geode/utils/random.hpp"
#include "Geode/utils/string.hpp"
#include "bot/bot.hpp"
#include "colorspace/nv12.hpp"
#include "colorspace/rgb0.hpp"
#include "colorspace/rgb24.hpp"
#include "colorspace/yuv420p.hpp"
#include "dsp.hpp"
#include "replay/system.hpp"
#include "ui/manager.hpp"

using namespace geode::prelude;

constexpr int ALIGNMENT = 1;

namespace fs = std::filesystem;

struct RenderOpt {
    std::string m_name;
    std::string m_value;
};

static std::vector<RenderOpt> parseArgs(std::stringstream& in) {
    std::vector<RenderOpt> opts;
    while (!in.eof()) {
        std::string name, value;
        in >> name >> value;

        if (!name.starts_with("-")) {
            return opts;
        }

        name.erase(0, 1);

        if (value.starts_with("\"") && value.ends_with("\"")) {
            value.erase(0, 1);
            value.erase(value.size() - 1, value.size());
        }

        opts.push_back(RenderOpt{
            .m_name = name,
            .m_value = value,
        });
    }

    return opts;
}

void Renderer::saveSettings(fs::path& path) const {
    auto& settings = m_settings;

    (void)glz::write_file_json<glz::opts{.prettify = true}>(
        settings, geode::utils::string::pathToString(path), std::string{});
}

enum class GPUVendor {
    NVIDIA,
    AMD,
    INTEL,
    OTHER,
};

static GPUVendor getGPUVendor() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SYSTEM\\CurrentControlSet\\Enum\\PCI", 0, KEY_READ,
                      &hKey) != ERROR_SUCCESS) {
        geode::log::info("Detected no GPU - defaulting to libx264 (CPU)");
        return GPUVendor::OTHER;
    }

    char subKeyName[256];
    DWORD idx = 0;

    while (idx < 8192) {
        DWORD size = sizeof(subKeyName);
        LONG result = RegEnumKeyExA(hKey, idx++, subKeyName, &size, nullptr,
                                    nullptr, nullptr, nullptr);

        if (result != ERROR_SUCCESS) {
            break;
        }

        std::string keyName = subKeyName;

        if (keyName.find("VEN_10DE") != std::string::npos) {
            geode::log::info("Detected NVIDIA GPU - defaulting to h264_nvenc");
            RegCloseKey(hKey);
            return GPUVendor::NVIDIA;
        }

        if (keyName.find("VEN_1002") != std::string::npos ||
            keyName.find("VEN_1022") != std::string::npos) {
            geode::log::info("Detected AMD GPU - defaulting to h264_amf");
            RegCloseKey(hKey);
            return GPUVendor::AMD;
        }

        if (keyName.find("VEN_8086") != std::string::npos) {
            geode::log::info("Detected Intel GPU - defaulting to h264_qsv");
            RegCloseKey(hKey);
            return GPUVendor::INTEL;
        }
    }

    geode::log::info("Detected no GPU - defaulting to libx264 (CPU)");
    return GPUVendor::OTHER;
}

static std::string getDefaultCodec() {
    switch (getGPUVendor()) {
        case GPUVendor::NVIDIA:
            return "h264_nvenc";
        case GPUVendor::AMD:
            return "h264_amf";
        case GPUVendor::INTEL:
            return "h264_qsv";
        default:
            return "libx264";
    }
}

void Renderer::initializeDefaults() { m_settings.m_codec = getDefaultCodec(); }

void Renderer::loadSettings(fs::path& path) {
    if (!fs::exists(path)) {
        return;
    }

    auto& settings = m_settings;
    auto ec = glz::read_file_json(
        settings, geode::utils::string::pathToString(path), std::string{});
    if (ec) {
        geode::log::error("Failed to read renderer settings: {}",
                          ec.custom_error_message);
        return;
    }

    Bot::get()->ui().m_state.m_bitrate =
        settings.m_bitrate / 1000000.0;  // convert to Mbps
    SLSettings::get()->lastLoadedPreset = path.stem().string();
}

static void silentChangeSize(CCSize size) {
    auto director = CCDirector::sharedDirector();
    auto view = CCEGLView::sharedOpenGLView();
    view->CCEGLViewProtocol::setFrameSize(size.width, size.height);
    director->updateScreenScale(size);
    director->setViewport();
    director->setProjection(kCCDirectorProjection2D);
    glViewport(0, 0, size.width, size.height);
}

static void resizeShaderLayer(CCSize size) {
    auto gjbgl = GJBaseGameLayer::get();
    if (!gjbgl || !gjbgl->m_shaderLayer) {
        return;
    }

    ShaderLayer* sh = gjbgl->m_shaderLayer;
    auto winSize = CCDirector::get()->getWinSize();

    sh->m_screenSize = size;
    sh->m_scaleFactor = size.height / winSize.height;
    sh->m_aspectRatio = size.width / size.height;

    float csf = CCDirector::get()->getContentScaleFactor();
    CCDirector::get()->setContentScaleFactor(1.0);
    sh->m_renderTexture->release();
    sh->m_renderTexture = CCRenderTexture::create(
        size.width, size.height, kCCTexture2DPixelFormat_RGBA8888);
    sh->m_renderTexture->retain();
    CCDirector::get()->setContentScaleFactor(csf);
    sh->m_sprite->setTexture(sh->m_renderTexture->getSprite()->getTexture());
    sh->m_textureContentSize = sh->m_sprite->getTexture()->getContentSize();
    sh->m_targetTextureSize = size;
    sh->m_targetTextureSizeExtra = CCSize(0.0f, 0.0f);
    sh->m_sprite->setTextureRect(
        {-1.0f, -1.0f, size.width + 2.0f, size.height + 2.0f});
    sh->m_state.m_textureScaleX = size.width / winSize.width;
    sh->m_state.m_textureScaleY = size.height / winSize.height;
}

// kinda does nothing since ALIGNMENT=1, if we were to align then maybe it would
// do something but i don't wanna copypaste ts everywhere
static CCSize alignedRenderSize(const RendererSettings& settings) {
    return CCSize((settings.m_width + ALIGNMENT - 1) & ~(ALIGNMENT - 1),
                  (settings.m_height + ALIGNMENT - 1) & ~(ALIGNMENT - 1));
}

void Renderer::acquireView() {
    if (m_viewResized) {
        return;
    }

    CCSize renderSize = alignedRenderSize(m_settings);

    m_windowSize = CCDirector::get()->getOpenGLView()->getFrameSize();
    m_viewResized = true;
    silentChangeSize(renderSize);

    if (m_recording) {
        resizeShaderLayer(renderSize);
    }
}

void Renderer::reacquireView() {
    if (!m_viewResized) {
        return;
    }

    CCSize frameSize = CCDirector::get()->getOpenGLView()->getFrameSize();
    if (frameSize.equals(m_windowSize)) {
        return;
    }

    m_windowSize = frameSize;
    silentChangeSize(alignedRenderSize(m_settings));

    m_texture.m_windowWidth = m_windowSize.width;
    m_texture.m_windowHeight = m_windowSize.height;
}

void Renderer::restoreView() {
    if (!m_viewResized) {
        return;
    }

    m_viewResized = false;
    silentChangeSize(m_windowSize);
    resizeShaderLayer(m_windowSize);
}

geode::Result<> Renderer::start() {
    geode::log::info("Starting renderer");

    this->finishStop();
    if (m_recording || m_recordThread.joinable()) {
        return geode::Err("A render is already in progress");
    }

    FMODAudioEngine::get()->m_system->getSoftwareFormat(&m_sampleRate, nullptr,
                                                        &m_channels);
    m_seenFrames = 0;

    std::vector<RenderOpt> args = {};
    {
        std::stringstream s(m_settings.m_renderArgs);
        args = parseArgs(s);
    }

    std::string fileName;
    if (m_autoVideoName->inner()) {
        auto pl = PlayLayer::get();

        std::string formatted(m_videoNameTemplate->inner());  // copy

        int randomNumber = geode::utils::random::generate(0, 999'999'999);

        namespace str = geode::utils::string;

        formatted = str::replace(formatted, "%name%", pl->m_level->m_levelName);
        formatted = str::replace(
            formatted, "%id%", std::to_string(pl->m_level->m_levelID.value()));
        formatted =
            str::replace(formatted, "%creator%", pl->m_level->m_creatorName);
        formatted =
            str::replace(formatted, "%rand%", std::to_string(randomNumber));

        fileName = formatted + "." + m_settings.m_extension;
    } else {
        fileName = m_settings.m_outputPath + "." + m_settings.m_extension;
    }

    std::string outPath = geode::utils::string::pathToString(
        Mod::get()->getPersistentDir() / "videos" /
        std::filesystem::path(fileName));

    // free a stale context left over from a previous start() that failed
    // before reaching stop()
    if (m_formatCtx) {
        ff->avformat_free_context(m_formatCtx);
        m_formatCtx = nullptr;
    }

    {
        ff->avformat_alloc_output_context2(&m_formatCtx, nullptr, nullptr,
                                           outPath.c_str());
    }
    if (!m_formatCtx) {
        return geode::Err("Failed to allocate output context");
    }

    m_videoCodec = ff->avcodec_find_encoder_by_name(m_settings.m_codec.c_str());
    if (!m_videoCodec) {
        return geode::Err("Failed to find codec");
    }

    m_audioCodec =
        ff->avcodec_find_encoder_by_name(m_settings.m_audioCodec.c_str());
    if (!m_audioCodec) {
        return geode::Err("Failed to find audio codec");
    }

    m_videoCodecCtx.reset(ff->avcodec_alloc_context3(m_videoCodec));
    if (!m_videoCodecCtx) {
        return geode::Err("Failed to allocate codec context");
    }

    AVPixelFormat* outPixFmts;
    int outPixFmtCount;

    m_settings.m_pixFmt = AV_PIX_FMT_NONE;
    if (ff->avcodec_get_supported_config(
            m_videoCodecCtx.get(), m_videoCodec, AV_CODEC_CONFIG_PIX_FORMAT, 0,
            (const void**)&outPixFmts, &outPixFmtCount) < 0) {
        return geode::Err("Failed to query codec pix fmt capabilities");
    }

    std::unique_ptr<Colorspace> colorspace;
    for (int i = 0; i < outPixFmtCount; i++) {
        AVPixelFormat fmt = outPixFmts[i];

        switch (fmt) {
            case AV_PIX_FMT_YUV420P: {
                geode::log::info("Using (+) yuv420p");
                m_settings.m_pixFmt = AV_PIX_FMT_YUV420P;
                colorspace = std::make_unique<YUV420PColorspace>();
                break;
            }
            case AV_PIX_FMT_NV12: {
                geode::log::info("Using (+) nv12");
                m_settings.m_pixFmt = AV_PIX_FMT_NV12;
                colorspace = std::make_unique<NV12Colorspace>();
                break;
            }
            case AV_PIX_FMT_RGB0: {
                geode::log::info("Using (+) rgb0");
                m_settings.m_pixFmt = AV_PIX_FMT_RGB0;
                colorspace = std::make_unique<RGB0Colorspace>();
                break;
            }
            case AV_PIX_FMT_RGB24: {
                geode::log::info("Using (+) rgb24");
                m_settings.m_pixFmt = AV_PIX_FMT_RGB24;
                colorspace = std::make_unique<RGB24Colorspace>();
                break;
            }
            default: {
                break;
            }
        }

        if (m_settings.m_pixFmt != AV_PIX_FMT_NONE) {
            break;
        }
    }

    if (m_settings.m_pixFmt == AV_PIX_FMT_NONE) {
        return geode::Err(
            "couldn't find a proper pixel format to use for codec {}",
            m_settings.m_codec);
    }

    m_videoCodecCtx->codec_id = m_videoCodec->id;
    m_videoCodecCtx->pix_fmt = m_settings.m_pixFmt;
    m_videoCodecCtx->width = m_settings.m_width;
    m_videoCodecCtx->height = m_settings.m_height;
    m_videoCodecCtx->bit_rate = m_settings.m_bitrate;
    m_videoCodecCtx->time_base = {1, m_settings.m_fps};
    m_videoCodecCtx->framerate = {m_settings.m_fps, 1};
    m_videoCodecCtx->max_b_frames = 0;  // youtube doesn't like bframes at all

    const bool isRgb = m_settings.m_pixFmt == AV_PIX_FMT_RGB24 ||
                       m_settings.m_pixFmt == AV_PIX_FMT_RGB0;

    m_videoCodecCtx->color_primaries = AVCOL_PRI_BT709;
    m_videoCodecCtx->color_trc = AVCOL_TRC_BT709;
    m_videoCodecCtx->colorspace = isRgb ? AVCOL_SPC_RGB : AVCOL_SPC_BT709;
    m_videoCodecCtx->color_range = isRgb ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;

    if (m_formatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_videoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVDictionary* video_opts = nullptr;
    if (m_settings.m_codec == "hevc_nvenc") {
        geode::log::info("Detected hevc_nvenc codec - forcing b_ref_mode to 0");
        ff->av_dict_set(&video_opts, "b_ref_mode", "0", 0);
    }

    if (m_settings.m_codec == "hevc_qsv" || m_settings.m_codec == "h264_qsv") {
        geode::log::info("Detected qsv codec - forcing async_depth to 1");
        ff->av_dict_set(&video_opts, "async_depth", "1", 0);
    }

    for (const auto& arg : args) {
        geode::log::info("Applying args {}: `{}`", arg.m_name, arg.m_value);
        ff->av_dict_set(&video_opts, arg.m_name.c_str(), arg.m_value.c_str(),
                        0);
    }

    geode::log::info("Opening video codec `{}`...", m_settings.m_codec);
    if (ff->avcodec_open2(m_videoCodecCtx.get(), m_videoCodec, &video_opts) <
        0) {
        return geode::Err("Failed to open codec");
    }

    geode::log::info("Allocating audio context...");
    m_audioCodecCtx.reset(ff->avcodec_alloc_context3(m_audioCodec));
    if (!m_audioCodecCtx.get()) {
        return geode::Err("Failed to allocate audio codec context");
    }

    m_audioCodecCtx->codec_tag = 0;
    m_audioCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    m_audioCodecCtx->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    m_audioCodecCtx->bit_rate = 320000;
    m_audioCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;

    m_audioCodecCtx->sample_rate = m_sampleRate;
    m_audioCodecCtx->time_base = {1, m_sampleRate};

    m_audioCodecCtx->flags &= ~AV_CODEC_FLAG_QSCALE;
    if (m_formatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_audioCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVDictionary* opts = nullptr;
    ff->av_dict_set(&opts, "compression_level", "0", 0);

    geode::log::info("Opening audio codec `{}`...", m_settings.m_audioCodec);
    if (ff->avcodec_open2(m_audioCodecCtx.get(), m_audioCodec, &opts) < 0) {
        return geode::Err("Failed to open audio codec");
    }

    geode::log::info("Trying to free opts...");
    ff->av_dict_free(&opts);

    geode::log::info("Creating video stream...");
    m_videoStream = ff->avformat_new_stream(m_formatCtx, m_videoCodec);
    if (!m_videoStream) {
        return geode::Err("Failed to create stream");
    }

    if (ff->avcodec_parameters_from_context(m_videoStream->codecpar,
                                            m_videoCodecCtx.get()) < 0) {
        return geode::Err("Failed to copy codec parameters");
    }

    geode::log::info("Creating audio stream...");
    m_audioStream = ff->avformat_new_stream(m_formatCtx, m_audioCodec);
    if (!m_audioStream) {
        return geode::Err("Failed to create audio stream");
    }

    if (ff->avcodec_parameters_from_context(m_audioStream->codecpar,
                                            m_audioCodecCtx.get()) < 0) {
        return geode::Err("Failed to copy audio codec parameters");
    }

    m_audioStream->time_base = m_audioCodecCtx->time_base;

    if (ff->avio_open(&m_formatCtx->pb, outPath.c_str(), AVIO_FLAG_WRITE) < 0) {
        return geode::Err("Failed to open output file");
    }

    if (ff->avformat_write_header(m_formatCtx, nullptr) < 0) {
        return geode::Err("Failed to write header");
    }

    m_frame.reset(ff->av_frame_alloc());
    if (!m_frame) {
        return geode::Err("Failed to allocate frame");
    }

    m_frame->format = m_settings.m_pixFmt;
    m_frame->width = m_settings.m_width;
    m_frame->height = m_settings.m_height;

    m_audioFrame.reset(ff->av_frame_alloc());
    if (!m_audioFrame) {
        return geode::Err("Failed to allocate audio frame");
    }

    m_audioFrame->nb_samples = m_audioCodecCtx->frame_size;
    m_audioFrame->sample_rate = m_audioCodecCtx->sample_rate;
    m_audioFrame->format = m_audioCodecCtx->sample_fmt;
    m_audioFrame->ch_layout = m_audioCodecCtx->ch_layout;
    if (ff->av_frame_get_buffer(m_audioFrame.get(), 0) < 0) {
        return geode::Err("Failed to get audio frame buffer");
    }

    m_pkt.reset(ff->av_packet_alloc());
    if (!m_pkt) {
        return geode::Err("Failed to allocate packet");
    }

    m_audioPkt.reset(ff->av_packet_alloc());
    if (!m_audioPkt) {
        return geode::Err("Failed to allocate packet (audio)");
    }

    m_time = 0.0;
    m_endTime = 0.0;
    m_audioIndex = 0;
    m_updateIndex = 0;
    m_bufferPts = 0;
    m_ptsQueue.clear();
    m_frameQueue.clear();
    m_encoded = 0;

    m_recording = true;

    // int linesizes[AV_NUM_DATA_POINTERS] = {ALIGNMENT, ALIGNMENT};
    // m_alignedWidth = m_settings.m_width;
    // m_alignedHeight = m_settings.m_height;
    // avcodec_align_dimensions2(
    //     m_videoCodecCtx.get(), reinterpret_cast<int*>(&m_alignedWidth),
    //     reinterpret_cast<int*>(&m_alignedHeight), linesizes);

    CCSize renderSize = alignedRenderSize(m_settings);
    m_alignedWidth = renderSize.width;
    m_alignedHeight = renderSize.height;

    geode::log::info("Initializing capture with aligned size: {}x{}",
                     m_alignedWidth, m_alignedHeight);
    m_texture.m_alignedWidth = m_alignedWidth;
    m_texture.m_alignedHeight = m_alignedHeight;

    this->acquireView();
    resizeShaderLayer(renderSize);

    m_texture.m_windowWidth = m_windowSize.width;
    m_texture.m_windowHeight = m_windowSize.height;
    m_texture.init(std::move(colorspace));

    m_bufferSize = static_cast<size_t>(m_texture.m_colorspace->getBufferSize());
    geode::log::info("Using buffer size of {}", m_bufferSize);

    m_frameCount = 0;
    m_audioFrameCount = 0;
    m_audioBuffer.clear();

    AudioRecorder::get()->init();
    AudioRecorder::get()->attach(m_settings.m_musicVolume,
                                 m_settings.m_sfxVolume);

    m_readyToKill.store(false, std::memory_order_relaxed);
    m_recordThread = std::thread(&Renderer::recordLoop, this);

    return geode::Ok();
}

geode::Result<> Renderer::encode(uint8_t* data, size_t size) {
    ff->av_frame_unref(m_frame.get());

    m_frame->format = m_videoCodecCtx->pix_fmt;
    m_frame->width = m_videoCodecCtx->width;
    m_frame->height = m_videoCodecCtx->height;

    ff->av_image_fill_linesizes(m_frame->linesize, m_settings.m_pixFmt,
                                m_alignedWidth);

    auto result =
        m_texture.m_colorspace->prepareFrame(m_frame.get(), data, size);
    if (result.isErr()) {
        return result;
    }

    // m_frame->buf[0] = m_hwBuffer;
    // pts of the delivered frame, set by capture() when it dequeued the buffer
    // (the readback pipeline runs one frame behind m_updateIndex).
    m_frame->pts = m_bufferPts;

    int ret = ff->avcodec_send_frame(m_videoCodecCtx.get(), m_frame.get());
    if (ret < 0) {
        // format error
        if (ret == AVERROR(EAGAIN)) {
            return geode::Err("Codec requires more input data");
        } else if (ret == AVERROR_EOF) {
            return geode::Err("End of file reached");
        }

        return geode::Err("Failed to send frame: {}", ret);
    }

    return geode::Ok();
}

geode::Result<> Renderer::write() {
    int ret = 0;
    while (ret >= 0) {
        ret = ff->avcodec_receive_packet(m_videoCodecCtx.get(), m_pkt.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            return geode::Err("Failed to receive packet");
        }

        ff->av_packet_rescale_ts(m_pkt.get(), m_videoCodecCtx->time_base,
                                 m_videoStream->time_base);
        m_pkt->stream_index = m_videoStream->index;

        {
            std::lock_guard<std::mutex> lock(m_muxMutex);
            ret = ff->av_interleaved_write_frame(m_formatCtx, m_pkt.get());
        }
        if (ret < 0) {
            return geode::Err("Failed to write frame");
        }

        ff->av_packet_unref(m_pkt.get());
    }

    // av_buffer_unref(&m_hwBuffer);

    return geode::Ok();
}

geode::Result<> Renderer::writeAudio(std::vector<float>& data, uint64_t pts) {
    m_audioFrame->pts = pts;
    m_audioFrame->nb_samples = m_audioCodecCtx->frame_size;
    m_audioFrame->ch_layout = m_audioCodecCtx->ch_layout;

    int ret = 0;

    AVChannelLayout inputChLayout;
    ff->av_channel_layout_default(&inputChLayout, m_channels);

    if (!m_swrCtx) {
        ret = ff->swr_alloc_set_opts2(
            &m_swrCtx, &m_audioCodecCtx->ch_layout, m_audioCodecCtx->sample_fmt,
            m_audioCodecCtx->sample_rate, &inputChLayout, AV_SAMPLE_FMT_FLT,
            m_sampleRate, 0, nullptr);
        if (ret < 0) {
            return geode::Err("Failed to set swr options: {}", ret);
        }

        if (!m_swrCtx) {
            return geode::Err("Failed to allocate swr context");
        }

        if (ff->swr_init(m_swrCtx) < 0) {
            return geode::Err("Failed to initialize swr context");
        }
    }

    const uint8_t* inData[1] = {reinterpret_cast<const uint8_t*>(data.data())};
    ff->swr_convert(m_swrCtx, m_audioFrame->data, m_audioCodecCtx->frame_size,
                    inData, m_audioCodecCtx->frame_size);

    ret = ff->avcodec_send_frame(m_audioCodecCtx.get(), m_audioFrame.get());
    if (ret < 0) {
        return geode::Err("Failed to send frame {}", ret);
    }

    // audioPkt is used here because it's encoding both audio and video
    // at the same time
    while (ret >= 0) {
        ret =
            ff->avcodec_receive_packet(m_audioCodecCtx.get(), m_audioPkt.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            return geode::Err("Failed to receive packet");
        }

        ff->av_packet_rescale_ts(m_audioPkt.get(), m_audioCodecCtx->time_base,
                                 m_audioStream->time_base);
        m_audioPkt->stream_index = m_audioStream->index;

        {
            std::lock_guard<std::mutex> lock(m_muxMutex);
            ret = ff->av_interleaved_write_frame(m_formatCtx, m_audioPkt.get());
        }
        if (ret < 0) {
            return geode::Err("Failed to write frame");
        }

        ff->av_packet_unref(m_audioPkt.get());
    }

    return geode::Ok();
}

geode::Result<> Renderer::stop() {
    m_recording = false;

    geode::log::info("Detaching audio and stopping all streams...");
    AudioRecorder::get()->detach();
    AudioRecorder::get()->uninit();

    std::string error;

    auto flush = [&](AVCodecContext* codecCtx, AVPacket* pkt,
                     AVStream* stream) {
        if (!m_formatCtx || !codecCtx || !pkt || !stream) {
            return;
        }

        ff->avcodec_send_frame(codecCtx, nullptr);
        while (ff->avcodec_receive_packet(codecCtx, pkt) == 0) {
            ff->av_packet_rescale_ts(pkt, codecCtx->time_base,
                                     stream->time_base);
            pkt->stream_index = stream->index;

            int ret;
            {
                std::lock_guard<std::mutex> lock(m_muxMutex);
                ret = ff->av_interleaved_write_frame(m_formatCtx, pkt);
            }

            ff->av_packet_unref(pkt);

            if (ret < 0) {
                if (error.empty()) {
                    error = fmt::format("Failed to write frame: {}", ret);
                }
                break;
            }
        }
    };

    flush(m_videoCodecCtx.get(), m_pkt.get(), m_videoStream);
    flush(m_audioCodecCtx.get(), m_audioPkt.get(), m_audioStream);

    if (m_formatCtx) {
        int ret;
        {
            std::lock_guard<std::mutex> lock(m_muxMutex);
            ret = ff->av_write_trailer(m_formatCtx);
        }
        if (ret < 0 && error.empty()) {
            error = fmt::format("Failed to write trailer: {}", ret);
        }
    }

    geode::log::info("Rendering has stopped. Cleaning up");

    if (m_frame) {
        m_frame->data[0] = nullptr;
        m_frame->data[1] = nullptr;
    }

    if (m_formatCtx) {
        if (m_formatCtx->pb) {
            ff->avio_close(m_formatCtx->pb);
        }

        // streams are owned by the format context, so this frees them too
        ff->avformat_free_context(m_formatCtx);
        m_formatCtx = nullptr;
    }

    m_videoStream = nullptr;
    m_audioStream = nullptr;

    ff->swr_free(&m_swrCtx);

    m_videoCodecCtx.reset();
    m_audioCodecCtx.reset();

    m_texture.destroy();
    this->restoreView();

    if (m_autoExitLevel->inner()) {
        m_shouldQuit = true;
    }

    geode::log::info("Renderer cleanup done");

    if (!error.empty()) {
        return geode::Err(error);
    }

    return geode::Ok();
}

void Renderer::recordLoop() {
    geode::log::info("Started encode thread");

    while (true) {
        QueuedFrame frame;
        {
            std::unique_lock<std::mutex> lock(m_recordMutex);
            m_recordCv.wait(
                lock, [this] { return !m_frameQueue.empty() || !m_recording; });

            if (m_frameQueue.empty()) {
                // nothing queued and recording is done: drain complete
                if (!m_recording) {
                    break;
                }
                continue;
            }

            frame = m_frameQueue.front();
            m_frameQueue.pop_front();
        }

        m_bufferPts = frame.m_pts;

        {
            auto ret = this->encode(frame.m_data, m_bufferSize);
            if (ret.isErr()) {
                geode::log::error("Failed to encode frame: {}",
                                  ret.unwrapErr());
                break;
            }
        }

        {
            auto ret = this->write();
            if (ret.isErr()) {
                geode::log::error("Failed to write frame: {}", ret.unwrapErr());
                break;
            }
        }

        m_encoded.fetch_add(1, std::memory_order_release);
    }

    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        m_recording = false;
    }

    m_readyToKill.store(true, std::memory_order_release);
    geode::log::info("Encode thread killed");
}

void Renderer::finishStop() {
    if (!m_readyToKill.exchange(false, std::memory_order_acquire)) {
        return;
    }

    if (m_recordThread.joinable()) {
        m_recordThread.join();
    }

    auto stopResult = this->stop();
    if (stopResult.isErr()) {
        geode::log::error("Failed to stop recording: {}",
                          stopResult.unwrapErr());
    }
}

void Renderer::enqueueFrame(uint8_t* data, int64_t pts) {
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        m_frameQueue.push_back(QueuedFrame{.m_data = data, .m_pts = pts});
    }
    m_recordCv.notify_one();
}

void Renderer::harvestCompleted() {
    uint8_t* data = nullptr;
    while (!m_ptsQueue.empty() &&
           m_texture.tryHarvest(&data, /* block */ false)) {
        int64_t pts = m_ptsQueue.front();
        m_ptsQueue.pop_front();
        this->enqueueFrame(data, pts);
    }
}

void Renderer::flushPending() {
    uint8_t* data = nullptr;
    while (!m_ptsQueue.empty() &&
           m_texture.mappedCount() < m_texture.issuedCount()) {
        if (!m_texture.tryHarvest(&data, /* block */ true)) {
            break;
        }
        int64_t pts = m_ptsQueue.front();
        m_ptsQueue.pop_front();
        this->enqueueFrame(data, pts);
    }
}

bool Renderer::tryBeginFrame() {
    this->harvestCompleted();

    return m_texture.issuedCount() - m_encoded.load(std::memory_order_acquire) <
           RenderTexture::RING_SIZE;
}

void Renderer::capture() {
    m_ptsQueue.push_back(m_updateIndex - 1);
    m_texture.issue(m_fadeThreshold);
}

void Renderer::update(PlayLayer* pl) {
    if (!this->isRecording()) return;

    bool started =
        pl->m_started || (m_settings.m_firstAttemptPause && m_seenFrames >= 2);
    m_seenFrames++;  // GD adds two "fade" frames
    if (pl->m_isPaused || !started) return;

    m_time = ++m_updateIndex * this->getDt();

    bool moreLevel =
        !Bot::get()->replaySystem().getNextQueuedInput().has_value();

    // fade in

    if (pl->m_hasCompletedLevel && moreLevel) {
        if (this->m_endTime >= this->m_settings.m_afterEndTime) {
            this->flushPending();
            this->signalStop();
            return;
        } else {
            this->m_endTime += this->getDt();
        }
    }

    double fadeOut = 1.0;
    if (pl->m_hasCompletedLevel && moreLevel) {
        fadeOut =
            m_settings.m_fadeOutTime == 0
                ? 1
                : std::clamp(
                      ((this->m_settings.m_afterEndTime - this->m_endTime) /
                       m_settings.m_fadeOutTime),
                      0.0, 1.0);
    }

    double fadeIn =
        m_settings.m_fadeInTime == 0
            ? 1
            : std::clamp(m_time / m_settings.m_fadeInTime, 0.0, 1.0);

    m_fadeThreshold = std::min(fadeIn, fadeOut);

    if (m_collectAudio) {
        AudioRecorder::get()->unpause();
    }

    this->capture();
}
