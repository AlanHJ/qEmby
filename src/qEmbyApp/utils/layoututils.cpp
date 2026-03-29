#include "layoututils.h"
#include "../components/moderncombobox.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>

namespace LayoutUtils {

QHBoxLayout *createInfoRow(const QString &label, QLabel *&valueLabel) {
    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);

    auto *keyLabel = new QLabel(label);
    keyLabel->setObjectName("ManageInfoKey");
    keyLabel->setFixedWidth(120);

    valueLabel = new QLabel(QString::fromUtf8("—"));
    valueLabel->setObjectName("ManageInfoValue");
    valueLabel->setWordWrap(true);

    row->addWidget(keyLabel);
    row->addWidget(valueLabel, 1);
    return row;
}

ModernComboBox *createStyledCombo(QWidget *parent) {
    auto *combo = new ModernComboBox(parent);
    combo->setObjectName("ManageLibComboBox");
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return combo;
}

QFrame *createSeparator(QWidget *parent) {
    auto *sep = new QFrame(parent);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("ManageLibSeparator");
    return sep;
}

QLabel *createSectionLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName("ManageInfoKey");
    return label;
}

void clearLayout(QLayout *layout) {
    if (!layout) {
        return;
    }

    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        if (QLayout *childLayout = item->layout()) {
            clearLayout(childLayout);
            delete childLayout;
        }
        delete item;
    }
}

} 
