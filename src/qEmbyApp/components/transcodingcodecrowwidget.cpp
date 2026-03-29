#include "transcodingcodecrowwidget.h"

#include "elidedlabel.h"
#include "../managers/thememanager.h"
#include "modernswitch.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

TranscodingCodecRowWidget::TranscodingCodecRowWidget(const QString& codecId,
                                                     const QString& title,
                                                     const QString& description,
                                                     const QString& infoToolTip,
                                                     const QString& dragToolTip,
                                                     QWidget* parent)
    : QFrame(parent), m_codecId(codecId) {
    setObjectName("TranscodingCodecItem");
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* rowLayout = new QHBoxLayout(this);
    rowLayout->setContentsMargins(14, 12, 14, 12);
    rowLayout->setSpacing(12);

    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(3);

    m_titleLabel = new ElidedLabel(this);
    m_titleLabel->setObjectName("TranscodingCodecName");
    m_titleLabel->setFullText(title);
    textLayout->addWidget(m_titleLabel);

    m_descriptionLabel = new ElidedLabel(this);
    m_descriptionLabel->setObjectName("TranscodingCodecDescription");
    m_descriptionLabel->setFullText(description);
    m_descriptionLabel->setVisible(!description.trimmed().isEmpty());
    textLayout->addWidget(m_descriptionLabel);

    rowLayout->addLayout(textLayout, 1);

    auto* controlsLayout = new QHBoxLayout();
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(8);

    m_infoButton = new QPushButton(this);
    m_infoButton->setObjectName("TranscodingMiniButton");
    m_infoButton->setCursor(Qt::PointingHandCursor);
    m_infoButton->setFixedSize(30, 30);
    m_infoButton->setIconSize(QSize(16, 16));
    m_infoButton->setIcon(ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/player/info.svg")));
    m_infoButton->setToolTip(infoToolTip);
    m_infoButton->setFocusPolicy(Qt::NoFocus);
    controlsLayout->addWidget(m_infoButton, 0, Qt::AlignVCenter);

    m_dragHandle = new QPushButton(this);
    m_dragHandle->setObjectName("TranscodingDragHandle");
    m_dragHandle->setCursor(Qt::OpenHandCursor);
    m_dragHandle->setFixedSize(30, 30);
    m_dragHandle->setToolTip(dragToolTip);
    m_dragHandle->setFocusPolicy(Qt::NoFocus);
    m_dragHandle->installEventFilter(this);
    controlsLayout->addWidget(m_dragHandle, 0, Qt::AlignVCenter);

    m_switch = new ModernSwitch(this);
    controlsLayout->addWidget(m_switch, 0, Qt::AlignVCenter);

    rowLayout->addLayout(controlsLayout, 0);

    connect(m_infoButton, &QPushButton::clicked, this,
            [this]() { emit infoRequested(m_codecId); });
    connect(m_switch, &ModernSwitch::toggled, this, [this](bool enabled) {
        emit enabledChanged(m_codecId, enabled);
    });
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        if (m_infoButton) {
            m_infoButton->setIcon(
                ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/player/info.svg")));
        }
    });
}

bool TranscodingCodecRowWidget::isCodecEnabled() const {
    return m_switch && m_switch->isChecked();
}

void TranscodingCodecRowWidget::setCodecEnabled(bool enabled) {
    if (m_switch) {
        m_switch->setChecked(enabled);
    }
}

void TranscodingCodecRowWidget::setDragging(bool dragging) {
    setProperty("dragging", dragging);
    style()->unpolish(this);
    style()->polish(this);
    update();

    if (m_dragHandle) {
        m_dragHandle->setCursor(dragging ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
    }
}

QSize TranscodingCodecRowWidget::sizeHint() const {
    return QSize(420, 68);
}

bool TranscodingCodecRowWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_dragHandle) {
        return QFrame::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            break;
        }

        m_dragCandidate = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_dragStartPos = mouseEvent->globalPosition().toPoint();
#else
        m_dragStartPos = mouseEvent->globalPos();
#endif
        m_dragHandle->setCursor(Qt::ClosedHandCursor);
        return true;
    }
    case QEvent::MouseMove: {
        if (!m_dragCandidate) {
            break;
        }

        auto* mouseEvent = static_cast<QMouseEvent*>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint globalPos = mouseEvent->globalPosition().toPoint();
#else
        const QPoint globalPos = mouseEvent->globalPos();
#endif

        if (!m_dragActive &&
            (globalPos - m_dragStartPos).manhattanLength() >=
                QApplication::startDragDistance()) {
            m_dragActive = true;
            emit dragStarted(this, globalPos);
        }

        if (m_dragActive) {
            emit dragMoved(this, globalPos);
        }
        return true;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            break;
        }

        const bool hadActiveDrag = m_dragActive;
        m_dragCandidate = false;
        m_dragActive = false;
        m_dragHandle->setCursor(Qt::OpenHandCursor);

        if (hadActiveDrag) {
            emit dragFinished(this);
        }
        return true;
    }
    default:
        break;
    }

    return QFrame::eventFilter(watched, event);
}
