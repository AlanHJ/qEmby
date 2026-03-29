#ifndef LAYOUTUTILS_H
#define LAYOUTUTILS_H

class QFrame;
class QHBoxLayout;
class QLayout;
class QLabel;
class QString;
class QWidget;
class ModernComboBox;

namespace LayoutUtils {




QHBoxLayout *createInfoRow(const QString &label, QLabel *&valueLabel);


ModernComboBox *createStyledCombo(QWidget *parent);


QFrame *createSeparator(QWidget *parent);


QLabel *createSectionLabel(const QString &text, QWidget *parent);


void clearLayout(QLayout *layout);

} 

#endif 
