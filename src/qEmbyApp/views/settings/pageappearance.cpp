#include "pageappearance.h"
#include "../../components/moderncombobox.h"
#include "../../components/modernmessagebox.h"
#include "../../components/moderntoast.h"
#include "../../components/modernswitch.h"
#include "../../components/settingscard.h"
#include "../../components/settingssubpanel.h"
#include "../../managers/searchhistorymanager.h"
#include "config/config_keys.h"
#include "config/configstore.h"
#include <QLabel>
#include <QPushButton>

PageAppearance::PageAppearance(QEmbyCore *core, QWidget *parent) : SettingsPageBase(core, tr("Appearance"), parent)
{
    
    auto *themeCombo = new ModernComboBox(this);
    themeCombo->addItem(tr("System Default"), "system");
    themeCombo->addItem(tr("Light Mode"), "light");
    themeCombo->addItem(tr("Dark Mode"), "dark");
    m_mainLayout->addWidget(new SettingsCard(":/svg/dark/appearance-theme.svg", tr("Theme Mode"),
                                             tr("Switch application theme (takes effect immediately)"), themeCombo,
                                             ConfigKeys::ThemeMode, this));

    
    auto *fontSizeCombo = new ModernComboBox(this);
    fontSizeCombo->addItem(tr("Small"), "small");
    fontSizeCombo->addItem(tr("Medium"), "medium");
    fontSizeCombo->addItem(tr("Large"), "large");
    fontSizeCombo->setCurrentIndex(1); 
    m_mainLayout->addWidget(new SettingsCard(":/svg/dark/appearance-font-size.svg", tr("Font Size"),
                                             tr("Adjust the overall font size of the interface"), fontSizeCombo,
                                             ConfigKeys::FontSize, this));

    
    auto *sidebarCombo = new ModernComboBox(this);
    sidebarCombo->addItem(tr("Left"), "left");
    sidebarCombo->addItem(tr("Right"), "right");
    m_mainLayout->addWidget(new SettingsCard(":/svg/dark/appearance-sidebar.svg", tr("Sidebar Position"),
                                             tr("Place the navigation sidebar on the left or right side"), sidebarCombo,
                                             ConfigKeys::SidebarPosition, this));

    
    m_mainLayout->addWidget(new SettingsCard(":/svg/dark/appearance-sidebar-pin.svg", tr("Pin Sidebar"),
                                             tr("Keep the sidebar always visible and integrated into the main layout"),
                                             new ModernSwitch(this), ConfigKeys::SidebarPinned, this));

    
    auto *windowStateCombo = new ModernComboBox(this);
    windowStateCombo->addItem(tr("Normal Window"), "normal");
    windowStateCombo->addItem(tr("Maximized"), "maximized");
    m_mainLayout->addWidget(new SettingsCard(":/svg/dark/appearance-window-state.svg", tr("Startup Window State"),
                                             tr("Choose window state when the application launches"), windowStateCombo,
                                             ConfigKeys::StartupWindowState, this));

    
    m_mainLayout->addWidget(new SettingsCard(":/svg/dark/appearance-animations.svg", tr("Reduce Animations"),
                                             tr("Reduce page transitions and hover effects for better performance"),
                                             new ModernSwitch(this), ConfigKeys::UiAnimations, this));

    
    m_mainLayout->addWidget(new SettingsCard(":/svg/dark/appearance-animations.svg", tr("Snapshot Navigation"),
                                             tr("Use static screenshots during page transitions for smoother animation on complex views"),
                                             new ModernSwitch(this), ConfigKeys::SnapshotNavigation, this));

    
    auto *searchHistorySwitch = new ModernSwitch(this);
    m_mainLayout->addWidget(new SettingsCard(":/svg/dark/search.svg", tr("Enable Search History"),
                                             tr("Save recent searches locally for quick reuse"), searchHistorySwitch,
                                             ConfigKeys::SearchHistoryEnabled, this, QVariant(false)));

    auto *searchAutocompletePanel = new SettingsSubPanel(":/svg/dark/search-autocomplete.svg", this);
    auto *searchAutocompleteLabel = new QLabel(tr("Enable Search Autocomplete"), this);
    searchAutocompleteLabel->setObjectName("SettingsCardDesc");
    auto *searchAutocompleteSwitch = new ModernSwitch(this);
    searchAutocompleteSwitch->setChecked(
        ConfigStore::instance()->get<bool>(ConfigKeys::SearchAutocompleteEnabled, false));
    searchAutocompletePanel->contentLayout()->addWidget(searchAutocompleteLabel, 1);
    searchAutocompletePanel->contentLayout()->addWidget(searchAutocompleteSwitch, 0, Qt::AlignVCenter);
    m_mainLayout->addWidget(searchAutocompletePanel);

    auto *clearHistoryPanel = new SettingsSubPanel(":/svg/dark/delete.svg", this);
    auto *clearHistoryLabel = new QLabel(tr("Clear Search History"), this);
    clearHistoryLabel->setObjectName("SettingsCardDesc");
    auto *clearHistoryBtn = new QPushButton(tr("Clear All"), this);
    clearHistoryBtn->setObjectName("SettingsCardButton");
    clearHistoryBtn->setCursor(Qt::PointingHandCursor);
    clearHistoryBtn->setFixedHeight(30);
    clearHistoryPanel->contentLayout()->addWidget(clearHistoryLabel, 1);
    clearHistoryPanel->contentLayout()->addWidget(clearHistoryBtn, 0, Qt::AlignVCenter);
    m_mainLayout->addWidget(clearHistoryPanel);

    if (ConfigStore::instance()->get<bool>(ConfigKeys::SearchHistoryEnabled, false))
    {
        searchAutocompletePanel->initExpanded();
        clearHistoryPanel->initExpanded();
    }

    connect(searchHistorySwitch, &ModernSwitch::toggled, searchAutocompletePanel, &SettingsSubPanel::setExpanded);
    connect(searchHistorySwitch, &ModernSwitch::toggled, clearHistoryPanel, &SettingsSubPanel::setExpanded);

    connect(searchAutocompleteSwitch, &ModernSwitch::toggled, this,
            [](bool checked) { ConfigStore::instance()->set(ConfigKeys::SearchAutocompleteEnabled, checked); });

    connect(
        clearHistoryBtn, &QPushButton::clicked, this,
        [this]()
        {
            const bool confirmed = ModernMessageBox::question(
                this, tr("Clear Search History"),
                tr("Are you sure you want to clear search history for all servers?\n\nThis action cannot be undone."),
                tr("Clear"), tr("Cancel"), ModernMessageBox::Danger, ModernMessageBox::Warning);
            if (!confirmed)
            {
                return;
            }

            SearchHistoryManager::instance()->clearAllHistory();
            ModernToast::showMessage(tr("Search history cleared"), 1500);
        });

    
    m_mainLayout->addStretch();
}
