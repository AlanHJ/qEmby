#include "playeroverlaydialog.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QShowEvent>
#include <QStyle>
#include <QVBoxLayout>

namespace
{

constexpr int kOverlayMargin = 28;
constexpr int kMinimumSurfaceWidth = 280;
constexpr int kMinimumSurfaceHeight = 180;

} 

PlayerOverlayDialog::PlayerOverlayDialog(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    if (parent)
    {
        parent->installEventFilter(this);
    }

    
    m_surface = new QWidget(this);
    m_surface->setAttribute(Qt::WA_StyledBackground, true);
    m_surface->setProperty("playerDialogSurface", true);
    m_surface->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *shadow = new QGraphicsDropShadowEffect(m_surface);
    shadow->setBlurRadius(42);
    shadow->setOffset(0, 18);
    shadow->setColor(QColor(0, 0, 0, 110));
    m_surface->setGraphicsEffect(shadow);

    auto *surfaceLayout = new QVBoxLayout(m_surface);
    surfaceLayout->setContentsMargins(0, 0, 0, 0);
    surfaceLayout->setSpacing(0);

    m_titleBarWidget = new QWidget(m_surface);
    m_titleBarWidget->setObjectName("dialog-titlebar");
    m_titleBarWidget->installEventFilter(this);

    auto *titleBarLayout = new QHBoxLayout(m_titleBarWidget);
    titleBarLayout->setContentsMargins(16, 0, 0, 0);
    titleBarLayout->setSpacing(0);

    m_titleLabel = new QLabel(m_titleBarWidget);
    m_titleLabel->setObjectName("dialog-title");
    
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    titleBarLayout->addWidget(m_titleLabel);
    titleBarLayout->addStretch();

    auto *closeButton = new QPushButton(m_titleBarWidget);
    closeButton->setObjectName("dialog-close-btn");
    closeButton->setCursor(Qt::PointingHandCursor);
    
    
    
    closeButton->setIcon(QIcon(QStringLiteral(":/svg/dark/close.svg")));
    closeButton->setIconSize(QSize(12, 12));
    connect(closeButton, &QPushButton::clicked, this,
            &PlayerOverlayDialog::reject);
    titleBarLayout->addWidget(closeButton);

    surfaceLayout->addWidget(m_titleBarWidget);

    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setContentsMargins(20, 10, 20, 20);
    surfaceLayout->addLayout(m_contentLayout);

    auto *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escapeShortcut, &QShortcut::activated, this,
            &PlayerOverlayDialog::reject);
}

void PlayerOverlayDialog::setTitle(const QString &title)
{
    if (m_titleLabel)
    {
        m_titleLabel->setText(title);
    }
}

void PlayerOverlayDialog::setSurfaceObjectName(const QString &name)
{
    if (m_surface)
    {
        m_surface->setObjectName(name);
        m_surface->style()->unpolish(m_surface);
        m_surface->style()->polish(m_surface);
    }
}

void PlayerOverlayDialog::setSurfacePreferredSize(const QSize &size)
{
    m_surfacePreferredSize = size;
    updateSurfaceBounds();
}

QWidget *PlayerOverlayDialog::surfaceWidget() const
{
    return m_surface;
}

void PlayerOverlayDialog::open()
{
    syncOverlayGeometry();
    show();
    raise();
    setFocus(Qt::OtherFocusReason);
}

void PlayerOverlayDialog::accept()
{
    if (m_resultEmitted)
    {
        return;
    }

    m_resultEmitted = true;
    emit accepted();
    emit finished(Accepted);
    close();
}

void PlayerOverlayDialog::reject()
{
    if (m_resultEmitted)
    {
        return;
    }

    m_resultEmitted = true;
    emit rejected();
    emit finished(Rejected);
    close();
}

void PlayerOverlayDialog::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    syncOverlayGeometry();
}

void PlayerOverlayDialog::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateSurfaceBounds();
}

void PlayerOverlayDialog::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), QColor(2, 6, 23, 118));
}

void PlayerOverlayDialog::mousePressEvent(QMouseEvent *event)
{
    
    if (event && !isInsideSurface(event->position().toPoint()))
    {
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

bool PlayerOverlayDialog::eventFilter(QObject *watched, QEvent *event)
{
    
    if (watched == m_titleBarWidget && event)
    {
        switch (event->type())
        {
        case QEvent::MouseButtonPress:
        {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton)
            {
                m_isDragging = true;
                m_dragStartGlobalPos = me->globalPosition().toPoint();
                m_surfaceDragStartPos = m_surface->pos();
                me->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseMove:
        {
            if (m_isDragging)
            {
                auto *me = static_cast<QMouseEvent *>(event);
                const QPoint delta =
                    me->globalPosition().toPoint() - m_dragStartGlobalPos;
                QPoint newPos = m_surfaceDragStartPos + delta;

                
                const int sw = m_surface->width();
                const int sh = m_surface->height();
                newPos.setX(qBound(kOverlayMargin, newPos.x(),
                                   qMax(kOverlayMargin,
                                        width() - sw - kOverlayMargin)));
                newPos.setY(qBound(kOverlayMargin, newPos.y(),
                                   qMax(kOverlayMargin,
                                        height() - sh - kOverlayMargin)));
                m_surface->move(newPos);

                
                const QPoint centered((width() - sw) / 2,
                                      (height() - sh) / 2);
                m_surfaceOffset = newPos - centered;
                me->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
        {
            if (m_isDragging)
            {
                m_isDragging = false;
                auto *me = static_cast<QMouseEvent *>(event);
                me->accept();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    
    if (watched == parentWidget() && event)
    {
        switch (event->type())
        {
        case QEvent::Resize:
        case QEvent::Move:
        case QEvent::Show:
        case QEvent::ZOrderChange:
            syncOverlayGeometry();
            break;
        case QEvent::Hide:
            hide();
            break;
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void PlayerOverlayDialog::syncOverlayGeometry()
{
    if (!parentWidget())
    {
        return;
    }

    setGeometry(parentWidget()->rect());
    updateSurfaceBounds();
    raise();
}

void PlayerOverlayDialog::updateSurfaceBounds()
{
    if (!m_surface)
    {
        return;
    }

    const int availableWidth = qMax(0, width() - (kOverlayMargin * 2));
    const int availableHeight = qMax(0, height() - (kOverlayMargin * 2));
    if (availableWidth <= 0 || availableHeight <= 0)
    {
        return;
    }

    QSize targetSize = m_surfacePreferredSize;
    if (!targetSize.isValid())
    {
        targetSize = m_surface->sizeHint();
    }

    targetSize = targetSize.boundedTo(QSize(availableWidth, availableHeight));

    const QSize minimumVisibleSize(qMin(availableWidth, kMinimumSurfaceWidth),
                                   qMin(availableHeight, kMinimumSurfaceHeight));
    targetSize = targetSize.expandedTo(minimumVisibleSize);

    m_surface->setFixedSize(targetSize);

    
    const QPoint centered((width() - targetSize.width()) / 2,
                          (height() - targetSize.height()) / 2);
    QPoint finalPos = centered + m_surfaceOffset;

    
    finalPos.setX(qBound(kOverlayMargin, finalPos.x(),
                         qMax(kOverlayMargin,
                              width() - targetSize.width() - kOverlayMargin)));
    finalPos.setY(qBound(kOverlayMargin, finalPos.y(),
                         qMax(kOverlayMargin,
                              height() - targetSize.height() - kOverlayMargin)));
    m_surface->move(finalPos);
}

bool PlayerOverlayDialog::isInsideSurface(const QPoint &pos) const
{
    return m_surface && m_surface->geometry().contains(pos);
}
