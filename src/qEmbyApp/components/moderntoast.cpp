#include "moderntoast.h"
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QStyleOption>
#include <QFontMetrics>
#include <QEasingCurve>

QPointer<ModernToast> ModernToast::s_instance = nullptr;

QWidget* ModernToast::getMainWindow() {
    QWidget *bestWin = nullptr;
    int maxArea = 0;
    for (QWidget *w : QApplication::topLevelWidgets()) {
        if (w->isVisible() && w->windowType() == Qt::Window) {
            int area = w->width() * w->height();
            if (area > maxArea) {
                maxArea = area;
                bestWin = w;
            }
        }
    }
    return bestWin;
}


void ModernToast::showMessage(const QString &msg, int durationMs) {
    if (!s_instance) {
        s_instance = new ModernToast(getMainWindow());
    }
    s_instance->showWithAnimation(msg, durationMs);
}

ModernToast::ModernToast(QWidget *parent)
    : QWidget(parent), m_textScale(1.0), m_comboCount(0), m_isCrtFadingOut(false)
{
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    
    setMinimumSize(0, 0);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    setWindowOpacity(0.0);

    
    m_showOpacity = new QPropertyAnimation(this, "windowOpacity", this);
    m_showOpacity->setDuration(200);
    m_showOpacity->setEasingCurve(QEasingCurve::OutQuad);

    m_showSlide = new QPropertyAnimation(this, "geometry", this);
    m_showSlide->setDuration(350);
    QEasingCurve slideCurve(QEasingCurve::OutBack);
    slideCurve.setOvershoot(1.2);
    m_showSlide->setEasingCurve(slideCurve);

    m_showGroup = new QParallelAnimationGroup(this);
    m_showGroup->addAnimation(m_showOpacity);
    m_showGroup->addAnimation(m_showSlide);

    
    
    
    m_crtSquashY = new QPropertyAnimation(this, "geometry", this);
    m_crtSquashY->setDuration(160); 
    m_crtSquashY->setEasingCurve(QEasingCurve::InCubic); 

    m_crtSquashX = new QPropertyAnimation(this, "geometry", this);
    m_crtSquashX->setDuration(120);
    m_crtSquashX->setEasingCurve(QEasingCurve::InExpo); 

    m_hideOpacity = new QPropertyAnimation(this, "windowOpacity", this);
    m_hideOpacity->setDuration(120);
    m_hideOpacity->setEasingCurve(QEasingCurve::InCubic);

    m_crtPhase2Group = new QParallelAnimationGroup(this);
    m_crtPhase2Group->addAnimation(m_crtSquashX);
    m_crtPhase2Group->addAnimation(m_hideOpacity);

    m_hideGroup = new QSequentialAnimationGroup(this);
    m_hideGroup->addAnimation(m_crtSquashY);
    m_hideGroup->addAnimation(m_crtPhase2Group);

    connect(m_hideGroup, &QSequentialAnimationGroup::finished, this, [this]() {
        hide();
        m_comboCount = 0;
        m_isCrtFadingOut = false; 

        
        if (graphicsEffect()) {
            graphicsEffect()->setEnabled(true);
        }
    });

    m_stayTimer = new QTimer(this);
    m_stayTimer->setSingleShot(true);
    connect(m_stayTimer, &QTimer::timeout, this, [this]() {
        m_isCrtFadingOut = true;
        update(); 

        
        if (graphicsEffect()) {
            graphicsEffect()->setEnabled(false);
        }

        QRect startGeo = geometry();
        
        QRect lineGeo(startGeo.x(), startGeo.y() + startGeo.height() / 2 - 1, startGeo.width(), 2);
        
        QRect dotGeo(startGeo.x() + startGeo.width() / 2 - 1, startGeo.y() + startGeo.height() / 2 - 1, 2, 2);

        m_crtSquashY->setStartValue(startGeo);
        m_crtSquashY->setEndValue(lineGeo);

        m_crtSquashX->setStartValue(lineGeo);
        m_crtSquashX->setEndValue(dotGeo);

        m_hideOpacity->setStartValue(windowOpacity());
        m_hideOpacity->setEndValue(0.0);

        m_hideGroup->start();
    });

    
    QEasingCurve expandCurve(QEasingCurve::OutBack);
    expandCurve.setOvershoot(2.5);

    m_shellExpand = new QPropertyAnimation(this, "geometry", this);
    m_shellExpand->setDuration(160);
    m_shellExpand->setEasingCurve(expandCurve);

    m_textExpand = new QPropertyAnimation(this, "textScale", this);
    m_textExpand->setDuration(160);
    m_textExpand->setEasingCurve(expandCurve);

    m_expandGroup = new QParallelAnimationGroup(this);
    m_expandGroup->addAnimation(m_shellExpand);
    m_expandGroup->addAnimation(m_textExpand);

    QEasingCurve shrinkCurve(QEasingCurve::OutCubic);

    m_shellShrink = new QPropertyAnimation(this, "geometry", this);
    m_shellShrink->setDuration(220);
    m_shellShrink->setEasingCurve(shrinkCurve);

    m_textShrink = new QPropertyAnimation(this, "textScale", this);
    m_textShrink->setDuration(220);
    m_textShrink->setEasingCurve(shrinkCurve);

    m_shrinkGroup = new QParallelAnimationGroup(this);
    m_shrinkGroup->addAnimation(m_shellShrink);
    m_shrinkGroup->addAnimation(m_textShrink);

    m_popSequentialGroup = new QSequentialAnimationGroup(this);
    m_popSequentialGroup->addAnimation(m_expandGroup);
    m_popSequentialGroup->addAnimation(m_shrinkGroup);
}

ModernToast::~ModernToast() {}

void ModernToast::setTextScale(qreal scale) {
    m_textScale = scale;
    update();
}

void ModernToast::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

    
    
    if (m_isCrtFadingOut) {
        return;
    }

    
    painter.setFont(this->font());
    QColor textColor = opt.palette.color(QPalette::WindowText);
    painter.setPen(textColor);

    QPointF center = rect().center();
    painter.translate(center);
    painter.scale(m_textScale, m_textScale);
    painter.translate(-center);

    painter.drawText(rect(), Qt::AlignCenter, m_message);
}


void ModernToast::showWithAnimation(const QString &msg, int durationMs) {
    m_message = msg;

    QFont currentFont = this->font();
    currentFont.setPixelSize(14);
    this->setFont(currentFont);

    QFontMetrics fm(currentFont);
    QSize baseTextSize = fm.size(0, msg);

    int targetW = baseTextSize.width() + 28;
    int targetH = baseTextSize.height() + 14;

    QWidget *mainWin = parentWidget();
    if (!mainWin) mainWin = getMainWindow();

    if (mainWin) {
        QRect geo = mainWin->geometry();
        int px = geo.x() + (geo.width() - targetW) / 2;
        int py = geo.y() + geo.height() - targetH - 120;
        m_baseGeometry = QRect(px, py, targetW, targetH);
    } else {
        QRect screenGeo = QGuiApplication::primaryScreen()->geometry();
        m_baseGeometry = QRect((screenGeo.width() - targetW) / 2, screenGeo.height() - targetH - 120, targetW, targetH);
    }

    show();

    bool isFullyHidden = (windowOpacity() == 0.0 && m_showGroup->state() != QAbstractAnimation::Running);
    bool isFadingOut = (m_hideGroup->state() == QAbstractAnimation::Running);

    if (isFullyHidden || isFadingOut) {
        m_comboCount = 0;

        m_hideGroup->stop();
        m_isCrtFadingOut = false;

        
        if (graphicsEffect()) {
            graphicsEffect()->setEnabled(true);
        }

        m_popSequentialGroup->stop();
        setTextScale(1.0);

        QRect slideStartGeo = m_baseGeometry.translated(0, 20);
        setGeometry(slideStartGeo);

        m_showSlide->setStartValue(slideStartGeo);
        m_showSlide->setEndValue(m_baseGeometry);

        m_showOpacity->setStartValue(windowOpacity());
        m_showOpacity->setEndValue(1.0);

        m_showGroup->start();
    } else {
        m_comboCount++;

        qreal dynamicScale = 1.15 + qMin(m_comboCount * 0.05, 0.3);

        int dx = qRound(targetW * (dynamicScale - 1.0) / 2.0) + 2;
        int dy = qRound(targetH * (dynamicScale - 1.0) / 2.0) + 2;

        m_showGroup->stop();
        setWindowOpacity(1.0);

        m_popSequentialGroup->stop();

        QRect expandedRect = m_baseGeometry.adjusted(-dx, -dy, dx, dy);

        m_shellExpand->setStartValue(geometry());
        m_shellExpand->setEndValue(expandedRect);

        m_textExpand->setStartValue(m_textScale);
        m_textExpand->setEndValue(dynamicScale);

        m_shellShrink->setStartValue(expandedRect);
        m_shellShrink->setEndValue(m_baseGeometry);

        m_textShrink->setStartValue(dynamicScale);
        m_textShrink->setEndValue(1.0);

        m_popSequentialGroup->start();
    }

    
    m_stayTimer->start(durationMs);
}
