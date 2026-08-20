#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "../settings/settings.hpp"
#include "../shared/value/value.hpp"
#include "ffmpeg.hpp"
#include "texture.hpp"
// #include "render_texture.hpp"
// #include "dsp.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <glaze/glaze.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "dsp.hpp"

struct RendererSettings {
    int m_width = 3840;
    int m_height = 2160;
    uint32_t m_bitrate = 68'000'000;
    std::string m_codec = "";
    AVPixelFormat m_pixFmt = AV_PIX_FMT_NV12;

    int m_fps = 60;

    float m_afterEndTime = 5.0f;  // in seconds
    bool m_colorFix = true;

    std::string m_outputPath = "output";
    std::string m_extension = "mp4";
    std::string m_audioCodec = "aac";

    std::string m_renderArgs = "";

    double m_musicVolume = 1.0;
    double m_sfxVolume = 1.0;

    bool m_firstAttemptPause = false;
    double m_fadeInTime = 1.5;
    double m_fadeOutTime = 1.5;
    int m_visualFps = 60;
};

template <>
struct glz::meta<RendererSettings> {
    using T = RendererSettings;
    static constexpr auto value = glz::object(
        "width", &T::m_width, "height", &T::m_height, "bitrate", &T::m_bitrate,
        "codec", &T::m_codec, "pix_fmt", &T::m_pixFmt, "fps", &T::m_fps,
        "after_end_time", &T::m_afterEndTime, "color_fix", &T::m_colorFix,
        "render_args", &T::m_renderArgs, "output_path", hide{&T::m_outputPath},
        "music_volume", &T::m_musicVolume, "sfx_volume", &T::m_sfxVolume,
        "fade_in", &T::m_fadeInTime, "fade_out", &T::m_fadeOutTime,
        "visual_fps", &T::m_visualFps, "record_paused",
        &T::m_firstAttemptPause);
};

#define SL_AV_PTR(type) std::unique_ptr<type, std::function<void(type*)>>

#define SL_AV_LEAK(type) [](type*) {}

#define SL_AV_DELETER(type, deleteFunc) \
    [&](type* ptr) {                    \
        if (ptr) {                      \
            deleteFunc(&ptr);           \
        }                               \
    }

#define SL_AV_DELETER2(type, deleteFunc) \
    [&](type* ptr) {                     \
        if (ptr) {                       \
            deleteFunc(ptr);             \
        }                                \
    }

class Renderer {
   public:
    ~Renderer() {
        // don't let the thread get charlie kirked and cause a crash
        if (m_recordThread.joinable()) {
            m_recordThread.detach();
        }
    }

    void queueStart() { m_shouldStart = true; }
    void startIfQueued() {
        if (m_shouldStart) {
            m_shouldStart = false;
            auto ret = start();
            if (ret.isErr()) {
                geode::log::error("Failed to start recording: {}",
                                  ret.unwrapErr());
                // PlayLayer::init took the view for a render that never
                // happened
                this->restoreView();
            }
        }
    }
    geode::Result<> start();
    geode::Result<> encode(uint8_t* data, size_t size);
    geode::Result<> write();
    // geode::Result<float> writeAudio(std::vector<float>& data);
    geode::Result<> writeAudio(std::vector<float>& data, uint64_t pts);
    geode::Result<> stop();

    void acquireView();
    void reacquireView();
    void restoreView();

    void signalStop() {
        {
            std::lock_guard<std::mutex> lock(m_recordMutex);
            m_recording = false;
        }
        m_recordCv.notify_one();
    }

    void recordLoop();

    void finishStop();

    bool tryBeginFrame();

    void capture();
    void update(PlayLayer* pl);

    void displayPreview() { m_texture.displayPreview(); }

    RendererSettings m_settings;

    static Renderer* get() {
        static Renderer instance;

        return &instance;
    }

    float getDt() const { return 1. / m_settings.m_fps; }
    bool isRecording() const { return m_recording; }
    float getTime() const { return m_time; }
    inline bool isFFmpegLoaded() const { return m_ffmpegLoaded; }

    void loadSettings(std::filesystem::path& path);
    void saveSettings(std::filesystem::path& path) const;
    void initializeDefaults();
    void loadFFmpeg() {
        if (ff) {
            free(ff);
        }

        ff = (ff_t*)malloc(sizeof(ff_t));
        if (!ff) {
            return;
        }

        m_ffmpegLoaded = loadFFmpegFunctions(ff);
    }

    bool m_shouldStart = false;
    bool m_collectAudio = true;
    bool m_shouldQuit = false;

    float m_fadeThreshold = 0.0;

    SLValuePtr<bool> m_autoVideoName = SLValue<bool>::create(
        "renderer.auto_video_name", &SLSettings::get()->automaticVideoName);
    SLValuePtr<bool> m_autoExitLevel = SLValue<bool>::create(
        "renderer.auto_exit_level", &SLSettings::get()->autoExitLevel);
    SLValuePtr<std::string> m_videoNameTemplate = SLValue<std::string>::create(
        "renderer.video_name_template", &SLSettings::get()->videoNameTemplate);

    double m_time = 0;
    RenderTexture m_texture;
    ff_t* ff = 0;

   private:
    struct QueuedFrame {
        uint8_t* m_data = nullptr;
        int64_t m_pts = 0;
    };

    void harvestCompleted();
    void flushPending();
    void enqueueFrame(uint8_t* data, int64_t pts);

    // AVBufferRef* m_hwDeviceCtx = nullptr;
    // AVBufferRef* m_hwFramesCtx = nullptr;
    // const AVCodec* m_codec = nullptr;
    // const AVCodec* m_audioCodec = nullptr;
    // AVFormatContext* m_formatCtx = nullptr;
    // AVCodecContext* m_videoCodecCtx = nullptr;
    // AVStream* m_videoStream = nullptr;
    // AVStream* m_audioStream = nullptr;
    // AVFrame* m_frame = nullptr;
    // AVFrame* m_dstFrame = nullptr;
    // AVFrame* m_filterFrame = nullptr;
    // AVFrame* m_audioFrame = nullptr;
    // AVPacket* m_pkt = nullptr;
    // AVPacket* m_audioPkt = nullptr;
    // SwsContext* m_swsCtx = nullptr;
    // AVFilterGraph* m_filterGraph = nullptr;
    // AVFilterContext* m_buffersinkCtx = nullptr;
    // AVFilterContext* m_buffersrcCtx = nullptr;
    // AVFilterContext* m_colorspaceCtx = nullptr;
    // AVBufferRef* m_frameBuffer = nullptr;

    SL_AV_PTR(AVCodecContext)
    m_videoCodecCtx = {nullptr,
                       SL_AV_DELETER(AVCodecContext, ff->avcodec_free_context)};
    SL_AV_PTR(AVCodecContext)
    m_audioCodecCtx = {nullptr,
                       SL_AV_DELETER(AVCodecContext, ff->avcodec_free_context)};
    // SL_AV_PTR(AVFormatContext) m_formatCtx = {nullptr,
    // SL_AV_DELETER2(AVFormatContext, avformat_free_context)};
    AVFormatContext* m_formatCtx = nullptr;  // managed by avformat_free_context

    AVStream* m_videoStream = nullptr;
    AVStream* m_audioStream = nullptr;
    AVBufferRef* m_hwBuffer = nullptr;
    SwrContext* m_swrCtx = nullptr;

    const AVCodec* m_videoCodec = nullptr;
    const AVCodec* m_audioCodec = nullptr;

    SL_AV_PTR(AVFrame)
    m_frame = {nullptr, SL_AV_DELETER(AVFrame, ff->av_frame_free)};
    SL_AV_PTR(AVFrame)
    m_audioFrame = {nullptr, SL_AV_DELETER(AVFrame, ff->av_frame_free)};

    SL_AV_PTR(AVPacket)
    m_pkt = {nullptr, SL_AV_DELETER(AVPacket, ff->av_packet_free)};
    SL_AV_PTR(AVPacket)
    m_audioPkt = {nullptr, SL_AV_DELETER(AVPacket, ff->av_packet_free)};

    float m_visualFps = 60.0f;
    std::atomic<bool> m_recording = false;

    std::thread m_recordThread;
    std::atomic<bool> m_readyToKill = false;

    std::mutex m_recordMutex;
    std::condition_variable m_recordCv;

    std::mutex m_muxMutex;

    int m_frameCount = 0;
    int m_audioFrameCount = 0;
    int m_audioIndex = 0;

    int m_updateIndex = 0;
    int m_seenFrames = 0;
    float m_endTime = 0;

    int m_channels;
    int m_sampleRate;

    size_t m_bufferSize = 0;

    std::deque<QueuedFrame> m_frameQueue;
    std::atomic<int64_t> m_encoded = 0;

    // updateindex cant be reliable pts no more cuz pts is slightly late
    int64_t m_bufferPts = 0;
    std::deque<int64_t> m_ptsQueue;

    int m_alignedWidth = 0;
    int m_alignedHeight = 0;

    cocos2d::CCSize m_windowSize;
    bool m_viewResized = false;

    bool m_ffmpegLoaded = false;

    std::vector<float> m_audioBuffer;

    std::mutex m_lock;

    friend class AudioRecorder;
};

#endif
