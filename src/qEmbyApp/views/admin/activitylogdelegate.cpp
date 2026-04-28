#include "activitylogdelegate.h"
#include "../../managers/thememanager.h"

#include <QPainter>
#include <QDateTime>
#include <QApplication>
#include <QStyle>
#include <QTimeZone>


ActivityLogThemeHelper::ActivityLogThemeHelper(QWidget *parent)
    : QWidget(parent),
      m_cardBg(51, 65, 85, 102),
      m_borderColor(71, 85, 105, 77),
      m_nameColor(241, 245, 249),
      m_overviewColor(148, 163, 184),
      m_timeColor(100, 116, 139)
{
    hide();
}


ActivityLogDelegate::ActivityLogDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void ActivityLogDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    
    if (!m_themeHelper && option.widget) {
        m_themeHelper = new ActivityLogThemeHelper(const_cast<QWidget*>(option.widget));
        m_themeHelper->setObjectName("activity-log-theme");
        option.widget->style()->polish(m_themeHelper);
    }

    
    QColor cardBg       = m_themeHelper ? m_themeHelper->cardBg()       : QColor(51, 65, 85, 102);
    QColor borderColor  = m_themeHelper ? m_themeHelper->borderColor()  : QColor(71, 85, 105, 77);
    QColor nameColor    = m_themeHelper ? m_themeHelper->nameColor()    : QColor(241, 245, 249);
    QColor overviewColor= m_themeHelper ? m_themeHelper->overviewColor(): QColor(148, 163, 184);
    QColor timeColor    = m_themeHelper ? m_themeHelper->timeColor()    : QColor(100, 116, 139);

    bool isDark = ThemeManager::instance()->isDarkMode();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    
    QRect rect = option.rect.adjusted(12, 4, -12, -4);

    
    if (option.state & QStyle::State_MouseOver) {
        cardBg = cardBg.lighter(isDark ? 120 : 97);
    }

    painter->setPen(QPen(borderColor, 1));
    painter->setBrush(cardBg);
    painter->drawRoundedRect(rect, 8, 8);

    int leftPad = 12;
    int rightPad = 12;
    int topPad = 8;

    
    QString name     = index.data(NameRole).toString();
    QString overview = index.data(OverviewRole).toString();
    QString date     = index.data(DateRole).toString();
    QString severity = index.data(SeverityRole).toString();

    int contentLeft = rect.left() + leftPad;
    int contentRight = rect.right() - rightPad;
    int contentWidth = contentRight - contentLeft;

    
    QString timeStr = formatExactTime(date);
    QFont timeFont = option.font;
    timeFont.setPixelSize(11);
    timeFont.setFamilies({"Segoe UI", "Microsoft YaHei", "sans-serif"});
    QFontMetrics timeFm(timeFont);
    int timeWidth = timeFm.horizontalAdvance(timeStr) + 4;

    painter->setFont(timeFont);
    painter->setPen(timeColor);
    painter->drawText(QRect(contentRight - timeWidth, rect.top() + topPad, timeWidth, 20),
                      Qt::AlignRight | Qt::AlignVCenter, timeStr);

    
    QColor sevColor = severityColor(severity);
    int dotSize = 6;
    int dotX = contentLeft + 1;
    int dotY = rect.top() + topPad + 7;
    painter->setPen(Qt::NoPen);
    painter->setBrush(sevColor);
    painter->drawEllipse(dotX, dotY, dotSize, dotSize);

    int textLeft = contentLeft + dotSize + 10;
    int nameMaxWidth = contentRight - timeWidth - textLeft - 12;

    
    QFont nameFont = option.font;
    nameFont.setPixelSize(12);
    nameFont.setWeight(QFont::Normal);
    nameFont.setFamilies({"Segoe UI", "Microsoft YaHei", "sans-serif"});
    QFontMetrics nameFm(nameFont);
    QString elidedName = nameFm.elidedText(name, Qt::ElideRight, nameMaxWidth);

    painter->setFont(nameFont);
    painter->setPen(nameColor);
    painter->drawText(QRect(textLeft, rect.top() + topPad, nameMaxWidth, 20),
                      Qt::AlignLeft | Qt::AlignVCenter, elidedName);

    
    if (!overview.isEmpty()) {
        QFont overviewFont = option.font;
        overviewFont.setPixelSize(11);
        overviewFont.setFamilies({"Segoe UI", "Microsoft YaHei", "sans-serif"});
        QFontMetrics overviewFm(overviewFont);
        QString elidedOverview = overviewFm.elidedText(overview, Qt::ElideRight, contentWidth - dotSize - 10);

        painter->setFont(overviewFont);
        painter->setPen(overviewColor);
        painter->drawText(QRect(textLeft, rect.top() + topPad + 22, contentWidth - dotSize - 10, 18),
                          Qt::AlignLeft | Qt::AlignVCenter, elidedOverview);
    }

    painter->restore();
}

QSize ActivityLogDelegate::sizeHint(const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const
{
    Q_UNUSED(option)
    QString overview = index.data(OverviewRole).toString();
    
    return QSize(200, overview.isEmpty() ? 44 : 56);
}

QString ActivityLogDelegate::formatExactTime(const QString& isoDate) const
{
    if (isoDate.isEmpty()) return {};

    QDateTime dt = QDateTime::fromString(isoDate, Qt::ISODate);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(isoDate, "yyyy-MM-ddTHH:mm:ss.zzzzzzzZ");
    }
    if (!dt.isValid()) return isoDate;

    dt.setTimeZone(QTimeZone::UTC);
    return dt.toLocalTime().toString("yyyy-MM-dd HH:mm:ss");
}

QColor ActivityLogDelegate::severityColor(const QString& severity) const
{
    if (severity.compare("Error", Qt::CaseInsensitive) == 0)
        return QColor("#EF4444");
    if (severity.compare("Warn", Qt::CaseInsensitive) == 0 ||
        severity.compare("Warning", Qt::CaseInsensitive) == 0)
        return QColor("#F59E0B");
    return QColor("#3B82F6"); 
}
