#include "settingscard.h"
#include "elidedlabel.h"
#include "modernswitch.h"
#include "moderntaginput.h"
#include "../managers/thememanager.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSlider>
#include <QPushButton>

#include "../../qEmbyCore/config/configstore.h"

SettingsCard::SettingsCard(const QString &iconSvgPath, const QString &title,
                           const QString &description, QWidget *controlWidget,
                           const QString &configKey, QWidget *parent,
                           const QVariant &defaultValue)
    : QFrame(parent), m_controlWidget(controlWidget), m_configKey(configKey),
      m_iconSvgPath(iconSvgPath), m_defaultValue(defaultValue) {
  
  setObjectName("SettingsCard");

  
  if (m_controlWidget) {
    m_controlWidget->setParent(this);
  }

  setupUi(title, description);

  
  if (!m_configKey.isEmpty() && m_controlWidget) {
    setupDataBinding();
  }

  
  connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
          &SettingsCard::onThemeChanged);
}

SettingsCard::~SettingsCard() = default;

void SettingsCard::setupUi(const QString &title, const QString &description) {
  
  m_mainLayout = new QHBoxLayout(this);
  m_mainLayout->setContentsMargins(16, 12, 16, 12);
  m_mainLayout->setSpacing(12);

  
  m_iconLabel = new QLabel(this);
  m_iconLabel->setFixedSize(16, 16);
  m_iconLabel->setAlignment(Qt::AlignCenter);
  m_iconLabel->setObjectName("SettingsCardIcon");
  updateIcon(); 

  
  auto *textLayout = new QVBoxLayout();
  textLayout->setContentsMargins(0, 0, 0, 0);
  textLayout->setSpacing(2);
  textLayout->setAlignment(Qt::AlignVCenter);

  m_titleLabel = new QLabel(title, this);
  m_titleLabel->setObjectName("SettingsCardTitle");

  m_descLabel = new ElidedLabel(this);
  m_descLabel->setObjectName("SettingsCardDesc");
  m_descLabel->setFullText(description);

  textLayout->addWidget(m_titleLabel);
  if (!description.isEmpty()) {
    textLayout->addWidget(m_descLabel);
  } else {
    m_descLabel->hide();
  }

  
  m_mainLayout->addWidget(m_iconLabel, 0, Qt::AlignVCenter);
  m_mainLayout->addLayout(textLayout, 1);

  
  if (m_controlWidget) {
    if (auto *combo = qobject_cast<QComboBox *>(m_controlWidget)) {
      combo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    } else if (auto *btn = qobject_cast<QPushButton *>(m_controlWidget)) {
      btn->setObjectName("SettingsCardButton");
      btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    } else if (auto *lineEdit = qobject_cast<QLineEdit *>(m_controlWidget)) {
      lineEdit->setMinimumWidth(200);
      lineEdit->setFixedHeight(32);
      lineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    } else if (qobject_cast<ModernTagInput *>(m_controlWidget)) {
      
      m_controlWidget->setMinimumWidth(220);
      m_controlWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    m_mainLayout->addWidget(m_controlWidget, 0, Qt::AlignVCenter);
  }
}


void SettingsCard::updateIcon() {
  if (m_iconSvgPath.isEmpty())
    return;
  QIcon icon = ThemeManager::getAdaptiveIcon(m_iconSvgPath);
  m_iconLabel->setPixmap(icon.pixmap(16, 16));
}

void SettingsCard::onThemeChanged() { updateIcon(); }

void SettingsCard::setupDataBinding() {
  auto *store = ConfigStore::instance();

  
  
  
  if (auto *switchControl = qobject_cast<ModernSwitch *>(m_controlWidget)) {
    
    switchControl->setChecked(store->get<bool>(m_configKey, m_defaultValue.toBool()));

    
    connect(switchControl, &ModernSwitch::toggled, this,
            [this, store](bool checked) { store->set(m_configKey, checked); });
  }
  
  
  
  else if (auto *comboControl = qobject_cast<QComboBox *>(m_controlWidget)) {
    
    QString currentVal = store->get<QString>(m_configKey);
    if (currentVal.isEmpty() && m_defaultValue.isValid()) {
      currentVal = m_defaultValue.toString();
    }
    int idx = comboControl->findData(currentVal);
    if (idx >= 0) {
      comboControl->setCurrentIndex(idx);
    }

    
    connect(comboControl, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, store, comboControl](int index) {
              store->set(m_configKey, comboControl->itemData(index));
            });
  }
  
  
  
  else if (auto *lineEdit = qobject_cast<QLineEdit *>(m_controlWidget)) {
    
    lineEdit->setText(store->get<QString>(m_configKey, m_defaultValue.toString()));

    
    connect(lineEdit, &QLineEdit::editingFinished, this,
            [this, store, lineEdit]() { store->set(m_configKey, lineEdit->text()); });
  }
  
  
  
  else if (auto *tagInput = qobject_cast<ModernTagInput *>(m_controlWidget)) {
    
    tagInput->setValue(store->get<QString>(m_configKey, m_defaultValue.toString()));

    
    connect(tagInput, &ModernTagInput::valueChanged, this,
            [this, store](const QString &val) { store->set(m_configKey, val); });
  }

  
  
  
  connect(store, &ConfigStore::valueChanged, this,
          &SettingsCard::onConfigValueChanged);
}

void SettingsCard::onConfigValueChanged(const QString &key,
                                        const QVariant &newValue) {
  if (key != m_configKey || !m_controlWidget)
    return;

  
  m_controlWidget->blockSignals(true);

  if (auto *switchControl = qobject_cast<ModernSwitch *>(m_controlWidget)) {
    switchControl->setChecked(newValue.toBool());
  } else if (auto *comboControl = qobject_cast<QComboBox *>(m_controlWidget)) {
    int idx = comboControl->findData(newValue);
    if (idx >= 0) {
      comboControl->setCurrentIndex(idx);
    }
  } else if (auto *lineEdit = qobject_cast<QLineEdit *>(m_controlWidget)) {
    lineEdit->setText(newValue.toString());
  } else if (auto *tagInput = qobject_cast<ModernTagInput *>(m_controlWidget)) {
    tagInput->setValue(newValue.toString());
  }

  m_controlWidget->blockSignals(false);
}
