#include "danmakuoptionslider.h"

#include "editablevaluelabel.h"
#include "modernslider.h"
#include <config/configstore.h>

#include <QResizeEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVariant>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {

QLabel *createHintLabel(QWidget *parent, Qt::Alignment alignment)
{
    auto *label = new QLabel(parent);
    label->setObjectName("DanmakuOptionSliderHint");
    label->setAlignment(alignment);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return label;
}

QString stripTrailingZeros(QString text)
{
    if (!text.contains(QLatin1Char('.'))) {
        return text;
    }

    while (text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    return text;
}

} 

DanmakuOptionSlider::DanmakuOptionSlider(DanmakuOptionUtils::SliderKind kind,
                                         QString configKey,
                                         QWidget *parent)
    : QWidget(parent), m_kind(kind), m_configKey(configKey)
{
    setObjectName("DanmakuOptionSlider");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(240);
    setMaximumWidth(320);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(4);

    m_topRow = new QWidget(this);
    m_topLayout = new QHBoxLayout(m_topRow);
    m_topLayout->setContentsMargins(0, 0, 0, 0);
    m_topLayout->setSpacing(8);

    m_topMinLabel = createHintLabel(m_topRow, Qt::AlignLeft | Qt::AlignVCenter);
    m_topLayout->addWidget(m_topMinLabel, 1);

    m_valueLabel = new EditableValueLabel(QStringLiteral("DanmakuOptionSliderValue"),
                                          m_topRow);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_topLayout->addWidget(m_valueLabel, 1);

    m_topMaxLabel = createHintLabel(m_topRow, Qt::AlignRight | Qt::AlignVCenter);
    m_topLayout->addWidget(m_topMaxLabel, 1);

    m_mainLayout->addWidget(m_topRow);

    m_middleRow = new QWidget(this);
    m_middleLayout = new QHBoxLayout(m_middleRow);
    m_middleLayout->setContentsMargins(0, 0, 0, 0);
    m_middleLayout->setSpacing(8);

    m_sideMinLabel = createHintLabel(m_middleRow,
                                     Qt::AlignLeft | Qt::AlignVCenter);
    m_middleLayout->addWidget(m_sideMinLabel, 0, Qt::AlignVCenter);

    m_slider = new ModernSlider(Qt::Horizontal, m_middleRow);
    m_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_slider->setFixedHeight(sliderPreferredHeight());
    m_middleLayout->addWidget(m_slider, 1);

    m_sideMaxLabel = createHintLabel(m_middleRow,
                                     Qt::AlignRight | Qt::AlignVCenter);
    m_middleLayout->addWidget(m_sideMaxLabel, 0, Qt::AlignVCenter);

    m_mainLayout->addWidget(m_middleRow);

    m_bottomRow = new QWidget(this);
    m_bottomLayout = new QHBoxLayout(m_bottomRow);
    m_bottomLayout->setContentsMargins(0, 0, 0, 0);
    m_bottomLayout->setSpacing(8);

    m_bottomMinLabel = createHintLabel(m_bottomRow,
                                       Qt::AlignLeft | Qt::AlignVCenter);
    m_bottomLayout->addWidget(m_bottomMinLabel, 1);

    m_bottomMaxLabel = createHintLabel(m_bottomRow,
                                       Qt::AlignRight | Qt::AlignVCenter);
    m_bottomLayout->addWidget(m_bottomMaxLabel, 1);

    m_mainLayout->addWidget(m_bottomRow);

    applySpec();
    syncFromStore();

    connect(m_slider, &QSlider::valueChanged, this,
            &DanmakuOptionSlider::handleSliderValueChanged);
    connect(m_slider, &QSlider::sliderReleased, this,
            &DanmakuOptionSlider::handleSliderReleased);
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            &DanmakuOptionSlider::handleConfigValueChanged);
    connect(m_valueLabel, &EditableValueLabel::textSubmitted, this,
            &DanmakuOptionSlider::handleValueTextSubmitted);
    setCompactMode(false);
}

int DanmakuOptionSlider::value() const
{
    return m_slider ? m_slider->value() : 0;
}

void DanmakuOptionSlider::setCompactMode(bool compact)
{
    if (m_mainLayout) {
        m_mainLayout->setSpacing(compact ? 2 : 4);
    }
    if (m_topLayout) {
        m_topLayout->setSpacing(compact ? 6 : 8);
    }
    if (m_middleLayout) {
        m_middleLayout->setSpacing(compact ? 6 : 8);
    }
    if (m_bottomLayout) {
        m_bottomLayout->setSpacing(compact ? 6 : 8);
    }
    if (m_slider) {
        m_slider->setFixedHeight(compact ? 20 : 24);
    }

    m_compactMode = compact;
    updateAdaptiveLayout();
    updateGeometry();
}

bool DanmakuOptionSlider::isCompactMode() const
{
    return m_compactMode;
}

void DanmakuOptionSlider::setAdaptiveRangeLabelPlacementEnabled(bool enabled)
{
    m_adaptiveRangeLabelPlacementEnabled = enabled;
    updateAdaptiveLayout();
    updateGeometry();
}

bool DanmakuOptionSlider::isAdaptiveRangeLabelPlacementEnabled() const
{
    return m_adaptiveRangeLabelPlacementEnabled;
}

void DanmakuOptionSlider::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_adaptiveRangeLabelPlacementEnabled) {
        updateAdaptiveLayout();
    }
}

void DanmakuOptionSlider::handleSliderValueChanged(int value)
{
    setCurrentValue(value, true, false);

    
    
    if (!m_slider || !m_slider->isSliderDown()) {
        emit valueChanged(this->value());
    }
}

void DanmakuOptionSlider::handleSliderReleased()
{
    emit valueChanged(value());
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
    updateRangeLabels(formatRangeHintValue(spec.minimum),
                      formatRangeHintValue(spec.maximum));
    updateAdaptiveLayout();
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

void DanmakuOptionSlider::updateAdaptiveLayout()
{
    if (!m_valueLabel || !m_slider || !m_topMinLabel || !m_topMaxLabel ||
        !m_sideMinLabel || !m_sideMaxLabel || !m_bottomMinLabel ||
        !m_bottomMaxLabel) {
        return;
    }

    if (!m_adaptiveRangeLabelPlacementEnabled) {
        applyRangeLabelPlacement(RangeLabelPlacement::Bottom);
        return;
    }

    const int spacing = m_mainLayout ? m_mainLayout->spacing() : 0;
    const int sliderHeight = sliderPreferredHeight();
    const int valueHeight = m_valueLabel->sizeHint().height();
    const int hintHeight = std::max(
        {m_topMinLabel->sizeHint().height(), m_topMaxLabel->sizeHint().height(),
         m_sideMinLabel->sizeHint().height(), m_sideMaxLabel->sizeHint().height(),
         m_bottomMinLabel->sizeHint().height(),
         m_bottomMaxLabel->sizeHint().height()});

    const int availableHeight =
        height() > 0 ? height()
                     : (m_compactMode ? valueHeight + spacing + sliderHeight
                                      : valueHeight + spacing + sliderHeight +
                                            spacing + hintHeight);
    const int availableWidth =
        width() > 0 ? width() : qMax(minimumWidth(), 240);

    const int topRequiredHeight = qMax(valueHeight, hintHeight) + spacing +
                                  sliderHeight;
    const int bottomRequiredHeight =
        valueHeight + spacing + sliderHeight + spacing + hintHeight;
    const int sideRequiredHeight =
        valueHeight + spacing + qMax(sliderHeight, hintHeight);

    const int topRequiredWidth =
        m_topMinLabel->sizeHint().width() + m_valueLabel->minimumSizeHint().width() +
        m_topMaxLabel->sizeHint().width() +
        (m_topLayout ? m_topLayout->spacing() * 2 : 0);
    const int sideRequiredWidth =
        m_sideMinLabel->sizeHint().width() + sideSliderMinimumWidth() +
        m_sideMaxLabel->sizeHint().width() +
        (m_middleLayout ? m_middleLayout->spacing() * 2 : 0);

    const bool canShowBottom = availableHeight >= bottomRequiredHeight;
    const bool canShowTop =
        availableHeight >= topRequiredHeight &&
        availableWidth >= topRequiredWidth;
    const bool canShowSide =
        availableHeight >= sideRequiredHeight &&
        availableWidth >= sideRequiredWidth;

    RangeLabelPlacement placement = RangeLabelPlacement::Bottom;
    if (m_compactMode) {
        
        if (canShowTop) {
            placement = RangeLabelPlacement::Top;
        } else {
            placement = RangeLabelPlacement::Side;
        }
    } else {
        if (!canShowBottom) {
            placement = canShowTop ? RangeLabelPlacement::Top
                                   : RangeLabelPlacement::Side;
        }
        if (placement == RangeLabelPlacement::Side && !canShowSide && canShowTop) {
            placement = RangeLabelPlacement::Top;
        }
    }

    applyRangeLabelPlacement(placement);
}

void DanmakuOptionSlider::applyRangeLabelPlacement(
    RangeLabelPlacement placement)
{
    m_rangeLabelPlacement = placement;

    const bool showTopHints = placement == RangeLabelPlacement::Top;
    const bool showSideHints = placement == RangeLabelPlacement::Side;
    const bool showBottomHints = placement == RangeLabelPlacement::Bottom;

    if (m_topMinLabel) {
        m_topMinLabel->setVisible(showTopHints);
    }
    if (m_topMaxLabel) {
        m_topMaxLabel->setVisible(showTopHints);
    }
    if (m_sideMinLabel) {
        m_sideMinLabel->setVisible(showSideHints);
    }
    if (m_sideMaxLabel) {
        m_sideMaxLabel->setVisible(showSideHints);
    }
    if (m_bottomRow) {
        m_bottomRow->setVisible(showBottomHints);
    }
    if (m_slider) {
        m_slider->setMinimumWidth(showSideHints ? sideSliderMinimumWidth() : 0);
    }

    updateGeometry();
}

void DanmakuOptionSlider::updateRangeLabels(const QString &minimumText,
                                            const QString &maximumText)
{
    if (m_topMinLabel) {
        m_topMinLabel->setText(minimumText);
    }
    if (m_sideMinLabel) {
        m_sideMinLabel->setText(minimumText);
    }
    if (m_bottomMinLabel) {
        m_bottomMinLabel->setText(minimumText);
    }
    if (m_topMaxLabel) {
        m_topMaxLabel->setText(maximumText);
    }
    if (m_sideMaxLabel) {
        m_sideMaxLabel->setText(maximumText);
    }
    if (m_bottomMaxLabel) {
        m_bottomMaxLabel->setText(maximumText);
    }
}

int DanmakuOptionSlider::sliderPreferredHeight() const
{
    return m_compactMode ? 20 : 24;
}

int DanmakuOptionSlider::sideSliderMinimumWidth() const
{
    return m_compactMode ? 48 : 96;
}

QString DanmakuOptionSlider::formatRangeHintValue(int value) const
{
    if (!m_compactMode) {
        return DanmakuOptionUtils::formatSliderValue(m_kind, value);
    }

    if (m_kind == DanmakuOptionUtils::SliderKind::OffsetMs) {
        if (value == 0) {
            return QStringLiteral("0");
        }

        const double seconds = value / 1000.0;
        const bool isIntegerSecond = std::fmod(std::fabs(seconds), 1.0) < 0.001;
        QString text = stripTrailingZeros(
            QString::number(seconds, 'f', isIntegerSecond ? 0 : 1));
        if (value > 0) {
            text.prepend(QLatin1Char('+'));
        }
        return text + QStringLiteral("s");
    }

    return DanmakuOptionUtils::formatSliderValue(m_kind, value);
}
