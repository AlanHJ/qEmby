#ifndef SUBTITLEOPTIONUTILS_H
#define SUBTITLEOPTIONUTILS_H

#include <QString>
#include <QStringList>

namespace SubtitleOptionUtils {

enum class SliderKind {
    DelayMs,
    FontSize,
    Position,
    OutlineSize,
    ShadowOffset,
    ScalePercent
};

struct SliderSpec {
    int minimum = 0;
    int maximum = 100;
    int singleStep = 1;
    int pageStep = 10;
    int defaultValue = 0;
};

SliderSpec sliderSpec(SliderKind kind);
int clampSliderValue(SliderKind kind, int value);
QString formatSliderValue(SliderKind kind, int value);
bool parseSliderValue(SliderKind kind, QString text, int &value);

QString defaultFontFamily();
QString normalizeFontFamily(QString value);
QStringList availableFontFamilies();

} 

#endif 
