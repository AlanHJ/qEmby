#include "playerstatisticsoverlay.h"

#include <QPainter>
#include <QPaintEvent>
#include <QTextLayout>
#include <QTextOption>
#include <cmath>

namespace
{
void configureWrappedLayout(QTextLayout *layout,
                            const QString &text,
                            const QFont &font,
                            qreal width)
{
    if (!layout)
    {
        return;
    }

    layout->clearLayout();
    layout->setText(text);
    layout->setFont(font);
    QTextOption option(Qt::AlignLeft | Qt::AlignTop);
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout->setTextOption(option);

    layout->beginLayout();
    qreal y = 0.0;
    while (true)
    {
        QTextLine line = layout->createLine();
        if (!line.isValid())
        {
            break;
        }
        line.setLineWidth(width);
        line.setPosition(QPointF(0.0, y));
        y += line.height();
    }
    layout->endLayout();
}

qreal layoutHeight(const QTextLayout &layout)
{
    qreal height = 0.0;
    for (int i = 0; i < layout.lineCount(); ++i)
    {
        const QTextLine line = layout.lineAt(i);
        height = qMax(height, line.y() + line.height());
    }
    return height;
}
} 

PlayerStatisticsOverlay::PlayerStatisticsOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QFont overlayFont(QStringLiteral("Microsoft YaHei UI"));
    overlayFont.setPointSize(10);
    overlayFont.setWeight(QFont::DemiBold);
    setFont(overlayFont);
}

void PlayerStatisticsOverlay::setLines(const QStringList &lines)
{
    if (m_lines == lines)
    {
        return;
    }

    m_lines = lines;
    updateGeometry();
    update();
}

void PlayerStatisticsOverlay::setTextColor(const QColor &color)
{
    if (m_textColor == color)
    {
        return;
    }

    m_textColor = color;
    update();
}

void PlayerStatisticsOverlay::setShadowColor(const QColor &color)
{
    if (m_shadowColor == color)
    {
        return;
    }

    m_shadowColor = color;
    update();
}

QSize PlayerStatisticsOverlay::sizeHint() const
{
    const int targetWidth = width() > 0 ? width() : 520;
    const qreal contentWidth = qMax<qreal>(160.0, targetWidth - (m_horizontalPadding * 2));

    qreal totalHeight = m_verticalPadding * 2;
    bool hadVisibleLine = false;

    for (const QString &line : m_lines)
    {
        if (line.trimmed().isEmpty())
        {
            if (hadVisibleLine)
            {
                totalHeight += m_sectionSpacing;
            }
            continue;
        }

        totalHeight += textHeightForWidth(line, contentWidth);
        totalHeight += m_rowSpacing;
        hadVisibleLine = true;
    }

    if (hadVisibleLine)
    {
        totalHeight -= m_rowSpacing;
    }

    return QSize(targetWidth, qMax(0, static_cast<int>(std::ceil(totalHeight))));
}

QSize PlayerStatisticsOverlay::minimumSizeHint() const
{
    return sizeHint();
}

void PlayerStatisticsOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal contentWidth = qMax<qreal>(160.0, width() - (m_horizontalPadding * 2));
    qreal y = m_verticalPadding;
    bool hadVisibleLine = false;

    for (const QString &line : m_lines)
    {
        if (line.trimmed().isEmpty())
        {
            if (hadVisibleLine)
            {
                y += m_sectionSpacing;
            }
            continue;
        }

        const qreal blockHeight = textHeightForWidth(line, contentWidth);
        const QPointF origin(m_horizontalPadding, y);

        static const QList<QPointF> shadowOffsets = {
            QPointF(-1.0, 0.0), QPointF(1.0, 0.0),  QPointF(0.0, -1.0),
            QPointF(0.0, 1.0),  QPointF(-1.0, -1.0), QPointF(1.0, -1.0),
            QPointF(-1.0, 1.0), QPointF(1.0, 1.0)};

        for (const QPointF &offset : shadowOffsets)
        {
            drawTextBlock(&painter, line, origin + offset, contentWidth, m_shadowColor);
        }

        drawTextBlock(&painter, line, origin, contentWidth, m_textColor);

        y += blockHeight + m_rowSpacing;
        hadVisibleLine = true;
    }
}

qreal PlayerStatisticsOverlay::textHeightForWidth(const QString &text, qreal width) const
{
    QTextLayout layout;
    configureWrappedLayout(&layout, text, font(), width);
    return layoutHeight(layout);
}

void PlayerStatisticsOverlay::drawTextBlock(QPainter *painter,
                                            const QString &text,
                                            const QPointF &topLeft,
                                            qreal width,
                                            const QColor &color) const
{
    QTextLayout layout;
    configureWrappedLayout(&layout, text, font(), width);

    painter->save();
    painter->setPen(color);
    layout.draw(painter, topLeft);
    painter->restore();
}
