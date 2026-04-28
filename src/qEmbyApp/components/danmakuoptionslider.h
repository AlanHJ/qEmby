#ifndef DANMAKUOPTIONSLIDER_H
#define DANMAKUOPTIONSLIDER_H

#include "../utils/danmakuoptionutils.h"

#include <QVariant>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class ModernSlider;
class EditableValueLabel;
class QResizeEvent;
class QVBoxLayout;

class DanmakuOptionSlider : public QWidget
{
    Q_OBJECT

public:
    explicit DanmakuOptionSlider(DanmakuOptionUtils::SliderKind kind,
                                 QString configKey,
                                 QWidget *parent = nullptr);

    int value() const;
    void setCompactMode(bool compact);
    bool isCompactMode() const;
    void setAdaptiveRangeLabelPlacementEnabled(bool enabled);
    bool isAdaptiveRangeLabelPlacementEnabled() const;

signals:
    void valueChanged(int value);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void handleSliderValueChanged(int value);
    void handleSliderReleased();
    void handleConfigValueChanged(const QString &key, const QVariant &newValue);
    void handleValueTextSubmitted(const QString &text);

private:
    enum class RangeLabelPlacement {
        Bottom,
        Top,
        Side
    };

    void applySpec();
    void refreshLabels();
    void syncFromStore();
    void setCurrentValue(int value, bool persistToStore, bool notify);
    void updateAdaptiveLayout();
    void applyRangeLabelPlacement(RangeLabelPlacement placement);
    void updateRangeLabels(const QString &minimumText, const QString &maximumText);
    QString formatRangeHintValue(int value) const;
    int sliderPreferredHeight() const;
    int sideSliderMinimumWidth() const;
    static int variantToInt(const QVariant &value, int fallback);

    DanmakuOptionUtils::SliderKind m_kind;
    QString m_configKey;
    QVBoxLayout *m_mainLayout = nullptr;
    QWidget *m_topRow = nullptr;
    QWidget *m_middleRow = nullptr;
    QWidget *m_bottomRow = nullptr;
    QHBoxLayout *m_topLayout = nullptr;
    QHBoxLayout *m_middleLayout = nullptr;
    QHBoxLayout *m_bottomLayout = nullptr;
    EditableValueLabel *m_valueLabel = nullptr;
    QLabel *m_topMinLabel = nullptr;
    QLabel *m_topMaxLabel = nullptr;
    QLabel *m_sideMinLabel = nullptr;
    QLabel *m_sideMaxLabel = nullptr;
    QLabel *m_bottomMinLabel = nullptr;
    QLabel *m_bottomMaxLabel = nullptr;
    ModernSlider *m_slider = nullptr;
    RangeLabelPlacement m_rangeLabelPlacement = RangeLabelPlacement::Bottom;
    bool m_compactMode = false;
    bool m_adaptiveRangeLabelPlacementEnabled = false;
};

#endif 
