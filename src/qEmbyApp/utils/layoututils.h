#ifndef LAYOUTUTILS_H
#define LAYOUTUTILS_H

class QFrame;
class QHBoxLayout;
class QLayout;
class QLabel;
class QString;
class QSize;
class QWidget;
class ModernComboBox;

namespace LayoutUtils {


struct DetailSectionMetrics {
    static constexpr int HorizontalMargin = 40;
    static constexpr int SectionSpacing = 24;
    static constexpr int ContentSpacing = 12;
};




QHBoxLayout *createInfoRow(const QString &label, QLabel *&valueLabel);


ModernComboBox *createStyledCombo(QWidget *parent);


QFrame *createSeparator(QWidget *parent);


QLabel *createSectionLabel(const QString &text, QWidget *parent);


void clearLayout(QLayout *layout);



void fixWindowToContents(QWidget *window, const QSize &preferredSize);

} 

#endif 
