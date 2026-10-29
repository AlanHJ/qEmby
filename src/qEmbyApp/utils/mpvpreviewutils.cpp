#include "mpvpreviewutils.h"

#include <QElapsedTimer>
#include <QByteArray>
#include <QMetaType>
#include <QStringList>
#include <QThread>
#include <mpv/client.h>
#include <mpv/render.h>
#include <vector>

namespace MpvPreviewUtils {
namespace {
void renderUpdated(void *context)
{
    static_cast<std::atomic_bool *>(context)->store(true);
}

int setOption(mpv_handle *mpv, const QString &name, const QVariant &value)
{
    const QByteArray key = name.toUtf8();
    if (value.metaType().id() == QMetaType::Bool) {
        int flag = value.toBool();
        return mpv_set_option(mpv, key.constData(), MPV_FORMAT_FLAG, &flag);
    }
    if (value.metaType().id() == QMetaType::QStringList ||
        value.metaType().id() == QMetaType::QVariantList) {
        const QStringList strings = value.toStringList();
        std::vector<QByteArray> bytes;
        std::vector<mpv_node> nodes(strings.size());
        bytes.reserve(strings.size());
        for (const QString &string : strings) bytes.push_back(string.toUtf8());
        for (size_t i = 0; i < bytes.size(); ++i) {
            nodes[i].format = MPV_FORMAT_STRING;
            nodes[i].u.string = bytes[i].data();
        }
        mpv_node_list list{};
        list.num = static_cast<int>(nodes.size());
        list.values = nodes.data();
        mpv_node node{};
        node.format = MPV_FORMAT_NODE_ARRAY;
        node.u.list = &list;
        return mpv_set_option(mpv, key.constData(), MPV_FORMAT_NODE, &node);
    }
    const QByteArray text = value.toString().toUtf8();
    return mpv_set_option_string(mpv, key.constData(), text.constData());
}
}

struct Session::State
{
    
    
    std::unique_ptr<mpv_handle, decltype(&mpv_terminate_destroy)>
        mpv{nullptr, &mpv_terminate_destroy};
    std::atomic_bool needsUpdate{true};
    std::unique_ptr<mpv_render_context, decltype(&mpv_render_context_free)>
        render{nullptr, &mpv_render_context_free};
    QString url;
    QVariantMap networkOptions;
    bool ready = false;
    uint64_t serial = 0;
};

Session::Session() = default;
Session::~Session() = default;

void Session::reset()
{
    m_state.reset();
}

Result Session::decode(Request request)
{
    if (!request.cancelled || request.cancelled->load() || request.url.isEmpty()) return {};
    const bool reuse = m_state && m_state->ready && m_state->url == request.url &&
                       m_state->networkOptions == request.networkOptions;
    if (!reuse) {
        reset();
        m_state = std::make_unique<State>();
    }
    State &state = *m_state;
    state.ready = false;
    auto &mpv = state.mpv;
    auto &render = state.render;
    auto &needsUpdate = state.needsUpdate;
    int error = 0;

    if (!reuse) {
        mpv.reset(mpv_create());
        if (!mpv) return {{}, MPV_ERROR_NOMEM, QStringLiteral("create")};

        const QVariantMap options{
            {"config", "no"}, {"load-scripts", "no"}, {"terminal", "no"},
            {"vo", "libmpv"}, {"ao", "null"}, {"audio", "no"},
            {"sid", "no"}, {"secondary-sid", "no"}, {"hwdec", "no"},
            {"pause", "yes"}, {"keep-open", "yes"}, {"idle", "yes"},
            {"osc", "no"}, {"osd-level", "0"}, {"audio-display", "no"},
            {"cache", "no"}, {"demuxer-readahead-secs", "0"},
            {"demuxer-seekable-cache", "yes"},
            {"demuxer-max-bytes", "16MiB"}, {"demuxer-max-back-bytes", "16MiB"},
            {"network-timeout", "8"},
            {"vd-lavc-threads", "1"}, {"hr-seek", "yes"},
            {"start", QString::number(request.seconds, 'f', 3)}
        };
        for (auto it = options.cbegin(); it != options.cend(); ++it) {
            const int error = setOption(mpv.get(), it.key(), it.value());
            if (error < 0) return {{}, error, QStringLiteral("option:") + it.key()};
        }
        
        
        for (auto it = request.networkOptions.cbegin(); it != request.networkOptions.cend(); ++it) {
            const int error = setOption(mpv.get(), it.key(), it.value());
            if (error < 0) return {{}, error, QStringLiteral("network-option:") + it.key()};
        }
        error = mpv_initialize(mpv.get());
        if (error < 0) return {{}, error, QStringLiteral("initialize")};
        if (request.cancelled->load()) return {};

        char api[] = MPV_RENDER_API_TYPE_SW;
        mpv_render_param createParams[] = {
            {MPV_RENDER_PARAM_API_TYPE, api}, {MPV_RENDER_PARAM_INVALID, nullptr}
        };
        mpv_render_context *rawContext = nullptr;
        error = mpv_render_context_create(&rawContext, mpv.get(), createParams);
        if (error < 0) return {{}, error, QStringLiteral("render-context")};
        render.reset(rawContext);
        mpv_render_context_set_update_callback(render.get(), renderUpdated, &needsUpdate);
        if (request.cancelled->load()) return {};

        state.url = request.url;
        state.networkOptions = request.networkOptions;
    }

    QImage image(320, 180, QImage::Format_RGBX8888);
    if (image.isNull()) return {{}, MPV_ERROR_NOMEM, QStringLiteral("image-buffer")};
    int size[] = {image.width(), image.height()};
    char format[] = "rgb0";
    size_t stride = static_cast<size_t>(image.bytesPerLine());
    int block = 0;
    mpv_render_param renderParams[] = {
        {MPV_RENDER_PARAM_SW_SIZE, size},
        {MPV_RENDER_PARAM_SW_FORMAT, format},
        {MPV_RENDER_PARAM_SW_STRIDE, &stride},
        {MPV_RENDER_PARAM_SW_POINTER, image.bits()},
        {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    if (reuse) {
        
        
        while (mpv_wait_event(mpv.get(), 0)->event_id != MPV_EVENT_NONE) {}
        needsUpdate.exchange(false);
        if (mpv_render_context_update(render.get()) & MPV_RENDER_UPDATE_FRAME) {
            error = mpv_render_context_render(render.get(), renderParams);
            if (error < 0) return {{}, error, QStringLiteral("drain-render"), reuse};
        }
    }
    if (request.cancelled->load()) return {};
    const uint64_t serial = ++state.serial;
    if (reuse) {
        const QByteArray seconds = QByteArray::number(request.seconds, 'f', 3);
        const char *seek[] = {"seek", seconds.constData(), "absolute+exact", nullptr};
        error = mpv_command_async(mpv.get(), serial, seek);
    } else {
        const QByteArray url = request.url.toUtf8();
        const char *load[] = {"loadfile", url.constData(), nullptr};
        error = mpv_command_async(mpv.get(), serial, load);
    }
    if (error < 0) return {{}, error, QStringLiteral("command"), reuse};

    QElapsedTimer deadline;
    deadline.start();
    bool loaded = reuse;
    bool seekStarted = !reuse;
    bool commandCompleted = false;
    bool playbackRestarted = false;
    bool frameReady = false;
    while (!request.cancelled->load() && deadline.elapsed() < 12000) {
        while (mpv_event *event = mpv_wait_event(mpv.get(), 0)) {
            if (event->event_id == MPV_EVENT_NONE) break;
            if (event->event_id == MPV_EVENT_FILE_LOADED) loaded = true;
            if (event->event_id == MPV_EVENT_COMMAND_REPLY && event->reply_userdata == serial) {
                if (event->error < 0) return {{}, event->error, QStringLiteral("command-reply"), reuse};
                commandCompleted = true;
            }
            if (event->event_id == MPV_EVENT_SEEK) {
                seekStarted = true;
                playbackRestarted = false;
                frameReady = false;
            }
            if (event->event_id == MPV_EVENT_PLAYBACK_RESTART && seekStarted)
                playbackRestarted = true;
            if (event->event_id == MPV_EVENT_END_FILE) {
                const auto *end = static_cast<mpv_event_end_file *>(event->data);
                return {{}, end && end->error < 0 ? end->error : MPV_ERROR_NOTHING_TO_PLAY,
                        QStringLiteral("end-file"), reuse};
            }
            if (event->event_id == MPV_EVENT_SHUTDOWN)
                return {{}, MPV_ERROR_NOTHING_TO_PLAY, QStringLiteral("shutdown"), reuse};
        }
        if (needsUpdate.exchange(false)) {
            const uint64_t flags = mpv_render_context_update(render.get());
            if (flags & MPV_RENDER_UPDATE_FRAME) {
                mpv_render_frame_info info{};
                error = mpv_render_context_get_info(render.get(), {MPV_RENDER_PARAM_NEXT_FRAME_INFO, &info});
                if (error < 0) return {{}, error, QStringLiteral("frame-info"), reuse};
                error = mpv_render_context_render(render.get(), renderParams);
                if (error < 0) return {{}, error, QStringLiteral("render"), reuse};
                
                
                if (seekStarted && (info.flags & MPV_RENDER_FRAME_INFO_PRESENT) &&
                    !(info.flags & (MPV_RENDER_FRAME_INFO_REDRAW | MPV_RENDER_FRAME_INFO_REPEAT))) {
                    frameReady = true;
                }
            }
        }
        if (loaded && commandCompleted && playbackRestarted && frameReady) {
            
            for (int y = 0; y < image.height(); ++y) {
                uchar *row = image.scanLine(y);
                for (int x = 0; x < image.width(); ++x) row[x * 4 + 3] = 255;
            }
            state.ready = true;
            return {image, 0, QStringLiteral("frame"), reuse};
        }
        QThread::msleep(5);
    }
    if (request.cancelled->load()) return {};
    return {{}, MPV_ERROR_GENERIC, QStringLiteral("timeout"), reuse};
}
}
