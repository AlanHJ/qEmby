#include "mpvcontroller.h"
#include "anime4kshadermanager.h"
#include "../utils/subtitlestyleutils.h"
#include "../utils/playerpreferenceutils.h"
#include <QDebug>
#include <QFile>
#include <QMetaType>
#include <QElapsedTimer>
#include <QThreadPool>
#include <clocale>
#include <config/configstore.h>
#include <config/config_keys.h>
#include <QStandardPaths>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <QThread>
#include <windows.h>
#endif


QAtomicInt MpvController::s_warmedUp(0);


struct node_autofree {
    mpv_node *ptr;
    explicit node_autofree(mpv_node *a_ptr) : ptr(a_ptr) {}
    ~node_autofree() { mpv_free_node_contents(ptr); }
};

MpvController::MpvController(QObject *parent)
    : QObject(parent), m_mpv(nullptr) {
}

MpvController::~MpvController() {
    forceCleanup();
}

void MpvController::forceCleanup() {
    m_propertyRequests.clear();
    m_latestPropertyRequests.clear();
    if (m_mpv) {
        
        this->blockSignals(true);

        mpv_command_string(m_mpv, "stop");
        
        mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
        
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

void MpvController::warmupOnce() {
    
    
    if (!s_warmedUp.testAndSetOrdered(0, 1)) {
        qDebug() << "[MpvController::warmupOnce] already warmed up, skip";
        return;
    }

    qDebug() << "[MpvController::warmupOnce] dispatch mpv warmup to worker thread"
             << "(preload libmpv DLLs + FFmpeg codec tables to avoid first-play UI freeze)";

    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    QThreadPool::globalInstance()->start([]() {
        QElapsedTimer timer;
        timer.start();

        {
            MpvController warmup;
            const bool ok = warmup.init();
            qDebug() << "[MpvController::warmupOnce] mpv_initialize completed"
                     << "| ok=" << ok
                     << "| elapsed=" << timer.elapsed() << "ms";
        }
        

        qDebug() << "[MpvController::warmupOnce] total elapsed" << timer.elapsed() << "ms"
                 << "(libmpv DLLs + codec tables now resident in process memory;"
                 << "next mpv_initialize will run on hot path)";
    });
}

bool MpvController::init() {
    
    std::setlocale(LC_NUMERIC, "C");

    m_mpv = mpv_create();
    if (!m_mpv) {
        emit errorOccurred(tr("Failed to create MPV instance."));
        return false;
    }

    
    
    
    
    mpv_set_option_string(m_mpv, "osc", "no");
    
    mpv_set_option_string(m_mpv, "load-scripts", "no");
    
    mpv_set_option_string(m_mpv, "config", "no");

    
    
    mpv_set_option_string(m_mpv, "vo", "libmpv");
    
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    
    
    
    
    mpv_set_option_string(m_mpv, "blend-subtitles", "video");
    mpv_set_option_string(m_mpv, "sub-gray", "yes");
    mpv_set_option_string(m_mpv, "sub-blur", "0");
    mpv_set_option_string(m_mpv, "sub-gauss", "0");
    
    
    mpv_set_option_string(m_mpv, "sub-hinting", "none");
    mpv_set_option_string(m_mpv, "sub-ass-hinting", "none");
    mpv_request_log_messages(m_mpv, "info");

    
    
    
#if defined(QEMBY_MPV_D3D11_RENDER)
    // D3D11 渲染通道: display-resample 以 vsync 反馈为节拍, 会把渲染 API 的
    // 重绘循环拖到视频帧率 (24fps 视频 => 全 UI 24Hz 顿挫), 强制回落音频主
    // 时钟; 弹幕/UI 动画由 MpvWidget 的独立高刷定时器驱动。
    QString vsyncMode = ConfigStore::instance()->get<QString>(
        ConfigKeys::PlayerVideoSync, "audio");
    if (vsyncMode == QLatin1String("display-resample")) {
        qWarning() << "[MpvController] video-sync=display-resample downgraded to audio"
                   << "| reason=D3D11 render loop decoupling";
        vsyncMode = QStringLiteral("audio");
    }
    // 渲染 API 的内部计时等待 (BLOCK_FOR_TARGET_TIME 默认开启) 会阻塞渲染
    // 调用方直至帧目标显示时间; 除在渲染调用中禁用外, 按 render.h 的建议把
    // video-timing-offset 归零 —— mpv 在目标时间才投递帧, 调用方即时呈现,
    // 音画同步保持且 GUI 线程零阻塞。
    mpv_set_option_string(m_mpv, "video-timing-offset", "0");
#else
    QString vsyncMode = ConfigStore::instance()->get<QString>(
        ConfigKeys::PlayerVideoSync, "display-resample");
#endif
    mpv_set_option_string(m_mpv, "video-sync", vsyncMode.toUtf8().constData());
    
    mpv_set_option_string(m_mpv, "interpolation", vsyncMode == "display-resample" ? "yes" : "no");
    qDebug() << "[MpvController] Applied video sync mode"
             << "| video-sync=" << vsyncMode
             << "| interpolation=" << (vsyncMode == "display-resample");

    
    
    
    // 默认硬解策略按平台零拷贝通道下发 (用户配置优先, 未配置时生效):
    //  - Windows + D3D11 渲染通道 (QEMBY_MPV_D3D11_RENDER): d3d11va 原生零拷贝
    //    (与 Qt RHI 共享 ID3D11Device, 无 PCIe 回传); d3d11va-copy 保留为容灾;
    //  - Windows GL 回退构建 (Qt < 6.7): 维持 auto-copy (GL 无法互操作 D3D11 帧);
    //  - Linux: vaapi (Intel/AMD, EGL dma-buf 零拷贝) -> nvdec (NVIDIA CUDA-GL)
    //    -> auto-safe, 全部为无 -copy 后缀的直通解码;
    //  - 其余平台: 维持 auto-copy。
#if defined(QEMBY_MPV_D3D11_RENDER)
    const QString hwdecDefault = QStringLiteral("d3d11va");
#elif defined(Q_OS_WIN)
    const QString hwdecDefault = QStringLiteral("auto-copy");
#elif defined(Q_OS_LINUX)
    const QString hwdecDefault = QStringLiteral("vaapi,nvdec,auto-safe");
#else
    const QString hwdecDefault = QStringLiteral("auto-copy");
#endif
    QString hwdec = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerHwDec, hwdecDefault);

#ifdef Q_OS_WIN
    bool isRDP = GetSystemMetrics(SM_REMOTESESSION) != 0;
    if (isRDP) {

        hwdec = "no";
        mpv_set_option_string(m_mpv, "profile", "sw-fast");
        m_gpuDumbMode = true;
    }
#endif
    mpv_set_option_string(m_mpv, "hwdec", hwdec.toUtf8().constData());
    qDebug() << "[MpvController] Hardware decoding policy"
             << "| hwdec=" << hwdec
             << "| source=" << (hwdec == hwdecDefault ? "default" : "config");

    
    
    
    const QString audioRules =
        ConfigStore::instance()->get<QString>(ConfigKeys::PlayerAudioLang, "auto");
    const QStringList audioLangCodes =
        PlayerPreferenceUtils::mpvLanguageCodesForRules(audioRules);
    if (!audioLangCodes.isEmpty()) {
        const QByteArray alangValue = audioLangCodes.join(',').toUtf8();
        mpv_set_option_string(m_mpv, "alang", alangValue.constData());
        qDebug() << "[MpvController] Applied audio language rules to MPV"
                 << "| rules=" << audioRules << "| alang=" << alangValue;
    }

    const QString subtitleRules =
        ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSubLang, "auto");
    if (PlayerPreferenceUtils::isSubtitleDisabled(subtitleRules)) {
        mpv_set_option_string(m_mpv, "sid", "no");
        qDebug() << "[MpvController] Subtitle rules disable subtitles"
                 << "| rules=" << subtitleRules;
    } else {
        const QStringList subtitleLangCodes =
            PlayerPreferenceUtils::mpvLanguageCodesForRules(subtitleRules);
        if (!subtitleLangCodes.isEmpty()) {
            const QByteArray slangValue = subtitleLangCodes.join(',').toUtf8();
            mpv_set_option_string(m_mpv, "slang", slangValue.constData());
            qDebug() << "[MpvController] Applied subtitle language rules to MPV"
                     << "| rules=" << subtitleRules << "| slang=" << slangValue;
        }
    }

    
    
    
    bool volNormal = ConfigStore::instance()->get<bool>(ConfigKeys::PlayerVolNormal, false);
    if (volNormal) {
        mpv_set_option_string(m_mpv, "af", "dynaudnorm=g=5:f=250:r=0.9:p=0.5");
    }

    
    
    
    bool useMpvConf = ConfigStore::instance()->get<bool>(ConfigKeys::PlayerUseMpvConf, false);
    if (useMpvConf) {
        QString mpvConfDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                                 .replace(QRegularExpression("[^/]+$"), "mpv");
        mpv_set_option_string(m_mpv, "config-dir", mpvConfDir.toUtf8().constData());
        mpv_set_option_string(m_mpv, "config", "yes");
    }

    if (mpv_initialize(m_mpv) < 0) {
        emit errorOccurred(tr("Failed to initialize MPV context."));
        return false;
    }

    SubtitleStyleUtils::applyToController(this);

    const bool anime4kEnabled = ConfigStore::instance()->get<bool>(
        ConfigKeys::PlayerAnime4kEnabled, false);
    if (anime4kEnabled) {
        const QString anime4kPreset = ConfigStore::instance()->get<QString>(
            ConfigKeys::PlayerAnime4kPreset, "balanced");
        if (!applyAnime4kShaders(true, anime4kPreset)) {
            qDebug() << "[MpvController] Anime4K shaders not applied at startup"
                     << "| preset=" << anime4kPreset
                     << "| gpu-dumb-mode=" << m_gpuDumbMode;
        }
    }

    mpv_set_wakeup_callback(m_mpv, MpvController::onMpvEvents, this);

    
    observeProperty("time-pos", MPV_FORMAT_DOUBLE);
    observeProperty("duration", MPV_FORMAT_DOUBLE);
    observeProperty("pause", MPV_FORMAT_FLAG);
    observeProperty("eof-reached", MPV_FORMAT_FLAG);
    
    observeProperty("cache-speed", MPV_FORMAT_INT64);
    observeProperty("volume", MPV_FORMAT_DOUBLE);
    observeProperty("mute", MPV_FORMAT_FLAG);
    observeProperty("speed", MPV_FORMAT_DOUBLE);
    observeProperty("track-list", MPV_FORMAT_NODE);
    observeProperty("sid", MPV_FORMAT_NODE);
    observeProperty("secondary-sid", MPV_FORMAT_NODE);
    
    
    observeProperty("paused-for-cache", MPV_FORMAT_FLAG);
    observeProperty("seeking", MPV_FORMAT_FLAG);
    
    return true;
}

bool MpvController::anime4kAvailable() const {
    return !m_gpuDumbMode;
}

bool MpvController::applyAnime4kShaders(bool enabled, const QString &presetId) {
    if (!m_mpv) return false;

    // mpv 的 change-list 命令要求 name/operation/value 三个参数, 即使是 clr 也
    // 必须传 value (空串即可, 会被忽略)。此前清空调用少传参数被 mpv 拒绝且错误
    // 被吞, 导致列表从不清理、append 链不断叠加。
    auto clearShaders = [this]() {
        return command(QVariantList{"change-list", "glsl-shaders", "clr", ""},
                       nullptr) >= 0;
    };
    // glsl-shaders 以节点字符串数组返回 (与 mpvinfodialog 的展示口径一致)。
    auto shaderCount = [this]() -> int {
        const QVariant value = getProperty(QStringLiteral("glsl-shaders"));
        if (value.typeId() == QMetaType::QStringList) {
            return value.toStringList().size();
        }
        int count = 0;
        for (const QVariant &item : value.toList()) {
            if (!item.toString().isEmpty()) {
                ++count;
            }
        }
        return count;
    };

    if (!enabled) {
        if (!clearShaders()) {
            qWarning() << "[MpvController] Failed to clear glsl-shaders";
            return false;
        }
        const int remaining = shaderCount();
        qDebug() << "[MpvController] Cleared Anime4K shaders"
                 << "| remaining=" << remaining;
        if (remaining != 0) {
            qWarning() << "[MpvController] glsl-shaders not empty after clear"
                       << "| remaining=" << remaining;
        }
        return true;
    }

    if (m_gpuDumbMode) {
        qDebug() << "[MpvController] Anime4K shaders unavailable"
                 << "| reason=gpu-dumb-mode";
        return false;
    }

    const bool extracted = Anime4KShaderManager::instance().ensureShadersExtracted();

    const QStringList chain = Anime4KShaderManager::isCustomPresetId(presetId)
        ? Anime4KShaderManager::instance().configuredChain()
        : Anime4KShaderManager::instance().shaderChain(
              Anime4KShaderManager::presetFromId(presetId));
    if (chain.isEmpty()) {
        qDebug() << "[MpvController] Anime4K shaders unavailable"
                 << "| reason=empty shader chain" << "| preset=" << presetId;
        return false;
    }

    if (!extracted) {
        // Tolerate leftovers of a previous extraction: when every shader file
        // of the chain already exists on disk, applying is still safe.
        bool allPresent = true;
        for (const QString &shaderPath : chain) {
            if (!QFile::exists(shaderPath)) {
                allPresent = false;
                break;
            }
        }
        if (!allPresent) {
            qDebug() << "[MpvController] Anime4K shaders unavailable"
                     << "| reason=shader extraction failed";
            return false;
        }
        qDebug() << "[MpvController] Using previously extracted Anime4K shaders"
                 << "| reason=extraction failed but chain files present"
                 << "| preset=" << presetId;
    }

    // 清空失败必须中止: 在残留链上继续 append 只会叠加。
    if (!clearShaders()) {
        qWarning() << "[MpvController] Failed to clear glsl-shaders before apply"
                   << "| preset=" << presetId;
        return false;
    }
    for (const QString &shaderPath : chain) {
        if (command(QVariantList{"change-list", "glsl-shaders", "append",
                                 shaderPath},
                    nullptr) < 0) {
            qDebug() << "[MpvController] Failed to append Anime4K shader"
                     << "| path=" << shaderPath;
            return false;
        }
    }
    const int applied = shaderCount();
    qDebug() << "[MpvController] Applied Anime4K shaders"
             << "| preset=" << presetId
             << "| expected=" << chain.size() << "| actual=" << applied;
    if (applied != chain.size()) {
        qWarning() << "[MpvController] glsl-shaders count mismatch after apply"
                   << "| expected=" << chain.size() << "| actual=" << applied;
    }
    return true;
}

void MpvController::onMpvEvents(void *ctx) {
    
    auto *self = static_cast<MpvController *>(ctx);
    QMetaObject::invokeMethod(self, "eventHandler", Qt::QueuedConnection);
}

void MpvController::eventHandler() {
    if (!m_mpv) return;

    while (true) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) break;

        const mpv_event_id eventId = event->event_id;
        QElapsedTimer dispatchTime;
        dispatchTime.start();
        QString eventDetail;

        switch (event->event_id) {
        case MPV_EVENT_START_FILE:
            m_propertyRequests.clear();
            m_latestPropertyRequests.clear();
            emit playbackStarting();
            break;
        case MPV_EVENT_VIDEO_RECONFIG:
            emit videoOutputChanged();
            break;
        case MPV_EVENT_FILE_LOADED:
            emit fileLoaded();
            break;
        case MPV_EVENT_END_FILE: {
            m_propertyRequests.clear();
            m_latestPropertyRequests.clear();
            
            emit playbackStopped();
            auto *prop = static_cast<mpv_event_end_file *>(event->data);
            if (prop && prop->reason == MPV_END_FILE_REASON_EOF) {
                emit endOfFile("eof");
            } else if (prop && prop->reason == MPV_END_FILE_REASON_ERROR) {
                emit endOfFile("error");
            }
            break;
        }
        case MPV_EVENT_GET_PROPERTY_REPLY: {
            const QString property = m_propertyRequests.take(event->reply_userdata);
            eventDetail = property;
            if (property.isEmpty() ||
                m_latestPropertyRequests.value(property) != event->reply_userdata) {
                break;
            }
            m_latestPropertyRequests.remove(property);
            const auto *prop = static_cast<mpv_event_property *>(event->data);
            QVariant value;
            if (event->error >= 0 && prop && prop->data && prop->format == MPV_FORMAT_NODE) {
                value = nodeToVariant(static_cast<mpv_node *>(prop->data));
            }
            emit propertyRead(property, value);
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto *prop = static_cast<mpv_event_property *>(event->data);
            
            
            if (!prop || !prop->data || prop->format == MPV_FORMAT_NONE) break;

            QString propName = QString::fromUtf8(prop->name);
            eventDetail = propName;
            QVariant data;

            if (prop->format == MPV_FORMAT_DOUBLE) {
                data = *static_cast<double *>(prop->data);
                if (propName == "time-pos") emit positionChanged(data.toDouble());
                else if (propName == "duration") emit durationChanged(data.toDouble());
            } else if (prop->format == MPV_FORMAT_FLAG) {
                data = *static_cast<bool *>(prop->data);
                if (propName == "pause") emit playbackStateChanged(data.toBool());
            } else if (prop->format == MPV_FORMAT_NODE) {
                data = nodeToVariant(static_cast<mpv_node *>(prop->data));
            } else if (prop->format == MPV_FORMAT_STRING) {
                char *str = *static_cast<char **>(prop->data);
                data = str ? QString::fromUtf8(str) : QString();
            } else if (prop->format == MPV_FORMAT_INT64) {
                data = static_cast<qlonglong>(*static_cast<int64_t *>(prop->data));
            }

            emit propertyChanged(propName, data);
            break;
        }
        case MPV_EVENT_LOG_MESSAGE: {
            auto *msg = static_cast<mpv_event_log_message *>(event->data);
            if (msg) {
                
                qDebug().noquote() << QString("[MPV %1] %2").arg(msg->prefix, QString::fromUtf8(msg->text).trimmed());
            }
            break;
        }
        default:
            break;
        }
        const qreal dispatchMs = dispatchTime.nsecsElapsed() / 1000000.0;
        if (dispatchMs >= 25.0) {


            qDebug() << "[MpvController][Timing] Slow event dispatch | event:"
                     << mpv_event_name(eventId) << "| detail:" << eventDetail
                     << "| elapsedMs:" << dispatchMs;
        }
    }
}

void MpvController::observeProperty(const QString &property, mpv_format format, uint64_t id) {
    if (m_mpv) {
        mpv_observe_property(m_mpv, id, property.toUtf8().constData(), format);
    }
}

int MpvController::setProperty(const QString &property, const QVariant &value) {
    if (!m_mpv) return MPV_ERROR_UNINITIALIZED;
    mpv_node node;
    memset(&node, 0, sizeof(mpv_node)); 
    setNode(&node, value);
    int err = mpv_set_property(m_mpv, property.toUtf8().constData(), MPV_FORMAT_NODE, &node);
    freeNode(&node);
    if (err < 0) {
        if (property.startsWith(QStringLiteral("secondary-sub-"))) {
            qDebug() << "[MpvController] setProperty failed"
                     << "| property:" << property
                     << "| error:" << mpv_error_string(err);
        } else {
            qWarning() << "[MpvController] setProperty failed"
                       << "| property:" << property
                       << "| error:" << mpv_error_string(err);
        }
    }
    return err;
}

void MpvController::requestProperty(const QString &property) {
    if (!m_mpv) return;
    const quint64 id = ++m_propertyRequestSerial;
    m_propertyRequests.insert(id, property);
    m_latestPropertyRequests.insert(property, id);
    const int error = mpv_get_property_async(m_mpv, id, property.toUtf8().constData(), MPV_FORMAT_NODE);
    if (error < 0) {
        m_propertyRequests.remove(id);
        m_latestPropertyRequests.remove(property);
        qDebug() << "[MpvController] Async property request failed | property:"
                 << property << "| error:" << error;
        emit propertyRead(property, QVariant());
    }
}

QVariant MpvController::getProperty(const QString &property) {
    if (!m_mpv) return QVariant();
    mpv_node node;
    memset(&node, 0, sizeof(mpv_node)); 
    QElapsedTimer readTime;
    readTime.start();
    int err = mpv_get_property(m_mpv, property.toUtf8().constData(), MPV_FORMAT_NODE, &node);
    const qreal readMs = readTime.nsecsElapsed() / 1000000.0;
    if (readMs >= 25.0) {
        
        qDebug() << "[MpvController][Timing] Slow property read | property:"
                 << property << "| elapsedMs:" << readMs << "| result:" << err;
    }
    if (err < 0) return QVariant();
    node_autofree f(&node);
    return nodeToVariant(&node);
}

int MpvController::command(const QVariant &params, QVariant *resultOut) {
    if (!m_mpv) return MPV_ERROR_UNINITIALIZED;

    mpv_node node;
    memset(&node, 0, sizeof(mpv_node));
    setNode(&node, params);

    mpv_node mpvResult;
    memset(&mpvResult, 0, sizeof(mpv_node)); 

    int err = mpv_command_node(m_mpv, &node, &mpvResult);
    freeNode(&node);
    node_autofree f(&mpvResult);

    if (err < 0) {
        QString commandName;
        const QVariantList args = params.toList();
        if (!args.isEmpty()) {
            commandName = args.first().toString();
        }
        qWarning() << "[MpvController] command failed"
                   << "| command:" << (commandName.isEmpty()
                                           ? QStringLiteral("<unknown>")
                                           : commandName)
                   << "| error:" << mpv_error_string(err);
        return err;
    }

    if (resultOut) {
        *resultOut = nodeToVariant(&mpvResult);
    }
    return err;
}

QVariant MpvController::command(const QVariant &params) {
    QVariant result;
    const int err = command(params, &result);
    return err < 0 ? QVariant() : result;
}



mpv_node_list *MpvController::createList(mpv_node *dst, bool is_map, int num) {
    dst->format = is_map ? MPV_FORMAT_NODE_MAP : MPV_FORMAT_NODE_ARRAY;
    auto *list = new mpv_node_list();
    dst->u.list = list;
    list->values = new mpv_node[num]();
    if (is_map) list->keys = new char *[num]();
    return list;
}

void MpvController::setNode(mpv_node *dst, const QVariant &src) {
    if (src.typeId() == QMetaType::QString) {
        dst->format = MPV_FORMAT_STRING;
        dst->u.string = qstrdup(src.toString().toUtf8().constData());
    } else if (src.typeId() == QMetaType::Bool) {
        dst->format = MPV_FORMAT_FLAG;
        dst->u.flag = src.toBool() ? 1 : 0;
    } else if (src.typeId() == QMetaType::Int || src.typeId() == QMetaType::LongLong || src.typeId() == QMetaType::Double) {
        if (src.typeId() == QMetaType::Double) {
            dst->format = MPV_FORMAT_DOUBLE;
            dst->u.double_ = src.toDouble();
        } else {
            dst->format = MPV_FORMAT_INT64;
            dst->u.int64 = src.toLongLong();
        }
    } else if (src.canConvert<QVariantList>()) {
        QVariantList qlist = src.toList();
        mpv_node_list *list = createList(dst, false, qlist.size());
        list->num = qlist.size();
        for (int n = 0; n < qlist.size(); ++n) setNode(&list->values[n], qlist[n]);
    } else if (src.canConvert<QVariantMap>()) {
        QVariantMap qmap = src.toMap();
        mpv_node_list *list = createList(dst, true, qmap.size());
        list->num = qmap.size();
        int n = 0;
        for (auto it = qmap.constKeyValueBegin(); it != qmap.constKeyValueEnd(); ++it) {
            list->keys[n] = qstrdup(it->first.toUtf8().constData());
            setNode(&list->values[n], it->second);
            ++n;
        }
    } else {
        dst->format = MPV_FORMAT_NONE;
    }
}

void MpvController::freeNode(mpv_node *dst) {
    switch (dst->format) {
    case MPV_FORMAT_STRING:
        delete[] dst->u.string;
        break;
    case MPV_FORMAT_NODE_ARRAY:
    case MPV_FORMAT_NODE_MAP: {
        mpv_node_list *list = dst->u.list;
        if (list) {
            for (int n = 0; n < list->num; ++n) {
                if (list->keys) delete[] list->keys[n];
                if (list->values) freeNode(&list->values[n]);
            }
            delete[] list->keys;
            delete[] list->values;
        }
        delete list;
        break;
    }
    default: break;
    }
    dst->format = MPV_FORMAT_NONE;
}

QVariant MpvController::nodeToVariant(const mpv_node *node) {
    switch (node->format) {
    case MPV_FORMAT_STRING: return QVariant(QString::fromUtf8(node->u.string));
    case MPV_FORMAT_FLAG: return QVariant(static_cast<bool>(node->u.flag));
    case MPV_FORMAT_INT64: return QVariant(static_cast<qlonglong>(node->u.int64));
    case MPV_FORMAT_DOUBLE: return QVariant(node->u.double_);
    case MPV_FORMAT_NODE_ARRAY: {
        mpv_node_list *list = node->u.list;
        QVariantList qlist;
        for (int n = 0; n < list->num; ++n) qlist.append(nodeToVariant(&list->values[n]));
        return QVariant(qlist);
    }
    case MPV_FORMAT_NODE_MAP: {
        mpv_node_list *list = node->u.list;
        QVariantMap qmap;
        for (int n = 0; n < list->num; ++n) qmap.insert(QString::fromUtf8(list->keys[n]), nodeToVariant(&list->values[n]));
        return QVariant(qmap);
    }
    default: return QVariant();
    }
}
