#include "moderntaginput.h"
#include "tagpresetoptionrow.h"
#include "../managers/thememanager.h"
#include <QAction>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

ModernTagInput::ModernTagInput(QWidget *parent)
    : QFrame(parent) {
    setObjectName("TagInputContainer");
    setFrameShape(QFrame::NoFrame);
    
    setFixedHeight(32);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 0, 2, 0);
    m_mainLayout->setSpacing(2);

    
    m_tagArea = new QHBoxLayout();
    m_tagArea->setContentsMargins(0, 0, 0, 0);
    m_tagArea->setSpacing(3);
    m_mainLayout->addLayout(m_tagArea);

    
    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setObjectName("TagInputInlineEdit");
    m_lineEdit->setPlaceholderText(tr("Type & Enter..."));
    m_lineEdit->setFrame(false);
    m_lineEdit->setMinimumWidth(60);
    m_lineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_mainLayout->addWidget(m_lineEdit, 1);

    m_buttonSpacer = new QWidget(this);
    m_buttonSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_buttonSpacer->hide();
    m_mainLayout->addWidget(m_buttonSpacer, 1);

    connect(m_lineEdit, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_lineEdit->text().trimmed();
        if (!text.isEmpty()) {
            addCustomTag(text);
            m_lineEdit->clear();
        }
    });
    connect(m_lineEdit, &QLineEdit::textChanged, this,
            [this]() { updateClearButtonVisibility(); });

    
    m_clearBtn = new QToolButton(this);
    m_clearBtn->setObjectName("TagInputClearBtn");
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setFixedSize(12, 12);
    m_clearBtn->setToolTip(tr("Clear all"));
    m_mainLayout->addWidget(m_clearBtn);

    connect(m_clearBtn, &QToolButton::clicked, this, &ModernTagInput::clearAll);

    
    m_moreBtn = new QToolButton(this);
    m_moreBtn->setObjectName("TagInputMoreBtn");
    m_moreBtn->setCursor(Qt::PointingHandCursor);
    m_moreBtn->setFixedSize(20, 20);
    m_moreBtn->setToolTip(tr("Presets"));
    m_mainLayout->addWidget(m_moreBtn);

    connect(m_moreBtn, &QToolButton::clicked, this, &ModernTagInput::showPresetMenu);

    
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this]() {
        rebuildTags(); 
    });

    updateClearButtonVisibility();
}

void ModernTagInput::addPreset(const QString &label, const QString &value) {
    m_presets.append({label, value});
}

void ModernTagInput::clearPresets() {
    m_presets.clear();
}

void ModernTagInput::setValue(const QString &commaSeparated) {
    m_selectedValues.clear();
    if (!commaSeparated.isEmpty()) {
        const auto parts = commaSeparated.split(',', Qt::SkipEmptyParts);
        for (const auto &p : parts) {
            const QString value = p.trimmed();
            if (!value.isEmpty() && !m_selectedValues.contains(value)) {
                m_selectedValues.append(value);
            }
        }
    }
    rebuildTags();
}

QString ModernTagInput::value() const {
    return m_selectedValues.join(",");
}

void ModernTagInput::setEditable(bool editable) {
    if (m_isEditable == editable) {
        return;
    }

    m_isEditable = editable;
    if (!m_isEditable) {
        m_lineEdit->clear();
    }
    m_lineEdit->setVisible(m_isEditable);
    m_buttonSpacer->setVisible(!m_isEditable);
    updateClearButtonVisibility();
    rebuildTags();
}

bool ModernTagInput::isEditable() const {
    return m_isEditable;
}

void ModernTagInput::setMaxPopupHeight(int height) {
    m_maxPopupHeight = qMax(100, height);
}

void ModernTagInput::setPopupMode(PopupMode mode) {
    m_popupMode = mode;
}

void ModernTagInput::setPopupThreshold(int threshold) {
    m_popupThreshold = qMax(1, threshold);
}

QSize ModernTagInput::sizeHint() const {
    return QSize(260, 32);
}

QSize ModernTagInput::minimumSizeHint() const {
    return QSize(180, 32);
}

void ModernTagInput::resizeEvent(QResizeEvent *event) {
    QFrame::resizeEvent(event);

    if (event->size().width() != event->oldSize().width()) {
        rebuildTags();
    }
}

void ModernTagInput::showEvent(QShowEvent *event) {
    QFrame::showEvent(event);

    
    QTimer::singleShot(0, this, [this]() { rebuildTags(); });
}

void ModernTagInput::rebuildTags() {
    if (m_isRebuilding) {
        return;
    }
    m_isRebuilding = true;

    
    while (m_tagArea->count() > 0) {
        QLayoutItem *item = m_tagArea->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    updateClearButtonVisibility();

    const int maxChipWidth = availableChipWidth();
    int usedWidth = 0;
    int hiddenCount = 0;

    for (int i = 0; i < m_selectedValues.size(); ++i) {
        const QString &value = m_selectedValues.at(i);
        auto *chip = createChipButton(displayTextForValue(value));

        const int chipSpacing = m_tagArea->count() > 0 ? m_tagArea->spacing() : 0;
        const int chipWidth = chip->sizeHint().width();
        const int remainingAfterCurrent = m_selectedValues.size() - i - 1;
        int summaryWidth = 0;
        if (remainingAfterCurrent > 0) {
            auto *summaryChip = createChipButton(
                QString("+%1").arg(remainingAfterCurrent), "TagChipSummary");
            summaryWidth = m_tagArea->spacing() + summaryChip->sizeHint().width();
            delete summaryChip;
        }

        const int projectedWidth = usedWidth + chipSpacing + chipWidth + summaryWidth;
        if (m_tagArea->count() > 0 && projectedWidth > maxChipWidth) {
            delete chip;
            hiddenCount = m_selectedValues.size() - i;
            break;
        }

        connect(chip, &QPushButton::clicked, this, [this, value]() {
            m_selectedValues.removeAll(value);
            scheduleRebuild(true);
        });

        m_tagArea->addWidget(chip);
        usedWidth += chipSpacing + chipWidth;
    }

    if (hiddenCount > 0) {
        auto *summaryChip =
            createChipButton(QString("+%1").arg(hiddenCount), "TagChipSummary");
        summaryChip->setToolTip(selectionToolTip());
        connect(summaryChip, &QPushButton::clicked, this,
                &ModernTagInput::showPresetMenu);
        m_tagArea->addWidget(summaryChip);
    }

    setToolTip(selectionToolTip());
    if (m_mainLayout) {
        m_mainLayout->invalidate();
    }
    if (layout()) {
        layout()->activate();
    }
    updateGeometry();
    update();
    m_isRebuilding = false;
}

void ModernTagInput::scheduleRebuild(bool shouldEmitChange) {
    if (shouldEmitChange) {
        m_emitChangeQueued = true;
    }

    if (m_isRebuildQueued) {
        return;
    }

    m_isRebuildQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_isRebuildQueued = false;
        rebuildTags();
        if (m_emitChangeQueued) {
            m_emitChangeQueued = false;
            this->emitChange();
        }
    });
}

void ModernTagInput::addCustomTag(const QString &text) {
    if (!m_isEditable) {
        return;
    }
    if (m_selectedValues.contains(text)) {
        return;
    }
    m_selectedValues.append(text);
    scheduleRebuild(true);
}

void ModernTagInput::showPresetMenu() {
    switch (m_popupMode) {
    case ForceCompact:
        showPresetCompactMenu();
        return;
    case ForcePopup:
        showPresetPopup();
        return;
    case Auto:
    default:
        if (m_presets.size() <= m_popupThreshold) {
            showPresetCompactMenu();
        } else {
            showPresetPopup();
        }
        return;
    }
}

void ModernTagInput::showPresetCompactMenu() {
    auto *menu = new QMenu(this);
    menu->setObjectName("TagPresetMenu");

    for (const auto &preset : m_presets) {
        auto *action = menu->addAction(preset.label);
        action->setCheckable(true);
        action->setChecked(m_selectedValues.contains(preset.value));

        connect(action, &QAction::toggled, this,
                [this, val = preset.value](bool checked) {
            if (checked && !m_selectedValues.contains(val)) {
                m_selectedValues.append(val);
            } else if (!checked) {
                m_selectedValues.removeAll(val);
            }
            scheduleRebuild(true);
        });
    }

    
    menu->popup(m_moreBtn->mapToGlobal(QPoint(0, m_moreBtn->height() + 2)));

    
    connect(menu, &QMenu::aboutToHide, menu, &QMenu::deleteLater);
}

void ModernTagInput::showPresetPopup() {
    
    auto *popup = new QFrame(this, Qt::Popup);
    popup->setObjectName("TagPresetPopup");
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setFrameShape(QFrame::NoFrame);

    auto *popupLayout = new QVBoxLayout(popup);
    popupLayout->setContentsMargins(0, 6, 0, 6);
    popupLayout->setSpacing(4);

    
    auto *searchRow = new QWidget(popup);
    auto *searchRowLayout = new QHBoxLayout(searchRow);
    searchRowLayout->setContentsMargins(6, 0, 6, 0);
    searchRowLayout->setSpacing(0);

    auto *searchEdit = new QLineEdit(searchRow);
    searchEdit->setObjectName("TagPresetSearchEdit");
    searchEdit->setPlaceholderText(tr("Search..."));
    searchEdit->setClearButtonEnabled(true);
    searchRowLayout->addWidget(searchEdit);
    popupLayout->addWidget(searchRow);

    
    auto *scrollArea = new QScrollArea(popup);
    scrollArea->setObjectName("TagPresetScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setMaximumHeight(m_maxPopupHeight);

    auto *listWidget = new QWidget();
    auto *listLayout = new QVBoxLayout(listWidget);
    
    listLayout->setContentsMargins(6, 0, 6, 0);
    listLayout->setSpacing(0);

    
    QList<TagPresetOptionRow *> optionRows;
    optionRows.reserve(m_presets.size());

    for (const auto &preset : m_presets) {
        auto *optionRow = new TagPresetOptionRow(preset.label, listWidget);
        optionRow->setChecked(m_selectedValues.contains(preset.value));

        connect(optionRow->checkBox(), &QCheckBox::toggled, this,
                [this, val = preset.value](bool checked) {
            if (checked && !m_selectedValues.contains(val)) {
                m_selectedValues.append(val);
            } else if (!checked) {
                m_selectedValues.removeAll(val);
            }
            scheduleRebuild(true);
        });

        listLayout->addWidget(optionRow);
        optionRows.append(optionRow);
    }

    listLayout->addStretch(1);
    scrollArea->setWidget(listWidget);
    popupLayout->addWidget(scrollArea);

    
    connect(searchEdit, &QLineEdit::textChanged, popup,
            [optionRows](const QString &filter) {
        const QString keyword = filter.trimmed();
        for (auto *optionRow : optionRows) {
            if (keyword.isEmpty()) {
                optionRow->setVisible(true);
            } else {
                optionRow->setVisible(
                    optionRow->text().contains(keyword, Qt::CaseInsensitive));
            }
        }
    });

    
    popup->adjustSize();
    const int popupWidth = qMax(popup->sizeHint().width(), this->width());
    popup->setFixedWidth(popupWidth);

    
    QPoint globalPos = mapToGlobal(QPoint(width() - popupWidth, height() + 2));
    popup->move(globalPos);
    popup->show();

    
    searchEdit->setFocus();

}

void ModernTagInput::clearAll() {
    m_selectedValues.clear();
    if (m_isEditable) {
        m_lineEdit->clear();
    }
    scheduleRebuild(true);
}

void ModernTagInput::emitChange() {
    emit valueChanged(value());
}

void ModernTagInput::updateClearButtonVisibility() {
    m_clearBtn->setVisible(!m_selectedValues.isEmpty() ||
                           (m_isEditable && !m_lineEdit->text().isEmpty()));
}

QString ModernTagInput::displayTextForValue(const QString &value) const {
    for (const auto &preset : m_presets) {
        if (preset.value == value) {
            return preset.label;
        }
    }

    return value;
}

QString ModernTagInput::selectionToolTip() const {
    QStringList labels;
    labels.reserve(m_selectedValues.size());
    for (const QString &value : m_selectedValues) {
        labels.append(displayTextForValue(value));
    }
    return labels.join(", ");
}

int ModernTagInput::availableChipWidth() const {
    if (!m_mainLayout) {
        return 0;
    }

    const QMargins margins = m_mainLayout->contentsMargins();
    int reservedWidth = margins.left() + margins.right();
    if (m_isEditable && m_lineEdit->isVisible()) {
        reservedWidth += m_lineEdit->minimumWidth();
    }
    reservedWidth += m_moreBtn->width() > 0 ? m_moreBtn->width()
                                            : m_moreBtn->sizeHint().width();
    reservedWidth += m_mainLayout->spacing();

    if (m_isEditable && m_lineEdit->isVisible()) {
        reservedWidth += m_mainLayout->spacing();
    }

    if (m_clearBtn->isVisible()) {
        reservedWidth += m_clearBtn->width() > 0 ? m_clearBtn->width()
                                                 : m_clearBtn->sizeHint().width();
        reservedWidth += m_mainLayout->spacing();
    }

    return qMax(0, contentsRect().width() - reservedWidth);
}

QPushButton *ModernTagInput::createChipButton(const QString &text,
                                              const QString &objectName) {
    auto *chip = new QPushButton(text, this);
    chip->setObjectName(objectName);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setFixedHeight(22);
    chip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    chip->ensurePolished();
    chip->setFixedWidth(chip->sizeHint().width());
    return chip;
}
