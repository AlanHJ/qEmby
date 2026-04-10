#include "subtitleoptionslider.h"

#include "editablevaluelabel.h"
#include "modernslider.h"
#include <config/configstore.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVariant>
#include <QVBoxLayout>

SubtitleOptionSlider::SubtitleOptionSlider(SubtitleOptionUtils::SliderKind kind,
                                           QString configKey,
                                           QWidget *parent)
    : QWidget(parent), m_kind(kind), m_configKey(configKey)
{
    setObjectName("SubtitleOptionSlider");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(240);
    setMaximumWidth(320);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    m_valueLabel = new EditableValueLabel(
        QStringLiteral("SubtitleOptionSliderValue"), this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_valueLabel);

    m_slider = new ModernSlider(Qt::Horizontal, this);
    m_slider->setFixedHeight(24);
    mainLayout->addWidget(m_slider);

    auto *hintLayout = new QHBoxLayout();
    hintLayout->setContentsMargins(0, 0, 0, 0);
    hintLayout->setSpacing(12);

    m_minLabel = new QLabel(this);
    m_minLabel->setObjectName("SubtitleOptionSliderHint");
    hintLayout->addWidget(m_minLabel, 0, Qt::AlignLeft);

    hintLayout->addStretch();

    m_maxLabel = new QLabel(this);
    m_maxLabel->setObjectName("SubtitleOptionSliderHint");
    hintLayout->addWidget(m_maxLabel, 0, Qt::AlignRight);

    mainLayout->addLayout(hintLayout);

    applySpec();
    syncFromStore();

    connect(m_slider, &QSlider::valueChanged, this,
            &SubtitleOptionSlider::handleSliderValueChanged);
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            &SubtitleOptionSlider::handleConfigValueChanged);
    connect(m_valueLabel, &EditableValueLabel::textSubmitted, this,
            &SubtitleOptionSlider::handleValueTextSubmitted);
}

int SubtitleOptionSlider::value() const
{
    return m_slider ? m_slider->value() : 0;
}

void SubtitleOptionSlider::handleSliderValueChanged(int value)
{
    setCurrentValue(value, true, true);
}

void SubtitleOptionSlider::handleConfigValueChanged(const QString &key,
                                                    const QVariant &newValue)
{
    if (key != m_configKey || !m_slider) {
        return;
    }

    const SubtitleOptionUtils::SliderSpec spec =
        SubtitleOptionUtils::sliderSpec(m_kind);
    const int resolvedValue = SubtitleOptionUtils::clampSliderValue(
        m_kind, variantToInt(newValue, spec.defaultValue));

    setCurrentValue(resolvedValue, false, false);
}

void SubtitleOptionSlider::handleValueTextSubmitted(const QString &text)
{
    int resolvedValue = 0;
    if (!SubtitleOptionUtils::parseSliderValue(m_kind, text, resolvedValue)) {
        refreshLabels();
        return;
    }

    setCurrentValue(resolvedValue, true, true);
}

void SubtitleOptionSlider::applySpec()
{
    const SubtitleOptionUtils::SliderSpec spec =
        SubtitleOptionUtils::sliderSpec(m_kind);
    m_slider->setRange(spec.minimum, spec.maximum);
    m_slider->setSingleStep(spec.singleStep);
    m_slider->setPageStep(spec.pageStep);
    m_slider->setBufferValue(spec.minimum);
}

void SubtitleOptionSlider::refreshLabels()
{
    if (!m_slider || !m_valueLabel) {
        return;
    }

    const SubtitleOptionUtils::SliderSpec spec =
        SubtitleOptionUtils::sliderSpec(m_kind);
    m_valueLabel->setText(
        SubtitleOptionUtils::formatSliderValue(m_kind, m_slider->value()));
    m_minLabel->setText(
        SubtitleOptionUtils::formatSliderValue(m_kind, spec.minimum));
    m_maxLabel->setText(
        SubtitleOptionUtils::formatSliderValue(m_kind, spec.maximum));
}

void SubtitleOptionSlider::syncFromStore()
{
    const SubtitleOptionUtils::SliderSpec spec =
        SubtitleOptionUtils::sliderSpec(m_kind);

    int resolvedValue = spec.defaultValue;
    if (!m_configKey.trimmed().isEmpty()) {
        const QVariant value = ConfigStore::instance()->get<QVariant>(
            m_configKey, QVariant(spec.defaultValue));
        resolvedValue = SubtitleOptionUtils::clampSliderValue(
            m_kind, variantToInt(value, spec.defaultValue));
    }

    setCurrentValue(resolvedValue, false, false);
}

void SubtitleOptionSlider::setCurrentValue(int value, bool persistToStore,
                                           bool notify)
{
    if (!m_slider) {
        return;
    }

    const int resolvedValue =
        SubtitleOptionUtils::clampSliderValue(m_kind, value);
    if (m_slider->value() != resolvedValue) {
        QSignalBlocker blocker(m_slider);
        m_slider->setValue(resolvedValue);
    }

    refreshLabels();

    if (persistToStore && !m_configKey.trimmed().isEmpty()) {
        ConfigStore::instance()->set(m_configKey, resolvedValue);
    }

    if (notify) {
        emit valueChanged(resolvedValue);
    }
}

int SubtitleOptionSlider::variantToInt(const QVariant &value, int fallback)
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
