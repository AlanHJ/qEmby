#include "moderndialogbase.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>


#include <QWKWidgets/widgetwindowagent.h>
#include <widgetframe/windowbar.h>
#include <widgetframe/windowbutton.h>

ModernDialogBase::ModernDialogBase(QWidget *parent) : QDialog(parent) {
    
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    
    setAttribute(Qt::WA_StyledBackground, true);

    
    auto *agent = new QWK::WidgetWindowAgent(this);
    agent->setup(this);

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

    
    auto *closeBtn = new QWK::WindowButton(m_titleBarWidget);
    closeBtn->setObjectName("dialog-close-btn");
    closeBtn->setProperty("system-button", true); 
    connect(closeBtn, &QWK::WindowButton::clicked, this, &QDialog::reject);

    titleBarLayout->addWidget(m_titleLabel);
    titleBarLayout->addStretch();
    titleBarLayout->addWidget(closeBtn);

    
    agent->setTitleBar(m_titleBarWidget);
    agent->setSystemButton(QWK::WindowAgentBase::Close, closeBtn);

    
    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setContentsMargins(20, 10, 20, 20); 

    mainLayout->addWidget(m_titleBarWidget);
    mainLayout->addLayout(m_contentLayout);
}

void ModernDialogBase::setTitle(const QString &title) {
    m_titleLabel->setText(title);
}
