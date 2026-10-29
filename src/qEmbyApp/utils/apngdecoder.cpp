#include "apngdecoder.h"
#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QtEndian>
#include <QMutex>
#include <QMutexLocker>

namespace {
quint32 integer(const QByteArray& bytes, qsizetype offset)
{
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(bytes.constData() + offset));
}
quint32 crc(const QByteArray& data)
{
    quint32 value = 0xffffffff;
    for (const unsigned char byte : data) {
        value ^= byte;
        for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ (0xedb88320U & (0U - (value & 1)));
    }
    return value ^ 0xffffffff;
}
QByteArray word(quint32 value)
{
    QByteArray bytes(4, Qt::Uninitialized);
    qToBigEndian(value, reinterpret_cast<uchar*>(bytes.data()));
    return bytes;
}
QByteArray chunk(const QByteArray& type, const QByteArray& data)
{
    const auto body = type + data;
    return word(data.size()) + body + word(crc(body));
}
struct RawFrame {
    quint32 width = 0, height = 0, x = 0, y = 0;
    int delay = 100;
    uchar dispose = 0, blend = 0;
    QByteArray compressed;
};
}

struct ApngDecoder::StreamDecoder::State {
    QByteArray header;
    QByteArray shared;
    QList<RawFrame> frames;
    QImage canvas;
    int next = 0;
    QMutex mutex;
};

int ApngDecoder::StreamDecoder::frameCount() const { return m_state ? m_state->frames.size() : 0; }

ApngDecoder::Frame ApngDecoder::StreamDecoder::nextFrame()
{
    if (!m_state || m_state->frames.isEmpty()) return {};
    auto& state = *m_state;
    QMutexLocker locker(&state.mutex);
    if (state.next >= state.frames.size()) { state.next = 0; state.canvas.fill(Qt::transparent); }
    const auto& frame = state.frames[state.next];
    QByteArray header = state.header;
    header.replace(0, 4, word(frame.width)); header.replace(4, 4, word(frame.height));
    const QByteArray png = QByteArray::fromHex("89504e470d0a1a0a") + chunk("IHDR", header) + state.shared +
                           chunk("IDAT", frame.compressed) + chunk("IEND", {});
    const QImage decoded = QImage::fromData(png, "PNG");
    if (decoded.isNull() || decoded.size() != QSize(frame.width, frame.height)) return {};
    const QImage previous = frame.dispose == 2 && state.next > 0 ? state.canvas : QImage();
    {
        QPainter painter(&state.canvas);
        painter.setCompositionMode(frame.blend == 0 ? QPainter::CompositionMode_Source : QPainter::CompositionMode_SourceOver);
        painter.drawImage(QPoint(frame.x, frame.y), decoded);
    }
    Frame result{state.canvas, frame.delay};
    if (frame.dispose == 1 || (frame.dispose == 2 && state.next == 0)) {
        QPainter painter(&state.canvas);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(QRect(frame.x, frame.y, frame.width, frame.height), Qt::transparent);
    } else if (frame.dispose == 2) state.canvas = previous;
    ++state.next;
    return result;
}

ApngDecoder::Animation ApngDecoder::decode(QByteArray bytes)
{
    Animation result;
    auto fail = [&result](const char* reason) {
        result.frames.clear(); result.stream.clear(); result.failure = QString::fromLatin1(reason); return result;
    };
    if (bytes.size() > MaxCompressedBytes) return fail("compressed-budget");
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    result.canvasSize = reader.size();
    if (!result.canvasSize.isValid() || quint64(result.canvasSize.width()) * result.canvasSize.height() > MaxCanvasPixels)
        return fail("canvas-budget");
    result.preview = reader.read();
    const QByteArray signature = QByteArray::fromHex("89504e470d0a1a0a");
    if (!bytes.startsWith(signature)) return result;
    QByteArray ihdr, shared;
    QList<RawFrame> frames;
    quint32 declaredFrames = 0, sequence = 0;
    bool animated = false, seenData = false, ended = false, defaultInAnimation = false;
    for (qsizetype offset = 8; offset < bytes.size();) {
        if (bytes.size() - offset < 12) return fail("truncated-chunk");
        const quint32 length = integer(bytes, offset);
        if (quint64(length) + 12 > quint64(bytes.size() - offset)) return fail("chunk-bounds");
        const QByteArray type = bytes.mid(offset + 4, 4);
        const QByteArray data = bytes.mid(offset + 8, length);
        if (crc(type + data) != integer(bytes, offset + 8 + length)) return fail("chunk-crc");
        offset += qsizetype(length) + 12;
        if (ihdr.isEmpty() && type != "IHDR") return fail("missing-ihdr");
        if (type == "IHDR") {
            if (length != 13 || !ihdr.isEmpty()) return fail("invalid-ihdr");
            ihdr = data;
            if (integer(data, 0) != quint32(result.canvasSize.width()) || integer(data, 4) != quint32(result.canvasSize.height()))
                return fail("canvas-mismatch");
        } else if (type == "acTL") {
            if (length != 8 || animated || seenData) return fail("invalid-actl");
            animated = true; declaredFrames = integer(data, 0); result.loops = integer(data, 4);
            if (!declaredFrames || declaredFrames > MaxFrames) return fail("frame-count-budget");
            if (quint64(result.canvasSize.width()) * result.canvasSize.height() * 4 * 5 > MaxDecodedBytes)
                return fail("working-canvas-budget");
        } else if (type == "fcTL") {
            if (!animated || length != 26 || integer(data, 0) != sequence++ ||
                (!frames.isEmpty() && frames.last().compressed.isEmpty())) return fail("invalid-fctl");
            if (frames.size() >= declaredFrames) return fail("frame-count");
            RawFrame frame;
            frame.width = integer(data, 4); frame.height = integer(data, 8);
            frame.x = integer(data, 12); frame.y = integer(data, 16);
            if (!frame.width || !frame.height || quint64(frame.x) + frame.width > quint64(result.canvasSize.width()) ||
                quint64(frame.y) + frame.height > quint64(result.canvasSize.height())) return fail("frame-bounds");
            const auto* delays = reinterpret_cast<const uchar*>(data.constData() + 20);
            const quint16 numerator = qFromBigEndian<quint16>(delays);
            const quint16 denominator = qFromBigEndian<quint16>(delays + 2);
            frame.delay = qMax(10, int((quint64(numerator) * 1000) / (denominator ? denominator : 100)));
            frame.dispose = uchar(data[24]); frame.blend = uchar(data[25]);
            if (frame.dispose > 2 || frame.blend > 1) return fail("frame-operation");
            if (!seenData) {
                if (!frames.isEmpty() || frame.x || frame.y || frame.width != quint32(result.canvasSize.width()) ||
                    frame.height != quint32(result.canvasSize.height())) return fail("default-frame-bounds");
                defaultInAnimation = true;
            }
            frames.append(frame);
        } else if (type == "IDAT") {
            if (seenData && frames.size() > 1) return fail("idat-order");
            seenData = true;
            if (defaultInAnimation && frames.size() == 1) frames.last().compressed += data;
        } else if (type == "fdAT") {
            if (!animated || !seenData || length < 4 || frames.isEmpty() ||
                (defaultInAnimation && frames.size() == 1) || integer(data, 0) != sequence++) return fail("invalid-fdat");
            frames.last().compressed += data.mid(4);
        } else if (type == "IEND") {
            if (length || offset != bytes.size()) return fail("invalid-iend");
            ended = true; break;
        } else if (!seenData && (type == "PLTE" || type == "tRNS" || type == "gAMA" ||
                                type == "cHRM" || type == "sRGB" || type == "iCCP" || type == "sBIT")) {
            shared += chunk(type, data);
        } else if (!type.isEmpty() && !(uchar(type[0]) & 0x20)) {
            return fail("unknown-critical-chunk");
        }
    }
    if (!animated) return result;
    if (!ended || frames.size() != declaredFrames) return fail("incomplete-animation");
    const quint64 canvasBytes = quint64(result.canvasSize.width()) * result.canvasSize.height() * 4;
    if (canvasBytes * (quint64(declaredFrames) + 3) > MaxDecodedBytes) {
        auto stream = QSharedPointer<StreamDecoder>::create();
        stream->m_state = QSharedPointer<StreamDecoder::State>::create();
        stream->m_state->header = ihdr;
        stream->m_state->shared = shared;
        stream->m_state->frames = std::move(frames);
        stream->m_state->canvas = QImage(result.canvasSize, QImage::Format_ARGB32_Premultiplied);
        if (stream->m_state->canvas.isNull()) return fail("canvas-allocation");
        stream->m_state->canvas.fill(Qt::transparent);
        const Frame first = stream->nextFrame();
        if (first.image.isNull()) return fail("frame-decode");
        result.preview = first.image;
        result.frames.append(first);
        result.stream = stream;
        result.storageBytes = canvasBytes * 5 + bytes.size();
        return result;
    }
    QImage canvas(result.canvasSize, QImage::Format_ARGB32_Premultiplied);
    if (canvas.isNull()) return fail("canvas-allocation");
    canvas.fill(Qt::transparent);
    for (qsizetype i = 0; i < frames.size(); ++i) {
        const auto& frame = frames[i];
        if (frame.compressed.isEmpty()) return fail("empty-frame");
        QByteArray header = ihdr;
        header.replace(0, 4, word(frame.width)); header.replace(4, 4, word(frame.height));
        const QByteArray png = signature + chunk("IHDR", header) + shared + chunk("IDAT", frame.compressed) + chunk("IEND", {});
        const QImage decoded = QImage::fromData(png, "PNG");
        if (decoded.isNull() || decoded.size() != QSize(frame.width, frame.height)) return fail("frame-decode");
        const QImage previous = frame.dispose == 2 && i > 0 ? canvas : QImage();
        {
            QPainter painter(&canvas);
            painter.setCompositionMode(frame.blend == 0 ? QPainter::CompositionMode_Source : QPainter::CompositionMode_SourceOver);
            painter.drawImage(QPoint(frame.x, frame.y), decoded);
        }
        result.frames.append({canvas, frame.delay});
        if (frame.dispose == 1 || (frame.dispose == 2 && i == 0)) {
            QPainter painter(&canvas);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(QRect(frame.x, frame.y, frame.width, frame.height), Qt::transparent);
        } else if (frame.dispose == 2) canvas = previous;
    }
    if (!result.frames.isEmpty()) result.preview = result.frames.first().image;
    return result;
}
