#include "modernmessagebox.h"
#include "modernswitch.h"
#include "../managers/thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>

ModernMessageBox::ModernMessageBox(QWidget *parent) : ModernDialogBase(parent) {}

bool ModernMessageBox::question(QWidget *parent, const QString &title, const QString &text, const QString &yesText, const QString &noText, ButtonType yesType, IconType iconType) {
    ModernMessageBox box(parent);
    box.setupUi(title, text, yesText, noText, yesType, iconType);
    box.exec();
    return box.m_result;
}

bool ModernMessageBox::questionWithToggle(QWidget *parent,
                                          const QString &title,
                                          const QString &text,
                                          const QString &toggleText,
                                          bool *toggleChecked,
                                          const QString &yesText,
                                          const QString &noText,
                                          ButtonType yesType,
                                          IconType iconType) {
    ModernMessageBox box(parent);
    box.setupUi(title, text, yesText, noText, yesType, iconType, toggleText,
                toggleChecked ? *toggleChecked : false);
    box.exec();
    if (toggleChecked) {
        *toggleChecked = box.m_toggleChecked;
    }
    return box.m_result;
}

void ModernMessageBox::information(QWidget *parent, const QString &title, const QString &text, const QString &okText) {
    ModernMessageBox box(parent);
    
    box.setupUi(title, text, okText, "", Primary, Info);
    box.exec();
}

void ModernMessageBox::warning(QWidget *parent, const QString &title, const QString &text, const QString &okText) {
    ModernMessageBox box(parent);
    box.setupUi(title, text, okText, "", Primary, Warning);
    box.exec();
}

void ModernMessageBox::critical(QWidget *parent, const QString &title, const QString &text, const QString &okText) {
    ModernMessageBox box(parent);
    
    box.setupUi(title, text, okText, "", Danger, Error);
    box.exec();
}

void ModernMessageBox::setupUi(const QString &title,
                               const QString &text,
                               const QString &yesText,
                               const QString &noText,
                               ButtonType yesType,
                               IconType iconType,
                               const QString &toggleText,
                               bool toggleChecked) {
    setTitle(title);
    m_toggleChecked = toggleChecked;

    auto *bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(16);

    if (iconType != NoIcon) {
        auto *iconLabel = new QLabel(this);
        iconLabel->setFixedSize(40, 40);

        QString iconPath;
        QColor iconColor;
        
        if (iconType == Question || iconType == Info) {
            iconPath = QStringLiteral(":/svg/light/question.svg");
            iconColor = ThemeManager::instance()->isDarkMode()
                            ? QColor(QStringLiteral("#93C5FD"))
                            : QColor(QStringLiteral("#2563EB"));
        } else if (iconType == Warning) {
            iconPath = QStringLiteral(":/svg/light/warning.svg");
            iconColor = ThemeManager::instance()->isDarkMode()
                            ? QColor(QStringLiteral("#FCD34D"))
                            : QColor(QStringLiteral("#D97706"));
        } else if (iconType == Error) {
            iconPath = QStringLiteral(":/svg/light/error.svg");
            iconColor = ThemeManager::instance()->isDarkMode()
                            ? QColor(QStringLiteral("#FCA5A5"))
                            : QColor(QStringLiteral("#DC2626"));
        }

        iconLabel->setPixmap(
            ThemeManager::getAdaptiveIcon(iconPath, iconColor).pixmap(40, 40));

        auto *iconVLayout = new QVBoxLayout();
        iconVLayout->addWidget(iconLabel);
        iconVLayout->addStretch();
        bodyLayout->addLayout(iconVLayout);
    }

    auto *contentVLayout = new QVBoxLayout();
    contentVLayout->setContentsMargins(0, 0, 0, 0);
    contentVLayout->setSpacing(14);

    auto *textLabel = new QLabel(text, this);
    textLabel->setObjectName("dialog-text");
    textLabel->setWordWrap(true);
    textLabel->setMinimumWidth(260);
    contentVLayout->addWidget(textLabel);

    if (!toggleText.trimmed().isEmpty()) {
        auto *toggleRow = new QWidget(this);
        toggleRow->setObjectName("dialog-option-row");
        auto *toggleLayout = new QHBoxLayout(toggleRow);
        toggleLayout->setContentsMargins(0, 0, 0, 0);
        toggleLayout->setSpacing(10);

        auto *toggleLabel = new QLabel(toggleText, toggleRow);
        toggleLabel->setObjectName("dialog-option-label");
        toggleLayout->addWidget(toggleLabel);

        auto *toggleSwitch = new ModernSwitch(toggleRow);
        toggleSwitch->setChecked(toggleChecked);
        connect(toggleSwitch, &ModernSwitch::toggled, this,
                [this](bool checked) { m_toggleChecked = checked; });
        toggleLayout->addWidget(toggleSwitch);
        toggleLayout->addStretch();

        contentVLayout->addWidget(toggleRow);
    }

    bodyLayout->addLayout(contentVLayout, 1);

    contentLayout()->addLayout(bodyLayout);
    contentLayout()->addSpacing(24); 

    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->setSpacing(12);

    
    if (!noText.isEmpty()) {
        auto *noBtn = new QPushButton(noText, this);
        noBtn->setObjectName("dialog-btn-cancel");
        noBtn->setCursor(Qt::PointingHandCursor);
        connect(noBtn, &QPushButton::clicked, this, [this]() {
            m_result = false;
            reject();
        });
        btnLayout->addWidget(noBtn);
    }

    auto *yesBtn = new QPushButton(yesText, this);
    yesBtn->setCursor(Qt::PointingHandCursor);
    yesBtn->setObjectName(yesType == Danger ? "dialog-btn-danger" : "dialog-btn-primary");
    connect(yesBtn, &QPushButton::clicked, this, [this]() {
        m_result = true;
        accept();
    });

    btnLayout->addWidget(yesBtn);

    contentLayout()->addLayout(btnLayout);
}
