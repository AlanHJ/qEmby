#include "searchcompleterpopup.h"

#include "../managers/thememanager.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QFontMetrics>
#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <utility>

namespace
{

constexpr int kPopupItemHeight = 40;
constexpr int kPopupItemHorizontalInset = 4;
constexpr int kPopupItemVerticalInset = 3;
constexpr int kPopupItemRadius = 13;
constexpr int kPopupItemIconBubbleSize = 22;
constexpr int kPopupItemIconSize = 14;
constexpr int kPopupItemLeftPadding = 12;
constexpr int kPopupItemRightPadding = 12;
constexpr int kPopupItemContentGap = 10;
constexpr int kPopupPanelChromeHeight = 2;

struct PopupItemPalette
{
    QColor backgroundTop;
    QColor backgroundBottom;
    QColor border;
    QColor text;
    QColor textHighlight;
    QColor iconBubble;
    QColor icon;
};

PopupItemPalette popupItemPalette(bool darkMode, bool hovered, bool selected)
{
    if (darkMode)
    {
        if (selected)
        {
            return {
                QColor(QStringLiteral("#163356")),
                QColor(QStringLiteral("#102B49")),
                QColor(QStringLiteral("#3C73A7")),
                QColor(QStringLiteral("#F8FAFC")),
                QColor(QStringLiteral("#BFDBFE")),
                QColor(QStringLiteral("#1D446C")),
                QColor(QStringLiteral("#BFDBFE"))};
        }

        if (hovered)
        {
            return {
                QColor(QStringLiteral("#162234")),
                QColor(QStringLiteral("#101B2C")),
                QColor(QStringLiteral("#28486B")),
                QColor(QStringLiteral("#E6EEF8")),
                QColor(QStringLiteral("#93C5FD")),
                QColor(QStringLiteral("#17314E")),
                QColor(QStringLiteral("#93C5FD"))};
        }

        return {
            QColor(QStringLiteral("#111B2B")),
            QColor(QStringLiteral("#0D1726")),
            QColor(QStringLiteral("#1D2B3D")),
            QColor(QStringLiteral("#CBD5E1")),
            QColor(QStringLiteral("#93C5FD")),
            QColor(QStringLiteral("#162233")),
            QColor(QStringLiteral("#7F93AA"))};
    }

    if (selected)
    {
        return {
            QColor(QStringLiteral("#EAF2FF")),
            QColor(QStringLiteral("#E2EEFF")),
            QColor(QStringLiteral("#C6D9F6")),
            QColor(QStringLiteral("#1E40AF")),
            QColor(QStringLiteral("#1D4ED8")),
            QColor(QStringLiteral("#DDE9FF")),
            QColor(QStringLiteral("#1D4ED8"))};
    }

    if (hovered)
    {
        return {
            QColor(QStringLiteral("#F6FAFF")),
            QColor(QStringLiteral("#EEF5FF")),
            QColor(QStringLiteral("#D8E5F7")),
            QColor(QStringLiteral("#284675")),
            QColor(QStringLiteral("#2563EB")),
            QColor(QStringLiteral("#EAF2FF")),
            QColor(QStringLiteral("#2563EB"))};
    }

    return {
        QColor(QStringLiteral("#F8FBFE")),
        QColor(QStringLiteral("#F3F7FA")),
        QColor(QStringLiteral("#E4EBF3")),
        QColor(QStringLiteral("#334155")),
        QColor(QStringLiteral("#2563EB")),
        QColor(QStringLiteral("#EFF4FA")),
        QColor(QStringLiteral("#7A8CA3"))};
}

class SearchCompleterItemDelegate : public QStyledItemDelegate
{
public:
    explicit SearchCompleterItemDelegate(const SearchCompleterPopup *popup,
                                         QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_popup(popup)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(0, kPopupItemHeight);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        if (!painter)
        {
            return;
        }

        const QString text = index.data(Qt::DisplayRole).toString().trimmed();
        if (text.isEmpty())
        {
            return;
        }

        const bool darkMode = ThemeManager::instance()->isDarkMode();
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const PopupItemPalette palette =
            popupItemPalette(darkMode, hovered, selected);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRect itemRect =
            option.rect.adjusted(kPopupItemHorizontalInset,
                                 kPopupItemVerticalInset,
                                 -kPopupItemHorizontalInset,
                                 -kPopupItemVerticalInset);
        const QRectF cardRect =
            QRectF(itemRect).adjusted(0.5, 0.5, -0.5, -0.5);

        QPainterPath cardPath;
        cardPath.addRoundedRect(cardRect, kPopupItemRadius, kPopupItemRadius);

        QLinearGradient backgroundGradient(0, cardRect.top(), 0,
                                           cardRect.bottom());
        backgroundGradient.setColorAt(0.0, palette.backgroundTop);
        backgroundGradient.setColorAt(1.0, palette.backgroundBottom);
        painter->fillPath(cardPath, backgroundGradient);
        painter->setPen(QPen(palette.border, 0.9));
        painter->drawPath(cardPath);

        const int iconBubbleY =
            itemRect.top() + (itemRect.height() - kPopupItemIconBubbleSize) / 2;
        const QRect iconBubbleRect(itemRect.left() + kPopupItemLeftPadding,
                                   iconBubbleY,
                                   kPopupItemIconBubbleSize,
                                   kPopupItemIconBubbleSize);
        QPainterPath iconBubblePath;
        iconBubblePath.addEllipse(QRectF(iconBubbleRect).adjusted(
            0.5, 0.5, -0.5, -0.5));
        painter->fillPath(iconBubblePath, palette.iconBubble);

        const QRect iconRect(
            iconBubbleRect.left() +
                (iconBubbleRect.width() - kPopupItemIconSize) / 2,
            iconBubbleRect.top() +
                (iconBubbleRect.height() - kPopupItemIconSize) / 2,
            kPopupItemIconSize, kPopupItemIconSize);
        ThemeManager::getAdaptiveIcon(
            QStringLiteral(":/svg/dark/search.svg"), palette.icon)
            .paint(painter, iconRect);

        const QRect textRect(
            iconBubbleRect.right() + kPopupItemContentGap,
            itemRect.top(),
            qMax(1, itemRect.right() - kPopupItemRightPadding -
                         (iconBubbleRect.right() + kPopupItemContentGap) + 1),
            itemRect.height());

        QFont textFont = option.font;
        textFont.setPointSizeF(9.75);
        textFont.setWeight(QFont::Medium);

        QFont highlightFont = textFont;
        highlightFont.setWeight(QFont::DemiBold);

        const QFontMetrics textMetrics(textFont);
        const QString displayText =
            textMetrics.elidedText(text, Qt::ElideRight, textRect.width());
        const QString highlightText =
            m_popup ? m_popup->highlightText().trimmed() : QString();
        const int highlightPos =
            highlightText.isEmpty()
                ? -1
                : text.indexOf(highlightText, 0, Qt::CaseInsensitive);

        if (displayText != text || highlightPos < 0)
        {
            painter->setPen(palette.text);
            painter->setFont(textFont);
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                              displayText);
            painter->restore();
            return;
        }

        const QString prefixText = text.left(highlightPos);
        const QString matchedText = text.mid(highlightPos,
                                             highlightText.size());
        const QString suffixText =
            text.mid(highlightPos + highlightText.size());

        const QFontMetrics normalMetrics(textFont);
        const QFontMetrics matchedMetrics(highlightFont);
        const int prefixWidth = normalMetrics.horizontalAdvance(prefixText);
        const int matchedWidth =
            matchedMetrics.horizontalAdvance(matchedText);
        const int totalTextWidth = prefixWidth + matchedWidth +
                                   normalMetrics.horizontalAdvance(suffixText);
        int drawX = textRect.left();
        if (totalTextWidth < textRect.width())
        {
            drawX += 0;
        }

        const int baselineY =
            textRect.top() + (textRect.height() + normalMetrics.ascent() -
                              normalMetrics.descent()) /
                                 2;

        painter->setPen(palette.text);
        painter->setFont(textFont);
        painter->drawText(drawX, baselineY, prefixText);
        drawX += prefixWidth;

        painter->setPen(palette.textHighlight);
        painter->setFont(highlightFont);
        painter->drawText(drawX, baselineY, matchedText);
        drawX += matchedWidth;

        painter->setPen(palette.text);
        painter->setFont(textFont);
        painter->drawText(drawX, baselineY, suffixText);
        painter->restore();
    }

private:
    const SearchCompleterPopup *m_popup = nullptr;
};

} 

SearchCompleterPopup::SearchCompleterPopup(QWidget *parent)
    : QListView(parent)
{
    setObjectName(QStringLiteral("SearchCompleterPopup"));
    setViewMode(QListView::ListMode);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setMouseTracking(true);
    setSpacing(0);
    setUniformItemSizes(true);
    setAlternatingRowColors(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setTextElideMode(Qt::ElideRight);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setFrameShape(QFrame::NoFrame);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAutoFillBackground(false);
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint |
                   Qt::NoDropShadowWindowHint);

    if (viewport())
    {
        viewport()->setAttribute(Qt::WA_Hover, true);
        viewport()->setMouseTracking(true);
    }

    if (auto *bar = verticalScrollBar())
    {
        bar->setSingleStep(16);
    }

    m_itemDelegate = new SearchCompleterItemDelegate(this, this);
    setItemDelegate(m_itemDelegate);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](ThemeManager::Theme) {
                if (viewport())
                {
                    viewport()->update();
                }
            });
}

void SearchCompleterPopup::setHighlightText(QString text)
{
    text = text.trimmed();
    if (m_highlightText == text)
    {
        return;
    }

    m_highlightText = std::move(text);
    if (viewport())
    {
        viewport()->update();
    }
}

QString SearchCompleterPopup::highlightText() const
{
    return m_highlightText;
}

void SearchCompleterPopup::setModel(QAbstractItemModel *model)
{
    QListView::setModel(model);
    ensureDelegate();
}

void SearchCompleterPopup::showEvent(QShowEvent *event)
{
    ensureDelegate();
    QListView::showEvent(event);
}

void SearchCompleterPopup::syncWidthToAnchor(const QWidget *anchor)
{
    if (!anchor)
    {
        return;
    }

    setFixedSize(anchor->width(), desiredPopupHeight());
}

void SearchCompleterPopup::setMaxVisibleRows(int rows)
{
    m_maxVisibleRows = qMax(1, rows);
}

QRect SearchCompleterPopup::popupRectForAnchor(const QWidget *anchor,
                                               int verticalOffset) const
{
    if (!anchor)
    {
        return {};
    }

    return QRect(0, anchor->height() + verticalOffset, anchor->width(), 1);
}

int SearchCompleterPopup::sizeHintForRow(int row) const
{
    Q_UNUSED(row);
    return kPopupItemHeight;
}

int SearchCompleterPopup::desiredPopupHeight() const
{
    const int rowCount = model() ? model()->rowCount(rootIndex()) : 0;
    const int visibleRows = qMax(1, qMin(m_maxVisibleRows, rowCount));
    return visibleRows * kPopupItemHeight + kPopupPanelChromeHeight;
}

void SearchCompleterPopup::ensureDelegate()
{
    if (!m_itemDelegate)
    {
        m_itemDelegate = new SearchCompleterItemDelegate(this, this);
    }

    if (itemDelegate() != m_itemDelegate)
    {
        setItemDelegate(m_itemDelegate);
    }
}
