#include "danmakuoptionslider.h"

#include "editablevaluelabel.h"
#include "modernslider.h"
#include <config/configstore.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVariant>
#include <QVBoxLayout>

DanmakuOptionSlider::DanmakuOptionSlider(DanmakuOptionUtils::SliderKind kind,
                                         QString configKey,
                                         QWidget *parent)
    : QWidget(parent), m_kind(kind), m_configKey(configKey)
{
    setObjectName("DanmakuOptionSlider");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(240);
    setMaximumWidth(320);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    m_valueLabel = new EditableValueLabel(QStringLiteral("DanmakuOptionSliderValue"),
                                          this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_valueLabel);

    m_slider = new ModernSlider(Qt::Horizontal, this);
    m_slider->setFixedHeight(24);
    mainLayout->addWidget(m_slider);

    auto *hintLayout = new QHBoxLayout();
    hintLayout->setContentsMargins(0, 0, 0, 0);
    hintLayout->setSpacing(12);

    m_minLabel = new QLabel(this);
    m_minLabel->setObjectName("DanmakuOptionSliderHint");
    hintLayout->addWidget(m_minLabel, 0, Qt::AlignLeft);

    hintLayout->addStretch();

    m_maxLabel = new QLabel(this);
    m_maxLabel->setObjectName("DanmakuOptionSliderHint");
    hintLayout->addWidget(m_maxLabel, 0, Qt::AlignRight);

    mainLayout->addLayout(hintLayout);

    applySpec();
    syncFromStore();

    connect(m_slider, &QSlider::valueChanged, this,
            &DanmakuOptionSlider::handleSliderValueChanged);
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            &DanmakuOptionSlider::handleConfigValueChanged);
    connect(m_valueLabel, &EditableValueLabel::textSubmitted, this,
            &DanmakuOptionSlider::handleValueTextSubmitted);
}

int DanmakuOptionSlider::value() const
{
    return m_slider ? m_slider->value() : 0;
}

void DanmakuOptionSlider::handleSliderValueChanged(int value)
{
    setCurrentValue(value, true, true);
}

void DanmakuOptionSlider::handleConfigValueChanged(const QString &key,
                                                   const QVariant &newValue)
{
    if (key != m_configKey || !m_slider) {
        return;
    }

    const DanmakuOptionUtils::SliderSpec spec =
        DanmakuOptionUtils::sliderSpec(m_kind);
    const int resolvedValue = DanmakuOptionUtils::clampSliderValue(
        m_kind, variantToInt(newValue, spec.defaultValue));

    setCurrentValue(resolvedValue, false, false);
}

void DanmakuOptionSlider::handleValueTextSubmitted(const QString &text)
{
    int resolvedValue = 0;
    if (!DanmakuOptionUtils::parseSliderValue(m_kind, text, resolvedValue)) {
        refreshLabels();
        return;
    }

    setCurrentValue(resolvedValue, true, true);
}

void DanmakuOptionSlider::applySpec()
{
    const DanmakuOptionUtils::SliderSpec spec =
        DanmakuOptionUtils::sliderSpec(m_kind);
    m_slider->setRange(spec.minimum, spec.maximum);
    m_slider->setSingleStep(spec.singleStep);
    m_slider->setPageStep(spec.pageStep);
    m_slider->setBufferValue(spec.minimum);
}

void DanmakuOptionSlider::refreshLabels()
{
    if (!m_slider || !m_valueLabel) {
        return;
    }

    const DanmakuOptionUtils::SliderSpec spec =
        DanmakuOptionUtils::sliderSpec(m_kind);
    m_valueLabel->setText(
        DanmakuOptionUtils::formatSliderValue(m_kind, m_slider->value()));
    m_minLabel->setText(
        DanmakuOptionUtils::formatSliderValue(m_kind, spec.minimum));
    m_maxLabel->setText(
        DanmakuOptionUtils::formatSliderValue(m_kind, spec.maximum));
}

void DanmakuOptionSlider::syncFromStore()
{
    const DanmakuOptionUtils::SliderSpec spec =
        DanmakuOptionUtils::sliderSpec(m_kind);

    int resolvedValue = spec.defaultValue;
    if (!m_configKey.trimmed().isEmpty()) {
        const QVariant value = ConfigStore::instance()->get<QVariant>(
            m_configKey, QVariant(QString::number(spec.defaultValue)));
        resolvedValue =
            DanmakuOptionUtils::clampSliderValue(m_kind,
                                                 variantToInt(value, spec.defaultValue));
    }

    setCurrentValue(resolvedValue, false, false);
}

void DanmakuOptionSlider::setCurrentValue(int value, bool persistToStore,
                                          bool notify)
{
    if (!m_slider) {
        return;
    }

    const int resolvedValue =
        DanmakuOptionUtils::clampSliderValue(m_kind, value);
    if (m_slider->value() != resolvedValue) {
        QSignalBlocker blocker(m_slider);
        m_slider->setValue(resolvedValue);
    }

    refreshLabels();

    if (persistToStore && !m_configKey.trimmed().isEmpty()) {
        ConfigStore::instance()->set(m_configKey, QString::number(resolvedValue));
    }

    if (notify) {
        emit valueChanged(resolvedValue);
    }
}

int DanmakuOptionSlider::variantToInt(const QVariant &value, int fallback)
{
    bool ok = false;
    const int result = value.toInt(&ok);
    if (ok) {
        return result;
    }

    const QString text = value.toString().trimmed();
    const int parsed = text.toInt(&ok);
    return ok ? parsed : fallback;
}
