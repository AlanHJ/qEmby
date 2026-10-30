#include "mpvinfodialog.h"

#include "anime4kshadermanager.h"
#include "mpvcontroller.h"

#include <config/config_keys.h>
#include <config/configstore.h>
#include <QCloseEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFrame>
#include <QGuiApplication>
#include <QHideEvent>
#include <QMoveEvent>
#include <QPlainTextEdit>
#include <QScreen>
#include <QScrollBar>
#include <QSettings>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace
{

constexpr int kRefreshIntervalMs = 1000;
constexpr int kGeometrySaveDelayMs = 400;
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

// glsl-shaders comes back from mpv as a node array of strings.
QStringList stringListValue(const QVariant &value)
{
    QStringList result;
    if (!value.isValid())
    {
        return result;
    }
    if (value.typeId() == QMetaType::QStringList)
    {
        return value.toStringList();
    }
    if (value.typeId() == QMetaType::QVariantList)
    {
        for (const QVariant &item : value.toList())
        {
            const QString text = item.toString();
            if (!text.isEmpty())
            {
                result.append(text);
            }
        }
        return result;
    }
    const QString text = value.toString();
    if (!text.isEmpty())
    {
        result.append(text);
    }
    return result;
}

// vf/af come back as a node array of {name, params} maps; fall back to the
// raw text when mpv reports them differently. Returns an empty string when no
// filter is active.
QString filterSummary(const QVariant &value)
{
    if (value.typeId() == QMetaType::QVariantList)
    {
        QStringList names;
        for (const QVariant &item : value.toList())
        {
            const QVariantMap map = item.toMap();
            const QString name = map.contains(QStringLiteral("name"))
                ? map.value(QStringLiteral("name")).toString()
                : item.toString();
            if (!name.isEmpty())
            {
                names.append(name);
            }
        }
        return names.join(QStringLiteral(", "));
    }
    return value.toString();
}

} // namespace

MpvInfoDialog::MpvInfoDialog(MpvController *controller, QWidget *playerWindow)
    : QWidget(nullptr, Qt::Window)
    , m_controller(controller)
{
    setObjectName(QStringLiteral("playerMpvInfoDialog"));
    setWindowTitle(tr("MPV Info"));
    setMinimumSize(QSize(420, 320));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 8, 16, 16);
    layout->setSpacing(10);

    m_textView = new QPlainTextEdit(this);
    m_textView->setObjectName(QStringLiteral("playerMpvInfoText"));
    m_textView->setReadOnly(true);
    m_textView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_textView->setFrameShape(QFrame::NoFrame);
    m_textView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);
    m_textView->setFont(monoFont);
    layout->addWidget(m_textView, 1);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(kRefreshIntervalMs);
    connect(m_refreshTimer, &QTimer::timeout, this, &MpvInfoDialog::refreshInfo);

    m_geometrySaveTimer = new QTimer(this);
    m_geometrySaveTimer->setSingleShot(true);
    m_geometrySaveTimer->setInterval(kGeometrySaveDelayMs);
    connect(m_geometrySaveTimer, &QTimer::timeout, this,
            &MpvInfoDialog::saveGeometryToSettings);

    restoreOrPlaceWindow(playerWindow);
    refreshInfo();
}

void MpvInfoDialog::setController(MpvController *controller)
{
    if (m_controller == controller)
    {
        return;
    }
    m_controller = controller;
    refreshInfo();
}

void MpvInfoDialog::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshInfo();
    m_refreshTimer->start();
}

void MpvInfoDialog::hideEvent(QHideEvent *event)
{
    m_refreshTimer->stop();
    QWidget::hideEvent(event);
}

void MpvInfoDialog::closeEvent(QCloseEvent *event)
{
    saveGeometryToSettings();
    QWidget::closeEvent(event);
}

void MpvInfoDialog::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    if (m_geometrySaveTimer)
    {
        m_geometrySaveTimer->start();
    }
}

void MpvInfoDialog::restoreOrPlaceWindow(const QWidget *playerWindow)
{
    QSettings settings(QStringLiteral("qEmby"), QStringLiteral("MpvInfoDialog"));
    const QByteArray saved = settings.value(QStringLiteral("geometry")).toByteArray();
    if (!saved.isEmpty() && restoreGeometry(saved))
    {
        return;
    }

    resize(620, 520);

    QPoint pos;
    if (playerWindow && playerWindow->isVisible())
    {
        // Right of the player window so the video area stays unobstructed.
        const QRect playerRect = playerWindow->geometry();
        pos = QPoint(playerRect.right() + 20, playerRect.top() + 40);
    }
    else
    {
        const QRect available =
            QGuiApplication::primaryScreen()->availableGeometry();
        pos = available.center() - QPoint(width() / 2, height() / 2);
    }

    QScreen *screen = QGuiApplication::screenAt(pos);
    const QRect available = screen ? screen->availableGeometry()
                                   : QGuiApplication::primaryScreen()->availableGeometry();
    pos.setX(qBound(available.left(), pos.x(), available.right() - width() + 1));
    pos.setY(qBound(available.top(), pos.y(), available.bottom() - height() + 1));
    move(pos);
}

void MpvInfoDialog::saveGeometryToSettings() const
{
    QSettings settings(QStringLiteral("qEmby"), QStringLiteral("MpvInfoDialog"));
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
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
        return tr("No playback");
    }

    QStringList lines;

    lines << tr("Shaders");
    const QString presetId = ConfigStore::instance()->get<QString>(
        ConfigKeys::PlayerAnime4kPreset, QStringLiteral("balanced"));
    const QString presetName = Anime4KShaderManager::isCustomPresetId(presetId)
        ? tr("Custom")
        : Anime4KShaderManager::presetDisplayName(
              Anime4KShaderManager::presetFromId(presetId));
    lines << infoRow(QStringLiteral("preset"), presetName);
    const QStringList shaderChain = stringListValue(
        m_controller->getProperty(QStringLiteral("glsl-shaders")));
    if (shaderChain.isEmpty())
    {
        lines << infoRow(QStringLiteral("glsl-shaders"), tr("(none)"));
    }
    else
    {
        lines << infoRow(QStringLiteral("glsl-shaders"),
                         QString::number(shaderChain.size()));
        for (int index = 0; index < shaderChain.size(); ++index)
        {
            const QString &shaderPath = shaderChain.at(index);
            lines << QStringLiteral("    %1. %2")
                         .arg(index + 1)
                         .arg(QFileInfo(shaderPath).fileName());
            lines << QStringLiteral("       ") + shaderPath;
        }
    }

    lines << QString();
    lines << tr("Filters");
    const QString videoFilters =
        filterSummary(m_controller->getProperty(QStringLiteral("vf")));
    const QString audioFilters =
        filterSummary(m_controller->getProperty(QStringLiteral("af")));
    lines << infoRow(QStringLiteral("vf"),
                     videoFilters.isEmpty() ? tr("(none)") : videoFilters);
    lines << infoRow(QStringLiteral("af"),
                     audioFilters.isEmpty() ? tr("(none)") : audioFilters);

    QString resolution = kMissingValue;
    const QVariant width = m_controller->getProperty(QStringLiteral("video-params/w"));
    const QVariant height = m_controller->getProperty(QStringLiteral("video-params/h"));
    if (width.isValid() && height.isValid())
    {
        resolution = QStringLiteral("%1x%2")
                         .arg(width.toLongLong())
                         .arg(height.toLongLong());
    }

    lines << QString();
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
