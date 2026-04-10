#include "moderndialogbase.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>


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
    
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    
    setAttribute(Qt::WA_StyledBackground, true);

    auto *mainLayout = new QVBoxLayout(this);
    
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    
    m_titleBarWidget = new QWidget(this);
    m_titleBarWidget->setObjectName("dialog-titlebar");

    auto *titleBarLayout = new QHBoxLayout(m_titleBarWidget);
    
    titleBarLayout->setContentsMargins(16, 0, 0, 0);
    titleBarLayout->setSpacing(0);

    m_titleLabel = new QLabel(m_titleBarWidget);
    m_titleLabel->setObjectName("dialog-title");

    
    auto *agent = new QWK::WidgetWindowAgent(this);
    agent->setup(this);

    
    auto *closeBtn = new QWK::WindowButton(m_titleBarWidget);
    closeBtn->setObjectName("dialog-close-btn");
    closeBtn->setProperty("system-button", true); 
    connect(closeBtn, &QWK::WindowButton::clicked, this, &QDialog::reject);

    titleBarLayout->addWidget(m_titleLabel);
    titleBarLayout->addStretch();
    titleBarLayout->addWidget(closeBtn);

    
    agent->setTitleBar(m_titleBarWidget);
    agent->setSystemButton(QWK::WindowAgentBase::Close, closeBtn);

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
}

void ModernDialogBase::setTitle(const QString &title) {
    m_titleLabel->setText(title);
}
