#ifndef MPVPREVIEWUTILS_H
#define MPVPREVIEWUTILS_H

#include <QImage>
#include <QString>
#include <QVariantMap>
#include <QMetaType>
#include <atomic>
#include <memory>

namespace MpvPreviewUtils {
struct Request {
    QString url;
    QVariantMap networkOptions;
    double seconds = 0.0;
    std::shared_ptr<std::atomic_bool> cancelled;
};

struct Result {
    QImage image;
    int error = 0;
    QString stage;
    bool reusedSession = false;
};



class Session
{
public:
    Session();
    ~Session();
    Result decode(Request request);
    void reset();

private:
    struct State;
    std::unique_ptr<State> m_state;
};
}

Q_DECLARE_METATYPE(MpvPreviewUtils::Request)
Q_DECLARE_METATYPE(MpvPreviewUtils::Result)

#endif
