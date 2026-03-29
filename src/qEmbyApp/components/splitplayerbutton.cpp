#include "splitplayerbutton.h"
#include "../managers/externalplayerdetector.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QWidgetAction>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyle>



SplitPlayerButton::SplitPlayerButton(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(36);

    m_menu = new QMenu(this);
    m_menu->setObjectName("immersive-stream-menu");
    
    m_menu->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    m_menu->setAttribute(Qt::WA_TranslucentBackground, true);
    m_menu->setAutoFillBackground(false);
}

void SplitPlayerButton::setPlayers(const QList<DetectedPlayer> &players,
                                    const QString &activePlayerPath)
{
    m_menu->clear();

    if (players.isEmpty()) {
        m_activeIcon = QIcon();
        m_activeText.clear();
        m_activePath.clear();
        m_hasMultiple = false;
        hide();
        return;
    }

    QFileIconProvider iconProvider;
    m_activePath = activePlayerPath;
    m_hasMultiple = (players.size() > 1);

    
    int activeIdx = 0;
    for (int i = 0; i < players.size(); ++i) {
        if (players[i].path == activePlayerPath) {
            activeIdx = i;
            break;
        }
    }
    m_activeIcon = iconProvider.icon(QFileInfo(players[activeIdx].path));
    m_activeText = players[activeIdx].name;

    
    for (const auto &player : players) {
        QIcon playerIcon = iconProvider.icon(QFileInfo(player.path));
        bool isActive = (player.path == activePlayerPath);
        QString title = isActive ? QString("★ %1").arg(player.name) : player.name;

        
        auto *widgetAction = new QWidgetAction(m_menu);

        auto *container = new QWidget();
        container->setObjectName("stream-menu-item");
        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(14, 6, 28, 6);
        layout->setSpacing(10);

        
        auto *iconLabel = new QLabel(container);
        iconLabel->setFixedSize(20, 20);
        iconLabel->setPixmap(playerIcon.pixmap(20, 20));
        iconLabel->setStyleSheet("background: transparent;");

        
        auto *textContainer = new QWidget(container);
        auto *textLayout = new QVBoxLayout(textContainer);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);

        auto *line1 = new QLabel(title, textContainer);
        line1->setObjectName("stream-menu-line1");

        auto *line2 = new QLabel(player.path, textContainer);
        line2->setObjectName("stream-menu-line2");

        textLayout->addWidget(line1);
        textLayout->addWidget(line2);

        layout->addWidget(iconLabel);
        layout->addWidget(textContainer, 1);

        widgetAction->setDefaultWidget(container);
        widgetAction->setCheckable(true);
        widgetAction->setChecked(isActive);
        m_menu->addAction(widgetAction);

        QString path = player.path;
        connect(widgetAction, &QAction::triggered, this, [this, path]() {
            emit playerSelected(path);
        });
    }

    setToolTip(tr("Play with %1").arg(m_activeText));
    show();
    update();
}

void SplitPlayerButton::setIconOnly(bool iconOnly) {
    if (m_iconOnly == iconOnly) return;
    m_iconOnly = iconOnly;
    updateGeometry();
    update();
}

void SplitPlayerButton::clear() {
    m_menu->clear();
    m_activeIcon = QIcon();
    m_activeText.clear();
    m_activePath.clear();
    m_hasMultiple = false;
    update();
}

int SplitPlayerButton::arrowAreaWidth() const {
    return m_hasMultiple ? 28 : 0;
}

SplitPlayerButton::Zone SplitPlayerButton::zoneAt(const QPoint &pos) const {
    if (!rect().contains(pos)) return None;
    if (m_hasMultiple && pos.x() >= width() - arrowAreaWidth())
        return ArrowZone;
    return MainZone;
}

QSize SplitPlayerButton::sizeHint() const {
    if (m_iconOnly) {
        
        int w = 8 + 20 + 8;
        if (m_hasMultiple) w += arrowAreaWidth();
        return QSize(w, 36);
    }
    int textWidth = fontMetrics().horizontalAdvance(m_activeText);
    int w = 8 + 20 + 6 + textWidth + 8;
    if (m_hasMultiple) w += arrowAreaWidth();
    return QSize(qMax(w, 60), 36);
}



void SplitPlayerButton::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    int arrowW = arrowAreaWidth();
    QRectF mainRect(0, 0, width() - arrowW, height());
    QRectF arrowRect(width() - arrowW, 0, arrowW, height());
    QRectF fullRect(0, 0, width(), height());

    
    QPainterPath bgPath;
    bgPath.addRoundedRect(fullRect.adjusted(0.5, 0.5, -0.5, -0.5),
                          m_borderRadius, m_borderRadius);

    QColor mainBg = Qt::transparent;
    QColor arrowBg = Qt::transparent;

    if (m_pressedZone == MainZone)       mainBg  = m_pressedColor;
    else if (m_hoverZone == MainZone)    mainBg  = m_hoverColor;

    if (m_pressedZone == ArrowZone)      arrowBg = m_pressedColor;
    else if (m_hoverZone == ArrowZone)   arrowBg = m_hoverColor;

    if (m_hasMultiple) {
        p.save();
        p.setClipPath(bgPath);
        if (mainBg != Qt::transparent)
            p.fillRect(mainRect, mainBg);
        if (arrowBg != Qt::transparent)
            p.fillRect(arrowRect, arrowBg);
        p.restore();
    } else {
        if (mainBg != Qt::transparent) {
            p.save();
            p.setClipPath(bgPath);
            p.fillRect(fullRect, mainBg);
            p.restore();
        }
    }

    
    int iconSize = 20;
    int iconX = 8;
    int iconY = (height() - iconSize) / 2;
    m_activeIcon.paint(&p, iconX, iconY, iconSize, iconSize);

    
    if (!m_iconOnly) {
        int textX = iconX + iconSize + 6;
        int textW = width() - textX - arrowW - 4;
        if (textW > 0) {
            p.setPen(m_textColor);
            QFont f = font();
            f.setPointSizeF(9.5);
            p.setFont(f);
            QString elidedText = p.fontMetrics().elidedText(
                m_activeText, Qt::ElideRight, textW);
            p.drawText(QRectF(textX, 0, textW, height()),
                       Qt::AlignVCenter | Qt::AlignLeft, elidedText);
        }
    }

    
    if (m_hasMultiple) {
        p.setPen(m_arrowColor);
        QFont arrowFont = font();
        arrowFont.setPointSizeF(9);
        p.setFont(arrowFont);
        p.drawText(arrowRect, Qt::AlignCenter, "⯆");
    }
}



void SplitPlayerButton::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton)
        m_pressedZone = zoneAt(e->pos());
    update();
}

void SplitPlayerButton::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        Zone released = zoneAt(e->pos());
        if (released == m_pressedZone) {
            if (released == MainZone && !m_activePath.isEmpty()) {
                emit playRequested(m_activePath);
            } else if (released == ArrowZone) {
                showPlayerMenu();
            }
        }
    }
    m_pressedZone = None;
    update();
}

void SplitPlayerButton::mouseMoveEvent(QMouseEvent *e) {
    Zone newZone = zoneAt(e->pos());
    if (newZone != m_hoverZone) {
        m_hoverZone = newZone;
        update();
    }
}

void SplitPlayerButton::enterEvent(QEnterEvent *) {
}

void SplitPlayerButton::leaveEvent(QEvent *) {
    m_hoverZone = None;
    m_pressedZone = None;
    update();
}

void SplitPlayerButton::showPlayerMenu() {
    
    m_menu->adjustSize();
    QPoint pos = mapToGlobal(QPoint(width() / 2, height() + 4));
    pos.setX(pos.x() - m_menu->sizeHint().width() / 2);
    m_menu->exec(pos);
}
