#include "moderndialogbase.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QApplication>
#include <QDebug>
#include <QPointer>
#include <QShowEvent>
#include <QTimer>
#include <QWindow>


#include <QWKWidgets/widgetwindowagent.h>
#include <widgetframe/windowbar.h>
#include <widgetframe/windowbutton.h>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

#ifdef Q_OS_WIN
using DwmSetWindowAttributePtr =
    HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);

void setWindowTransitionsDisabled(WId windowId, bool disabled)
{
    static const HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
    if (!dwmapi || !windowId) {
        return;
    }

    static const auto dwmSetWindowAttribute =
        reinterpret_cast<DwmSetWindowAttributePtr>(
            GetProcAddress(dwmapi, "DwmSetWindowAttribute"));
    if (!dwmSetWindowAttribute) {
        return;
    }

    constexpr DWORD kDwmwaTransitionsForcedDisabled = 3;
    const BOOL value = disabled ? TRUE : FALSE;
    dwmSetWindowAttribute(reinterpret_cast<HWND>(windowId),
                          kDwmwaTransitionsForcedDisabled,
                          &value, sizeof(value));
}
#endif

} 

ModernDialogBase::ModernDialogBase(QWidget *parent,
                                   bool disableNativeTransitions)
    : QDialog(parent) {
    qInfo() << "[ModernDialogBase] construction begin" << "| dialog=" << this;
    
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    
    setAttribute(Qt::WA_StyledBackground, true);

    auto *mainLayout = new QVBoxLayout(this);
    
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    
    m_titleBarWidget = new QWidget(this);
    m_titleBarWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_titleBarWidget->setObjectName("dialog-titlebar");
    m_titleBarWidget->setProperty("standaloneDialogTitleBar", true);
    
    
    m_titleBarWidget->setSizePolicy(QSizePolicy::Expanding,
                                    QSizePolicy::Fixed);

    auto *titleBarLayout = new QHBoxLayout(m_titleBarWidget);
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    
    titleBarLayout->setContentsMargins(52, 0, 16, 0);
#else
    
    titleBarLayout->setContentsMargins(16, 0, 0, 0);
#endif
    titleBarLayout->setSpacing(0);

    m_titleLabel = new QLabel(m_titleBarWidget);
    m_titleLabel->setObjectName("dialog-title");

    qInfo() << "[ModernDialogBase] window backend=qwindowkit" << "| dialog=" << this;
    auto *agent = new QWK::WidgetWindowAgent(this);
    qInfo() << "[ModernDialogBase] window agent setup begin" << "| dialog=" << this;
    const bool agentReady = agent->setup(this);
    qInfo() << "[ModernDialogBase] window agent setup complete"
            << "| dialog=" << this << "| ready=" << agentReady;
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    agent->setWindowAttribute("no-system-buttons", false);
    agent->setWindowAttribute("macos-close-button-only", true);
#endif

#if !defined(Q_OS_MACOS) && !defined(Q_OS_MAC)
    
    auto *closeBtn = new QWK::WindowButton(m_titleBarWidget);
    closeBtn->setObjectName("dialog-close-btn");
    closeBtn->setProperty("system-button", true); 
    closeBtn->setToolTip(tr("Close"));
    closeBtn->setAccessibleName(tr("Close"));
    closeBtn->setFocusPolicy(Qt::NoFocus);
    connect(closeBtn, &QWK::WindowButton::clicked, this, &QDialog::reject);
#endif

    titleBarLayout->addWidget(m_titleLabel);
    titleBarLayout->addStretch();
#if !defined(Q_OS_MACOS) && !defined(Q_OS_MAC)
    titleBarLayout->addWidget(closeBtn);
#endif

    
    agent->setTitleBar(m_titleBarWidget);
#if !defined(Q_OS_MACOS) && !defined(Q_OS_MAC)
    agent->setSystemButton(QWK::WindowAgentBase::Close, closeBtn);
#endif

#ifdef Q_OS_WIN
    if (disableNativeTransitions) {
        setWindowTransitionsDisabled(winId(), true);
    }
#else
    Q_UNUSED(disableNativeTransitions);
#endif

    
    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setContentsMargins(20, 10, 20, 20); 

    mainLayout->addWidget(m_titleBarWidget);
    mainLayout->addLayout(m_contentLayout);
    qInfo() << "[ModernDialogBase] construction complete" << "| dialog=" << this;
}

int ModernDialogBase::exec() {
    QPointer<ModernDialogBase> guard(this);
    qInfo() << "[ModernDialogBase] exec begin"
            << "| dialog=" << this << "| size=" << size()
            << "| parentVisible=" << (parentWidget() && parentWidget()->isVisible());
    const int dialogResult = QDialog::exec();
    qInfo() << "[ModernDialogBase] exec complete"
            << "| alive=" << !guard.isNull() << "| result=" << dialogResult;
    return dialogResult;
}

void ModernDialogBase::showEvent(QShowEvent *event) {
    qInfo() << "[ModernDialogBase] show event begin" << "| dialog=" << this;
    QDialog::showEvent(event);
    qInfo() << "[ModernDialogBase] show event complete"
            << "| dialog=" << this << "| geometry=" << geometry()
            << "| modality=" << windowModality();
    QTimer::singleShot(0, this, [this]() {
        qInfo() << "[ModernDialogBase] event loop responsive"
                << "| dialog=" << this << "| visible=" << isVisible()
                << "| exposed=" << (windowHandle() && windowHandle()->isExposed())
                << "| active=" << isActiveWindow()
                << "| activeModal=" << (QApplication::activeModalWidget() == this);
    });
    QTimer::singleShot(250, this, [this]() {
        qInfo() << "[ModernDialogBase] post-show state"
                << "| dialog=" << this << "| visible=" << isVisible()
                << "| exposed=" << (windowHandle() && windowHandle()->isExposed())
                << "| geometry=" << geometry();
    });
}

void ModernDialogBase::setTitle(const QString &title) {
    setWindowTitle(title);
    m_titleLabel->setText(title);
}
