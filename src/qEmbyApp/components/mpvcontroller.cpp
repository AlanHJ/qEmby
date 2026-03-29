#include "mpvcontroller.h"
#include <QDebug>
#include <QMetaType>
#include <clocale>
#include <config/configstore.h>
#include <config/config_keys.h>
#include <QStandardPaths>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <QThread>
#include <windows.h>
#endif


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
    if (m_mpv) {
        
        this->blockSignals(true);

        mpv_command_string(m_mpv, "stop");
        
        mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
        
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
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
    mpv_request_log_messages(m_mpv, "info");

    
    
    
    QString vsyncMode = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerVideoSync, "audio");
    mpv_set_option_string(m_mpv, "video-sync", vsyncMode.toUtf8().constData());
    
    mpv_set_option_string(m_mpv, "interpolation", vsyncMode == "display-resample" ? "yes" : "no");

    
    
    
    QString hwdec = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerHwDec, "auto-copy");

#ifdef Q_OS_WIN
    mpv_set_option_string(m_mpv, "gpu-api", "opengl");
    mpv_set_option_string(m_mpv, "opengl-es", "no");
    bool isRDP = GetSystemMetrics(SM_REMOTESESSION) != 0;
    if (isRDP) {
        
        hwdec = "no";
        mpv_set_option_string(m_mpv, "profile", "sw-fast");

        
        mpv_set_option_string(m_mpv, "gpu-dumb-mode", "yes");
        mpv_set_option_string(m_mpv, "opengl-pbo", "no");
        mpv_set_option_string(m_mpv, "dither-depth", "no");
    }
#endif
    mpv_set_option_string(m_mpv, "hwdec", hwdec.toUtf8().constData());

    
    
    
    QString audioLang = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerAudioLang, "auto");
    if (audioLang != "auto" && !audioLang.isEmpty()) {
        mpv_set_option_string(m_mpv, "alang", audioLang.toUtf8().constData());
    }

    QString subLang = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSubLang, "auto");
    if (subLang == "none") {
        mpv_set_option_string(m_mpv, "sid", "no");
    } else if (subLang != "auto" && !subLang.isEmpty()) {
        mpv_set_option_string(m_mpv, "slang", subLang.toUtf8().constData());
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

    mpv_set_wakeup_callback(m_mpv, MpvController::onMpvEvents, this);

    
    observeProperty("time-pos", MPV_FORMAT_DOUBLE);
    observeProperty("duration", MPV_FORMAT_DOUBLE);
    observeProperty("pause", MPV_FORMAT_FLAG);
    
    observeProperty("cache-speed", MPV_FORMAT_INT64);
    observeProperty("volume", MPV_FORMAT_DOUBLE);
    observeProperty("mute", MPV_FORMAT_FLAG);
    
    
    observeProperty("paused-for-cache", MPV_FORMAT_FLAG);
    observeProperty("seeking", MPV_FORMAT_FLAG);
    
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

        switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED:
            emit fileLoaded();
            break;
        case MPV_EVENT_END_FILE: {
            auto *prop = static_cast<mpv_event_end_file *>(event->data);
            if (prop && prop->reason == MPV_END_FILE_REASON_EOF) {
                emit endOfFile("eof");
            } else if (prop && prop->reason == MPV_END_FILE_REASON_ERROR) {
                emit endOfFile("error");
            }
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto *prop = static_cast<mpv_event_property *>(event->data);
            
            
            if (!prop || !prop->data || prop->format == MPV_FORMAT_NONE) break;

            QString propName = QString::fromUtf8(prop->name);
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
    return err;
}

QVariant MpvController::getProperty(const QString &property) {
    if (!m_mpv) return QVariant();
    mpv_node node;
    memset(&node, 0, sizeof(mpv_node)); 
    int err = mpv_get_property(m_mpv, property.toUtf8().constData(), MPV_FORMAT_NODE, &node);
    if (err < 0) return QVariant();
    node_autofree f(&node);
    return nodeToVariant(&node);
}

QVariant MpvController::command(const QVariant &params) {
    if (!m_mpv) return QVariant();

    mpv_node node;
    memset(&node, 0, sizeof(mpv_node));
    setNode(&node, params);

    mpv_node result;
    memset(&result, 0, sizeof(mpv_node)); 

    int err = mpv_command_node(m_mpv, &node, &result);
    freeNode(&node);

    if (err < 0) return QVariant();
    node_autofree f(&result);
    return nodeToVariant(&result);
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
