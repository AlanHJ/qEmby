#include "mpvinfodialog.h"

#include "mpvcontroller.h"

#include <QFontDatabase>
#include <QFrame>
#include <QHideEvent>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace
{

constexpr int kRefreshIntervalMs = 1000;
constexpr int kKeyColumnWidth = 30;

const QString kMissingValue = QStringLiteral("-");

QString stringValue(const QVariant &value)
{
    if (!value.isValid())
    {
        return kMissingValue;
    }
    const QString text = value.toString();
    return text.isEmpty() ? kMissingValue : text;
}

QString formatDouble(const QVariant &value, int precision)
{
    if (!value.isValid())
    {
        return kMissingValue;
    }
    bool ok = false;
    const double number = value.toDouble(&ok);
    if (!ok)
    {
        return kMissingValue;
    }
    QString text = QString::number(number, 'f', precision);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0')))
    {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.')))
    {
        text.chop(1);
    }
    return text;
}

QString formatScaled(const QVariant &value, double divisor, int precision,
                     const QString &suffix)
{
    if (!value.isValid())
    {
        return kMissingValue;
    }
    bool ok = false;
    const double number = value.toDouble(&ok);
    if (!ok)
    {
        return kMissingValue;
    }
    return QString::number(number / divisor, 'f', precision) + suffix;
}

QString infoRow(const QString &key, const QString &value)
{
    return QStringLiteral("  ") +
           key.leftJustified(kKeyColumnWidth, QLatin1Char(' ')) +
           QStringLiteral(" : ") + value;
}

} // namespace

MpvInfoDialog::MpvInfoDialog(MpvController *controller, QWidget *parent)
    : PlayerOverlayDialog(parent)
    , m_controller(controller)
{
    setSurfaceObjectName(QStringLiteral("playerMpvInfoDialog"));
    setSurfacePreferredSize(QSize(620, 520));
    setTitle(tr("MPV Info"));

    contentLayout()->setContentsMargins(16, 8, 16, 16);
    contentLayout()->setSpacing(10);

    m_textView = new QPlainTextEdit(this);
    m_textView->setObjectName(QStringLiteral("playerMpvInfoText"));
    m_textView->setReadOnly(true);
    m_textView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_textView->setFrameShape(QFrame::NoFrame);
    m_textView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);
    m_textView->setFont(monoFont);
    contentLayout()->addWidget(m_textView, 1);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(kRefreshIntervalMs);
    connect(m_refreshTimer, &QTimer::timeout, this, &MpvInfoDialog::refreshInfo);

    refreshInfo();
}

void MpvInfoDialog::showEvent(QShowEvent *event)
{
    PlayerOverlayDialog::showEvent(event);
    refreshInfo();
    m_refreshTimer->start();
}

void MpvInfoDialog::hideEvent(QHideEvent *event)
{
    m_refreshTimer->stop();
    PlayerOverlayDialog::hideEvent(event);
}

void MpvInfoDialog::refreshInfo()
{
    if (!m_textView)
    {
        return;
    }

    QScrollBar *scrollBar = m_textView->verticalScrollBar();
    const int scrollValue = scrollBar ? scrollBar->value() : 0;
    m_textView->setPlainText(buildInfoText());
    if (scrollBar)
    {
        scrollBar->setValue(scrollValue);
    }
}

QString MpvInfoDialog::buildInfoText() const
{
    if (!m_controller)
    {
        return tr("MPV controller unavailable");
    }

    QStringList lines;

    QString resolution = kMissingValue;
    const QVariant width = m_controller->getProperty(QStringLiteral("video-params/w"));
    const QVariant height = m_controller->getProperty(QStringLiteral("video-params/h"));
    if (width.isValid() && height.isValid())
    {
        resolution = QStringLiteral("%1x%2")
                         .arg(width.toLongLong())
                         .arg(height.toLongLong());
    }

    lines << tr("File");
    lines << infoRow(QStringLiteral("media-title"),
                     stringValue(m_controller->getProperty(QStringLiteral("media-title"))));
    lines << infoRow(QStringLiteral("filename"),
                     stringValue(m_controller->getProperty(QStringLiteral("filename"))));
    lines << infoRow(QStringLiteral("file-format"),
                     stringValue(m_controller->getProperty(QStringLiteral("file-format"))));

    lines << QString();
    lines << tr("Video");
    lines << infoRow(QStringLiteral("video-params/w x h"), resolution);
    lines << infoRow(QStringLiteral("video-params/pixelformat"),
                     stringValue(m_controller->getProperty(QStringLiteral("video-params/pixelformat"))));
    lines << infoRow(QStringLiteral("video-params/aspect"),
                     formatDouble(m_controller->getProperty(QStringLiteral("video-params/aspect")), 3));
    lines << infoRow(QStringLiteral("container-fps"),
                     formatDouble(m_controller->getProperty(QStringLiteral("container-fps")), 3));
    lines << infoRow(QStringLiteral("estimated-vf-fps"),
                     formatDouble(m_controller->getProperty(QStringLiteral("estimated-vf-fps")), 3));

    lines << QString();
    lines << tr("Playback");
    lines << infoRow(QStringLiteral("frame-drop-count"),
                     stringValue(m_controller->getProperty(QStringLiteral("frame-drop-count"))));
    lines << infoRow(QStringLiteral("vo-delayed-frame-count"),
                     stringValue(m_controller->getProperty(QStringLiteral("vo-delayed-frame-count"))));
    lines << infoRow(QStringLiteral("mistimed-frame-count"),
                     stringValue(m_controller->getProperty(QStringLiteral("mistimed-frame-count"))));
    lines << infoRow(QStringLiteral("avsync"),
                     formatDouble(m_controller->getProperty(QStringLiteral("avsync")), 3));

    lines << QString();
    lines << tr("Audio");
    lines << infoRow(QStringLiteral("audio-params/samplerate"),
                     formatScaled(m_controller->getProperty(QStringLiteral("audio-params/samplerate")),
                                  1.0, 0, QStringLiteral(" Hz")));
    lines << infoRow(QStringLiteral("audio-params/channel-layout"),
                     stringValue(m_controller->getProperty(QStringLiteral("audio-params/channel-layout"))));
    lines << infoRow(QStringLiteral("audio-bitrate"),
                     formatScaled(m_controller->getProperty(QStringLiteral("audio-bitrate")),
                                  1000.0, 1, QStringLiteral(" kbps")));

    lines << QString();
    lines << tr("Bitrate/Cache");
    lines << infoRow(QStringLiteral("video-bitrate"),
                     formatScaled(m_controller->getProperty(QStringLiteral("video-bitrate")),
                                  1000.0, 1, QStringLiteral(" kbps")));
    lines << infoRow(QStringLiteral("demuxer-cache-duration"),
                     formatScaled(m_controller->getProperty(QStringLiteral("demuxer-cache-duration")),
                                  1.0, 1, QStringLiteral(" s")));
    lines << infoRow(QStringLiteral("cache-speed"),
                     formatScaled(m_controller->getProperty(QStringLiteral("cache-speed")),
                                  1024.0, 1, QStringLiteral(" KiB/s")));

    lines << QString();
    lines << tr("Output");
    lines << infoRow(QStringLiteral("vo-current"),
                     stringValue(m_controller->getProperty(QStringLiteral("vo-current"))));
    lines << infoRow(QStringLiteral("hwdec-current"),
                     stringValue(m_controller->getProperty(QStringLiteral("hwdec-current"))));
    const QVariant videoCodec =
        m_controller->getProperty(QStringLiteral("current-tracks/video/codec"));
    if (videoCodec.isValid() && !videoCodec.toString().isEmpty())
    {
        lines << infoRow(QStringLiteral("current-tracks/video/codec"),
                         videoCodec.toString());
    }

    return lines.join(QLatin1Char('\n'));
}
