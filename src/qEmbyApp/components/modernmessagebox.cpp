#include "modernmessagebox.h"
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

void ModernMessageBox::setupUi(const QString &title, const QString &text, const QString &yesText, const QString &noText, ButtonType yesType, IconType iconType) {
    setTitle(title);

    auto *bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(16);

    if (iconType != NoIcon) {
        auto *iconLabel = new QLabel(this);
        iconLabel->setFixedSize(40, 40);

        QIcon icon;
        
        if (iconType == Question || iconType == Info) icon = QIcon(":/svg/light/question.svg");
        else if (iconType == Warning) icon = QIcon(":/svg/light/warning.svg");
        else if (iconType == Error) icon = QIcon(":/svg/light/error.svg");

        iconLabel->setPixmap(icon.pixmap(40, 40));

        auto *iconVLayout = new QVBoxLayout();
        iconVLayout->addWidget(iconLabel);
        iconVLayout->addStretch();
        bodyLayout->addLayout(iconVLayout);
    }

    auto *textLabel = new QLabel(text, this);
    textLabel->setObjectName("dialog-text");
    textLabel->setWordWrap(true);
    textLabel->setMinimumWidth(260);
    bodyLayout->addWidget(textLabel, 1);

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
