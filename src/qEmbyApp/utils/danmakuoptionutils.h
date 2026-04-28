#ifndef DANMAKUOPTIONUTILS_H
#define DANMAKUOPTIONUTILS_H

#include <QString>

namespace DanmakuOptionUtils {

enum class SliderKind {
    Opacity,
    FontScale,
    FontWeight,
    OutlineSize,
    ShadowOffset,
    Area,
    Density,
    SpeedScale,
    OffsetMs
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

} 

#endif 
