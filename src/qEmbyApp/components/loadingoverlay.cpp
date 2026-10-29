#include "loadingoverlay.h"
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QConicalGradient>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QHideEvent>
#include <QFontMetrics>
#include <QPalette>
#include <QRadialGradient>
#include <QTimer>
#include <QPushButton>
#include <QShortcut>
#include <QKeySequence>
#include <QGraphicsOpacityEffect>
#include <QResizeEvent>
#include <QtMath>

LoadingOverlay::LoadingOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);

    m_rotateAnim = new QPropertyAnimation(this, "angle", this);
    m_rotateAnim->setStartValue(0.0);
    m_rotateAnim->setEndValue(360.0);
    m_rotateAnim->setDuration(1000);
    m_rotateAnim->setLoopCount(-1);

    m_fadeAnim = new QPropertyAnimation(this, "opacity", this);
    m_fadeAnim->setDuration(300);
    m_fadeAnim->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_fadeAnim, &QPropertyAnimation::finished, this, [this]() {
        if (m_state == State::FadingOut)
            forceStop();
    });

    m_completionAnim = new QPropertyAnimation(this, "completion", this);
    m_completionAnim->setDuration(260);
    m_completionAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_completionAnim, &QPropertyAnimation::finished, this, [this]() {
        m_state = State::Complete;
        m_rotateAnim->stop();
        Q_EMIT completed();
    });

    m_revealTimer = new QTimer(this);
    m_revealTimer->setSingleShot(true);
    connect(m_revealTimer, &QTimer::timeout, this, &LoadingOverlay::reveal);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setObjectName("loading-cancel-button");
    m_cancelButton->setToolTip(tr("Cancel (Esc)"));
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    m_cancelOpacityEffect = new QGraphicsOpacityEffect(m_cancelButton);
    m_cancelOpacityEffect->setOpacity(0.0);
    m_cancelButton->setGraphicsEffect(m_cancelOpacityEffect);
    m_cancelShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_cancelShortcut->setContext(Qt::WindowShortcut);
    m_cancelShortcut->setAutoRepeat(false);
    const auto requestCancel = [this]() {
        if (m_cancellable && m_state == State::Loading)
            Q_EMIT cancelRequested();
    };
    connect(m_cancelShortcut, &QShortcut::activated, this, requestCancel);
    connect(m_cancelButton, &QPushButton::clicked, this, requestCancel);
    m_cancelRevealTimer = new QTimer(this);
    m_cancelRevealTimer->setSingleShot(true);
    m_cancelRevealTimer->setTimerType(Qt::PreciseTimer);
    connect(m_cancelRevealTimer, &QTimer::timeout,
            this, &LoadingOverlay::updateCancelControls);
    updateCancelControls();
    hide();
}

void LoadingOverlay::start() {
    if (m_state == State::Loading || m_state == State::Completing)
        return;

    m_revealTimer->stop();
    m_fadeAnim->stop();
    m_completionAnim->stop();
    m_state = State::Loading;
    m_loadingElapsed.start();
    m_completion = 0.0;
    show();
    raise();
    updateCancelControls();
    if (m_cancellable && m_cancelButtonDelayMs == 0)
        m_cancelButton->setFocus(Qt::OtherFocusReason);

    
    if (m_ambientMode && !m_reducedMotion && m_opacity == 0.0) {
        m_revealTimer->start(120);
    } else {
        reveal();
    }
}

void LoadingOverlay::reveal() {
    if (m_state != State::Loading)
        return;

    if (m_reducedMotion) {
        setOpacity(1.0);
        return;
    }

    if (m_rotateAnim->state() != QAbstractAnimation::Running)
        m_rotateAnim->start();

    m_fadeAnim->stop();
    m_fadeAnim->setDuration(m_ambientMode ? 280 : 300);
    m_fadeAnim->setEasingCurve(m_ambientMode ? QEasingCurve::OutCubic
                                          : QEasingCurve::InOutQuad);
    m_fadeAnim->setStartValue(m_opacity);
    m_fadeAnim->setEndValue(1.0);
    m_fadeAnim->start();
}

void LoadingOverlay::stop() {
    if (m_state == State::Idle || m_state == State::FadingOut)
        return;

    m_revealTimer->stop();
    m_completionAnim->stop();
    m_fadeAnim->stop();
    if (m_reducedMotion || m_opacity == 0.0 || !isVisible()) {
        forceStop();
        return;
    }

    m_state = State::FadingOut;
    updateCancelControls();
    m_fadeAnim->setDuration(m_ambientMode ? 180 : 300);
    m_fadeAnim->setEasingCurve(QEasingCurve::InOutQuad);
    m_fadeAnim->setStartValue(m_opacity);
    m_fadeAnim->setEndValue(0.0);
    m_fadeAnim->start();
}

void LoadingOverlay::forceStop() {
    m_state = State::Idle;
    updateCancelControls();
    m_revealTimer->stop();
    m_completionAnim->stop();
    m_fadeAnim->stop();
    m_rotateAnim->stop();
    m_opacity = 0.0;
    m_cancelOpacityEffect->setOpacity(0.0);
    m_completion = 0.0;
    hide();
    update();
}

void LoadingOverlay::hideEvent(QHideEvent* event) {
    
    if (m_ambientMode) {
        if (m_state == State::Completing) {
            m_fadeAnim->stop();
            m_rotateAnim->stop();
            setOpacity(1.0);
        } else {
            forceStop();
        }
    }
    QWidget::hideEvent(event);
}

void LoadingOverlay::setAmbientMode(bool ambient) {
    m_ambientMode = ambient;
    m_rotateAnim->setEndValue(ambient ? 720.0 : 360.0);
    m_rotateAnim->setDuration(ambient ? 2600 : 1000);
    update();
}

void LoadingOverlay::setReducedMotion(bool reduced) {
    m_reducedMotion = reduced;
    if (reduced)
        m_rotateAnim->stop();
}

void LoadingOverlay::setStatusText(const QString& text) {
    m_statusText = text;
    setAccessibleName(text);
    update();
}

void LoadingOverlay::setCancellable(bool cancellable, int buttonDelayMs) {
    m_cancellable = cancellable;
    m_cancelButtonDelayMs = qMax(0, buttonDelayMs);
    updateCancelControls();
}

void LoadingOverlay::updateCancelControls() {
    const bool available = m_cancellable && m_state == State::Loading;
    const qint64 remainingMs = available
        ? qMax<qint64>(0, m_cancelButtonDelayMs - m_loadingElapsed.elapsed())
        : 0;
    const bool showButton = available && remainingMs == 0;
    m_cancelButton->setVisible(showButton);
    m_cancelButton->setEnabled(showButton);
    m_cancelShortcut->setEnabled(available);
    if (available && remainingMs > 0)
        m_cancelRevealTimer->start(int(remainingMs));
    else
        m_cancelRevealTimer->stop();
    if (showButton)
        positionCancelButton();
}

void LoadingOverlay::positionCancelButton() {
    const QSize buttonSize = m_cancelButton->sizeHint().expandedTo(QSize(112, 32));
    const int y = qMax(0, qMin(height() / 2 + 48 + fontMetrics().height(),
                               height() - buttonSize.height() - 12));
    m_cancelButton->setGeometry((width() - buttonSize.width()) / 2, y,
                                buttonSize.width(), buttonSize.height());
}

void LoadingOverlay::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    positionCancelButton();
}

void LoadingOverlay::setCompletion(qreal progress) {
    m_completion = qBound(0.0, progress, 1.0);
    update();
}

void LoadingOverlay::complete() {
    if (m_state == State::Completing || m_state == State::Complete)
        return;

    m_revealTimer->stop();
    m_fadeAnim->stop();
    
    if (!m_ambientMode || m_reducedMotion || m_opacity < 0.05 || !isVisible()) {
        m_state = State::Complete;
        updateCancelControls();
        m_rotateAnim->stop();
        Q_EMIT completed();
        return;
    }

    m_state = State::Completing;
    updateCancelControls();
    m_fadeAnim->setDuration(180);
    m_fadeAnim->setStartValue(m_opacity);
    m_fadeAnim->setEndValue(1.0);
    m_fadeAnim->start();
    m_completionAnim->setStartValue(m_completion);
    m_completionAnim->setEndValue(1.0);
    m_completionAnim->start();
}

qreal LoadingOverlay::opacity() const { return m_opacity; }
void LoadingOverlay::setOpacity(qreal op) {
    m_opacity = op;
    if (m_cancelOpacityEffect)
        m_cancelOpacityEffect->setOpacity(op);
    update();
}

qreal LoadingOverlay::angle() const { return m_angle; }
void LoadingOverlay::setAngle(qreal a) { m_angle = a; update(); }

bool LoadingOverlay::isImmersive() const { return m_isImmersive; }
void LoadingOverlay::setImmersive(bool immersive) {
    m_isImmersive = immersive;
    update();
}

bool LoadingOverlay::isHudPanelVisible() const { return m_hudPanelVisible; }
void LoadingOverlay::setHudPanelVisible(bool visible) {
    if (m_hudPanelVisible == visible) {
        return;
    }

    m_hudPanelVisible = visible;
    update();
}

bool LoadingOverlay::isSubtleOverlay() const { return m_subtleOverlay; }
void LoadingOverlay::setSubtleOverlay(bool subtle) {
    if (m_subtleOverlay == subtle) {
        return;
    }

    m_subtleOverlay = subtle;
    update();
}

void LoadingOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacity);

    if (m_ambientMode) {
        paintAmbient(p);
        return;
    }

    
    
    if (m_isImmersive) {
        p.fillRect(rect(), QColor(0, 0, 0, 60));
    } else {
        QColor overlayColor = m_overlayBgColor;
        if (m_subtleOverlay) {
            overlayColor.setAlpha(qMax(8, qRound(overlayColor.alpha() * 0.35)));
        }
        p.fillRect(rect(), overlayColor);
    }

    p.translate(width() / 2.0, height() / 2.0);

    qreal radius = 24.0;
    qreal thickness = 3.5;

    
    if (!m_isImmersive && m_hudPanelVisible) {
        int hudSize = 90;
        QRectF hudRect(-hudSize / 2.0, -hudSize / 2.0, hudSize, hudSize);

        QPainterPath shadowPath;
        shadowPath.addRoundedRect(hudRect.translated(0, 8), 16, 16);
        p.fillPath(shadowPath, m_shadowColor);

        QPainterPath hudPath;
        hudPath.addRoundedRect(hudRect, 16, 16);

        QLinearGradient hudGrad(hudRect.topLeft(), hudRect.bottomRight());
        hudGrad.setColorAt(0.0, m_hudColorTop);
        hudGrad.setColorAt(0.5, m_hudColorMid);
        hudGrad.setColorAt(1.0, m_hudColorBottom);
        p.fillPath(hudPath, hudGrad);

        QLinearGradient borderGrad(hudRect.topLeft(), hudRect.bottomLeft());
        borderGrad.setColorAt(0.0, m_borderColor);
        QColor transparentBorder = m_borderColor;
        transparentBorder.setAlpha(qMax(0, m_borderColor.alpha() - 50));
        borderGrad.setColorAt(1.0, transparentBorder);

        QPen borderPen(QBrush(borderGrad), 1.0);
        p.setPen(borderPen);
        p.drawPath(hudPath);
    } else if (m_isImmersive) {
        
        radius = 36.0;
        thickness = 4.0;
    } else {
        
        radius = 30.0;
        thickness = 3.8;
    }

    QRectF ringRect(-radius, -radius, radius * 2, radius * 2);

    
    QColor currentTrackColor = m_isImmersive ? QColor(255, 255, 255, 30) : m_trackColor;
    QColor currentSweepColor = m_isImmersive ? QColor(255, 255, 255, 255) : m_sweepColor;

    QPen trackPen(currentTrackColor);
    trackPen.setWidthF(thickness);
    p.setPen(trackPen);
    p.drawEllipse(ringRect);

    
    QConicalGradient grad(0, 0, -m_angle);
    QColor sweepFull = currentSweepColor;
    QColor sweepMed = currentSweepColor;
    sweepMed.setAlpha(static_cast<int>(currentSweepColor.alpha() * 0.7));
    QColor sweepZero = currentSweepColor;
    sweepZero.setAlpha(0);

    grad.setColorAt(0.0, sweepFull);
    grad.setColorAt(0.1, sweepMed);
    grad.setColorAt(0.75, sweepZero);
    grad.setColorAt(1.0, sweepZero);

    QPen sweepPen(QBrush(grad), thickness);
    sweepPen.setCapStyle(Qt::RoundCap);
    p.setPen(sweepPen);
    p.drawArc(ringRect, qRound(-m_angle * 16.0), 280 * 16);
}

void LoadingOverlay::paintAmbient(QPainter& p) {
    
    QColor veil = m_overlayBgColor;
    veil.setAlphaF(veil.alphaF() + (1.0 - veil.alphaF()) * m_completion);
    p.fillRect(rect(), veil);

    const qreal phase = qDegreesToRadians(m_angle * 0.5);
    const qreal breath = m_reducedMotion ? 0.5 : (1.0 - qCos(phase)) * 0.5;
    const qreal lift = m_reducedMotion ? 0.0 : 6.0 * (1.0 - m_opacity);
    p.translate(width() / 2.0, height() / 2.0 - 16.0 + lift);

    const qreal glowRadius = 96.0 + 6.0 * breath;
    QColor glow = m_glowColor;
    glow.setAlphaF(glow.alphaF() * (0.82 + 0.18 * breath));
    QColor clearGlow = glow;
    clearGlow.setAlpha(0);
    QRadialGradient halo(QPointF(0, 0), glowRadius);
    halo.setColorAt(0.0, glow);
    halo.setColorAt(1.0, clearGlow);
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(QPointF(0, 0), glowRadius, glowRadius);

    p.save();
    const qreal scale = m_reducedMotion ? 1.0 : 0.96 + 0.04 * m_opacity;
    p.scale(scale, scale);
    p.setBrush(Qt::NoBrush);
    const QRectF ring(-25.0, -25.0, 50.0, 50.0);
    p.setPen(QPen(m_trackColor, 2.0));
    p.drawEllipse(ring);

    
    p.setOpacity(m_opacity * (1.0 - m_completion));
    QPen sweep(m_sweepColor, 2.6, Qt::SolidLine, Qt::RoundCap);
    p.setPen(sweep);
    const qreal span = m_reducedMotion ? 100.0 : 80.0 + 160.0 * breath;
    p.drawArc(ring, qRound((90.0 - m_angle) * 16.0), qRound(-span * 16.0));

    
    if (m_completion > 0.0) {
        p.setOpacity(m_opacity * m_completion);
        const QPointF start(-10.0, 0.0);
        const QPointF corner(-3.0, 7.0);
        const QPointF end(11.0, -8.0);
        QPainterPath check;
        check.moveTo(start);
        if (m_completion < 0.35) {
            check.lineTo(start + (corner - start) * (m_completion / 0.35));
        } else {
            check.lineTo(corner);
            check.lineTo(corner + (end - corner) * ((m_completion - 0.35) / 0.65));
        }
        sweep.setJoinStyle(Qt::RoundJoin);
        p.setPen(sweep);
        p.drawPath(check);
    }
    p.restore();

    p.setPen(palette().color(QPalette::WindowText));
    p.setFont(font());
    const int textWidth = qMax(0, width() - 48);
    const QString text = fontMetrics().elidedText(m_statusText, Qt::ElideRight, textWidth);
    p.drawText(QRectF(-textWidth / 2.0, 43.0, textWidth, fontMetrics().height() + 4),
               Qt::AlignHCenter | Qt::AlignTop, text);
}


void LoadingOverlay::mousePressEvent(QMouseEvent* event) {
    if (!m_isImmersive) event->accept();
}
void LoadingOverlay::mouseReleaseEvent(QMouseEvent* event) {
    if (!m_isImmersive) event->accept();
}
void LoadingOverlay::mouseDoubleClickEvent(QMouseEvent* event) {
    if (!m_isImmersive) event->accept();
}
