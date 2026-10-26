#pragma once
#include <QByteArray>
#include <QImage>
#include <QList>
#include <QString>
#include <QSharedPointer>
namespace ApngDecoder {
struct Frame { QImage image; int delayMs = 100; };
struct Animation;
class StreamDecoder {
public:
    Frame nextFrame();
    int frameCount() const;
private:
    struct State;
    QSharedPointer<State> m_state;
    friend Animation decode(QByteArray bytes);
};
struct Animation {
    QSize canvasSize;
    QImage preview;
    QList<Frame> frames;
    quint32 loops = 0;
    QString failure;
    QSharedPointer<StreamDecoder> stream;
    quint64 storageBytes = 0;
    bool isAnimated() const { return frames.size() > 1 || !stream.isNull(); }
};

inline constexpr qsizetype MaxCompressedBytes = 32 * 1024 * 1024;
inline constexpr quint64 MaxCanvasPixels = 8 * 1024 * 1024;
inline constexpr quint64 MaxDecodedBytes = 96 * 1024 * 1024;
inline constexpr quint32 MaxFrames = 512;
Animation decode(QByteArray bytes);
}
