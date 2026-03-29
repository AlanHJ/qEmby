#include "serverwheelview.h"
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDateTime>
#include <QMap>
#include <QtMath>
#include <QLabel>
#include <QPushButton>
#include <QFont>

ServerWheelView::ServerWheelView(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("ServerWheelView { background: transparent; }");

    m_scrollAnim = new QVariantAnimation(this);
    
    m_scrollAnim->setEasingCurve(QEasingCurve::OutExpo);
    m_scrollAnim->setDuration(450);
    connect(m_scrollAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val){
        m_scrollOffset = val.toReal();
        updateLayout();
    });
}

ServerWheelView::~ServerWheelView() {
    clear();
}

void ServerWheelView::addCard(QWidget* card) {
    card->setParent(this);
    auto* effect = new QGraphicsOpacityEffect(card);
    card->setGraphicsEffect(effect);

    installFilterRecursively(card);
    m_items.append(card);

    updateLayout();
}

void ServerWheelView::clear() {
    m_scrollAnim->stop();
    m_scrollOffset = 0.0;
    m_currentIndex = 0;
    qDeleteAll(m_items);
    m_items.clear();
}

int ServerWheelView::count() const {
    return m_items.size();
}

void ServerWheelView::setCurrentIndex(int index, bool animated) {
    int N = m_items.size();
    if (N == 0) return;

    index = index % N;
    if (index < 0) index += N;

    if (animated) {
        qreal delta = index - m_scrollOffset;
        qreal wrapped_delta = delta - N * qRound(delta / static_cast<qreal>(N));
        scrollTo(m_scrollOffset + wrapped_delta);
    } else {
        m_scrollAnim->stop();
        m_scrollOffset = index;
        updateLayout();
    }
}

int ServerWheelView::currentIndex() const {
    return m_currentIndex;
}

void ServerWheelView::setTransitionMode(bool active) {
    m_isTransitioning = active;
    if (active) {
        m_scrollAnim->stop();
        m_scrollOffset = m_currentIndex;
    }
    updateLayout();
}

void ServerWheelView::resizeEvent(QResizeEvent*) {
    updateLayout();
}

void ServerWheelView::handleMousePress(QPoint globalPos) {
    if (m_items.isEmpty()) return;
    m_scrollAnim->stop();
    m_isDragging = true;
    m_dragStarted = false;
    m_lastPos = globalPos;
    m_dragVelocity = 0.0;
    m_lastTime = QDateTime::currentMSecsSinceEpoch();
}

void ServerWheelView::handleMouseMove(QPoint globalPos) {
    if (!m_isDragging) return;
    qreal dy = globalPos.y() - m_lastPos.y();
    if (!m_dragStarted && qAbs(dy) > 5) m_dragStarted = true;
    if (m_dragStarted) {
        qreal deltaOffset = -dy / 80.0;
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 dt = now - m_lastTime;
        if (dt > 0) m_dragVelocity = deltaOffset / dt * 16.0;

        m_scrollOffset += deltaOffset;

        m_lastPos = globalPos;
        m_lastTime = now;
        updateLayout();
    }
}

bool ServerWheelView::handleMouseRelease() {
    if (!m_isDragging) return false;
    m_isDragging = false;
    if (m_dragStarted) {
        if (!m_items.isEmpty()) {
            qreal target = qRound(m_scrollOffset + m_dragVelocity * 10.0);
            scrollTo(target);
        }
        return true;
    }
    return false;
}

void ServerWheelView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) handleMousePress(event->globalPosition().toPoint());
    QWidget::mousePressEvent(event);
}

void ServerWheelView::mouseMoveEvent(QMouseEvent* event) {
    handleMouseMove(event->globalPosition().toPoint());
    QWidget::mouseMoveEvent(event);
}

void ServerWheelView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) handleMouseRelease();
    QWidget::mouseReleaseEvent(event);
}

bool ServerWheelView::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) handleMousePress(me->globalPosition().toPoint());
    }
    else if (event->type() == QEvent::MouseMove) {
        auto* me = static_cast<QMouseEvent*>(event);
        handleMouseMove(me->globalPosition().toPoint());
        if (m_dragStarted) return true;
    }
    else if (event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            if (handleMouseRelease()) return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ServerWheelView::wheelEvent(QWheelEvent* event) {
    if (m_items.isEmpty()) return;

    int step = event->angleDelta().y() > 0 ? -1 : 1;

    
    qreal baseTarget = m_scrollAnim->state() == QAbstractAnimation::Running ?
                           m_scrollAnim->endValue().toReal() : qRound(m_scrollOffset);

    scrollTo(baseTarget + step);
}

void ServerWheelView::installFilterRecursively(QWidget* w) {
    w->installEventFilter(this);
    for (auto* child : w->findChildren<QWidget*>()) {
        child->installEventFilter(this);
    }
}

void ServerWheelView::scrollTo(qreal target) {
    m_scrollAnim->stop();
    m_scrollAnim->setStartValue(m_scrollOffset);
    m_scrollAnim->setEndValue(target);
    m_scrollAnim->start();
}

void ServerWheelView::updateLayout() {
    int N = m_items.size();
    if (N == 0) return;

    int vh = height();
    int vw = width();
    int centerY = vh / 2;
    int baseItemHeight = 72;

    QMap<qreal, QWidget*> zOrderMap;

    int newIndex = qRound(m_scrollOffset) % N;
    if (newIndex < 0) newIndex += N;

    if (newIndex != m_currentIndex) {
        m_currentIndex = newIndex;
        Q_EMIT currentIndexChanged(m_currentIndex);
    }

    for (int i = 0; i < N; ++i) {
        QWidget* card = m_items[i];

        qreal rawDelta = i - m_scrollOffset;
        qreal logicalIdx = rawDelta - N * qRound(rawDelta / static_cast<qreal>(N));
        qreal distance = qAbs(logicalIdx);

        qreal opacity = qMax(0.0, 1.0 - distance * 0.4);
        bool shouldHide = (opacity <= 0.0 || (m_isTransitioning && i != m_currentIndex));

        
        if (shouldHide) {
            if (card->isVisible()) card->hide();
            continue;
        } else {
            if (!card->isVisible()) card->show();
        }

        if (card->testAttribute(Qt::WA_TransparentForMouseEvents)) {
            card->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        }

        bool isDepthCard = (distance > 1.5);
        if (card->property("isDepthCard").toBool() != isDepthCard) {
            card->setProperty("isDepthCard", isDepthCard);
        }

        
        if (isDepthCard) {
            QWidget* actionContainer = nullptr;
            if (card->property("cachedActionPtr").isValid()) {
                actionContainer = reinterpret_cast<QWidget*>(card->property("cachedActionPtr").toULongLong());
            } else {
                for (auto* btn : card->findChildren<QPushButton*>()) {
                    if (btn->objectName() == "action-edit-btn" && btn->parentWidget()) {
                        actionContainer = btn->parentWidget();
                        card->setProperty("cachedActionPtr", reinterpret_cast<qulonglong>(actionContainer));
                        break;
                    }
                }
            }
            if (actionContainer && actionContainer->isVisible()) {
                actionContainer->hide();
            }
        }

        qreal angle = logicalIdx * (M_PI / 6.0);
        qreal R = vh / 2.0;
        qreal scale = qMax(0.5, 1.0 - distance * 0.2);

        int projectedW = vw * scale;
        int projectedH = baseItemHeight * scale;
        int projectedX = (vw - projectedW) / 2;
        int projectedY = centerY + R * qSin(angle) - projectedH / 2.0;

        
        QRect newGeometry(projectedX, projectedY, projectedW, projectedH);
        if (card->geometry() != newGeometry) {
            card->setGeometry(newGeometry);
        }

        for (auto* child : card->findChildren<QWidget*>()) {

            if (qobject_cast<QLabel*>(child) || qobject_cast<QPushButton*>(child)) {
                if (!child->property("originalFontSize").isValid()) {
                    QFont f = child->font();
                    qreal size = f.pixelSize() > 0 ? f.pixelSize() : f.pointSizeF();
                    child->setProperty("originalFontSize", size);
                    child->setProperty("usePixel", f.pixelSize() > 0);
                }

                qreal origSize = child->property("originalFontSize").toReal();
                bool usePixel = child->property("usePixel").toBool();
                int newSizeInt = qMax(1, qRound(origSize * scale));
                int lastSizeInt = child->property("lastAppliedFontSize").toInt();

                if (newSizeInt != lastSizeInt) {
                    child->setProperty("lastAppliedFontSize", newSizeInt);

                    QFont newFont = child->font();
                    if (usePixel) {
                        newFont.setPixelSize(newSizeInt);
                    } else {
                        newFont.setPointSizeF(newSizeInt);
                    }
                    child->setFont(newFont);
                }
            }

            if (QLabel* label = qobject_cast<QLabel*>(child)) {
                if (!label->property("originalIconSize").isValid() &&
                    label->minimumWidth() == label->maximumWidth() &&
                    label->minimumHeight() == label->maximumHeight() &&
                    label->minimumWidth() > 0) {
                    label->setProperty("originalIconSize", label->minimumWidth());
                }

                if (label->property("originalIconSize").isValid()) {
                    int origIconSize = label->property("originalIconSize").toInt();
                    int newSize = qMax(1, qRound(origIconSize * scale));
                    int lastIconSize = label->property("lastAppliedIconSize").toInt();

                    if (newSize != lastIconSize) {
                        label->setProperty("lastAppliedIconSize", newSize);
                        label->setFixedSize(newSize, newSize);
                    }
                }
            }
        }

        if (auto* effect = qobject_cast<QGraphicsOpacityEffect*>(card->graphicsEffect())) {
            
            if (!qFuzzyCompare(effect->opacity(), opacity)) {
                effect->setOpacity(opacity);
            }
            bool enableEffect = opacity < 0.99;
            if (effect->isEnabled() != enableEffect) {
                effect->setEnabled(enableEffect);
            }
        }

        zOrderMap.insert(-distance, card);
    }

    for (auto* card : zOrderMap.values()) {
        card->raise();
    }
}
