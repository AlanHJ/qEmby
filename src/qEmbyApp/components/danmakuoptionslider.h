#ifndef DANMAKUOPTIONSLIDER_H
#define DANMAKUOPTIONSLIDER_H

#include "../utils/danmakuoptionutils.h"

#include <QVariant>
#include <QWidget>

class QLabel;
class ModernSlider;
class EditableValueLabel;

class DanmakuOptionSlider : public QWidget
{
    Q_OBJECT

public:
    explicit DanmakuOptionSlider(DanmakuOptionUtils::SliderKind kind,
                                 QString configKey,
                                 QWidget *parent = nullptr);

    int value() const;

signals:
    void valueChanged(int value);

private slots:
    void handleSliderValueChanged(int value);
    void handleConfigValueChanged(const QString &key, const QVariant &newValue);
    void handleValueTextSubmitted(const QString &text);

private:
    void applySpec();
    void refreshLabels();
    void syncFromStore();
    void setCurrentValue(int value, bool persistToStore, bool notify);
    static int variantToInt(const QVariant &value, int fallback);

    DanmakuOptionUtils::SliderKind m_kind;
    QString m_configKey;
    EditableValueLabel *m_valueLabel = nullptr;
    QLabel *m_minLabel = nullptr;
    QLabel *m_maxLabel = nullptr;
    ModernSlider *m_slider = nullptr;
};

#endif 
