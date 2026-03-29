#include "settingspagebase.h"
#include <QLabel>

SettingsPageBase::SettingsPageBase(QEmbyCore* core, const QString& pageTitle, QWidget* parent)
    : QWidget(parent), m_core(core)
{
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(16, 30, 16, 40);
    m_mainLayout->setSpacing(16);

    
    if (!pageTitle.isEmpty()) {
        auto* titleLabel = new QLabel(pageTitle, this);
        titleLabel->setObjectName("SettingsPageTitle");
        m_mainLayout->addWidget(titleLabel);
        m_mainLayout->addSpacing(10); 
    }
}
