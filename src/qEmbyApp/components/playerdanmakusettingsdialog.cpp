#include "playerdanmakusettingsdialog.h"

#include "danmakuoptionslider.h"
#include "moderntaginput.h"
#include "modernswitch.h"
#include "../managers/thememanager.h"

#include <config/config_keys.h>
#include <config/configstore.h>

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
QLabel *createAdaptiveIconLabel(QWidget *parent, const QString &iconPath,
                                int boxSize = 28, int iconSize = 14)
{
    auto *iconLabel = new QLabel(parent);
    iconLabel->setObjectName("playerDanmakuTileIcon");
    iconLabel->setFixedSize(boxSize, boxSize);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setProperty("compactIcon", boxSize < 28);

    auto updateIcon = [iconLabel, iconPath, iconSize]() {
        iconLabel->setPixmap(
            ThemeManager::getAdaptiveIcon(iconPath).pixmap(iconSize, iconSize));
    };
    updateIcon();

    QObject::connect(ThemeManager::instance(), &ThemeManager::themeChanged,
                     iconLabel, [updateIcon](ThemeManager::Theme) {
                         updateIcon();
                     });
    return iconLabel;
}
} 

PlayerDanmakuSettingsDialog::PlayerDanmakuSettingsDialog(QWidget *parent)
    : PlayerOverlayDialog(parent)
{
    setSurfaceObjectName("playerDanmakuSettingsDialog");
    setSurfacePreferredSize(QSize(620, 560));
    setTitle(tr("Danmaku Settings"));

    contentLayout()->setContentsMargins(16, 8, 16, 16);
    contentLayout()->setSpacing(10);

    m_liveReloadTimer = new QTimer(this);
    m_liveReloadTimer->setSingleShot(true);
    m_liveReloadTimer->setInterval(140);
    connect(m_liveReloadTimer, &QTimer::timeout, this, [this]() {
        emit liveReloadRequested();
        m_requiresReload = false;
    });

    auto bindSwitch =
        [this](ModernSwitch *control, const QString &configKey,
               const QVariant &defaultValue, bool affectsRender) {
            if (!control)
            {
                return;
            }

            auto *store = ConfigStore::instance();
            control->setChecked(store->get<bool>(configKey,
                                                 defaultValue.toBool()));

            connect(control, &ModernSwitch::toggled, this,
                    [this, store, configKey, affectsRender](bool checked) {
                        store->set(configKey, checked);
                        if (affectsRender)
                        {
                            m_requiresReload = true;
                            scheduleLiveReload();
                        }
                    });

            connect(store, &ConfigStore::valueChanged, control,
                    [control, configKey](const QString &key,
                                         const QVariant &newValue) {
                        if (key != configKey || !control)
                        {
                            return;
                        }

                        QSignalBlocker blocker(control);
                        control->setChecked(newValue.toBool());
                    });
        };

    auto bindTagInput =
        [this](ModernTagInput *control, const QString &configKey,
               bool affectsRender) {
            if (!control)
            {
                return;
            }

            auto *store = ConfigStore::instance();
            control->setPopupMode(ModernTagInput::ForcePopup);
            control->setMaxPopupHeight(260);
            control->setValue(store->get<QString>(configKey));

            connect(control, &ModernTagInput::valueChanged, this,
                    [this, store, configKey, affectsRender](
                        const QString &value) {
                        store->set(configKey, value);
                        if (affectsRender)
                        {
                            m_requiresReload = true;
                            scheduleLiveReload();
                        }
                    });

            connect(store, &ConfigStore::valueChanged, control,
                    [control, configKey](const QString &key,
                                         const QVariant &newValue) {
                        if (key != configKey || !control)
                        {
                            return;
                        }

                        QSignalBlocker blocker(control);
                        control->setValue(newValue.toString());
                    });
        };

    auto createSwitchTile =
        [this, &bindSwitch](const QString &iconPath, const QString &title,
                            const QString &description, const QString &configKey,
                            const QVariant &defaultValue, bool affectsRender,
                            const QString &objectName) {
            const bool isCompactToggleTile =
                description.isEmpty() &&
                objectName == QLatin1String("playerDanmakuToggleTile");

            auto *tile = new QFrame(this);
            tile->setObjectName(objectName);
            tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

            auto *layout = new QHBoxLayout(tile);
            layout->setContentsMargins(isCompactToggleTile ? 10 : 14,
                                       isCompactToggleTile ? 10 : 12,
                                       isCompactToggleTile ? 10 : 14,
                                       isCompactToggleTile ? 10 : 12);
            layout->setSpacing(isCompactToggleTile ? 8 : 12);

            layout->addWidget(createAdaptiveIconLabel(tile, iconPath,
                                                      isCompactToggleTile ? 22 : 28,
                                                      isCompactToggleTile ? 11 : 14),
                              0,
                              isCompactToggleTile ? Qt::AlignVCenter
                                                  : Qt::AlignTop);

            auto *textContainer = new QWidget(tile);
            textContainer->setSizePolicy(QSizePolicy::Expanding,
                                         QSizePolicy::Preferred);
            auto *textLayout = new QVBoxLayout(textContainer);
            textLayout->setContentsMargins(0, 0, 0, 0);
            textLayout->setSpacing(description.isEmpty() ? 0 : 3);

            auto *titleLabel = new QLabel(title, textContainer);
            titleLabel->setObjectName("playerDanmakuTileTitle");
            titleLabel->setWordWrap(!isCompactToggleTile);
            titleLabel->setAlignment(isCompactToggleTile ? Qt::AlignVCenter
                                                         : Qt::AlignLeft);
            textLayout->addWidget(titleLabel);

            if (!description.isEmpty())
            {
                auto *descLabel = new QLabel(description, textContainer);
                descLabel->setObjectName("playerDanmakuTileDesc");
                descLabel->setWordWrap(true);
                textLayout->addWidget(descLabel);
            }

            layout->addWidget(textContainer, 1);

            auto *control = new ModernSwitch(tile);
            bindSwitch(control, configKey, defaultValue, affectsRender);
            layout->addWidget(control, 0, Qt::AlignVCenter);

            return tile;
        };

    auto createSliderTile =
        [this](const QString &iconPath, const QString &title,
               DanmakuOptionUtils::SliderKind kind, const QString &configKey) {
            auto *tile = new QFrame(this);
            tile->setObjectName("playerDanmakuSliderTile");
            tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

            auto *layout = new QVBoxLayout(tile);
            layout->setContentsMargins(14, 12, 14, 12);
            layout->setSpacing(8);

            auto *headerRow = new QHBoxLayout();
            headerRow->setContentsMargins(0, 0, 0, 0);
            headerRow->setSpacing(10);

            headerRow->addWidget(createAdaptiveIconLabel(tile, iconPath), 0,
                                 Qt::AlignTop);

            auto *titleLabel = new QLabel(title, tile);
            titleLabel->setObjectName("playerDanmakuTileTitle");
            titleLabel->setWordWrap(true);
            headerRow->addWidget(titleLabel, 1, Qt::AlignVCenter);
            layout->addLayout(headerRow);

            auto *control = new DanmakuOptionSlider(kind, configKey, tile);
            control->setMinimumWidth(0);
            control->setMaximumWidth(QWIDGETSIZE_MAX);
            control->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            control->setCompactMode(true);
            control->setAdaptiveRangeLabelPlacementEnabled(true);
            connect(control, &DanmakuOptionSlider::valueChanged, this,
                    [this](int) {
                        m_requiresReload = true;
                        scheduleLiveReload();
                    });
            layout->addWidget(control);

            return tile;
        };

    auto createTagInputTile =
        [this, &bindTagInput](const QString &iconPath, const QString &title,
                              const QString &configKey) {
            auto *tile = new QFrame(this);
            tile->setObjectName("playerDanmakuInputTile");
            tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

            auto *layout = new QHBoxLayout(tile);
            layout->setContentsMargins(10, 10, 10, 10);
            layout->setSpacing(8);

            layout->addWidget(createAdaptiveIconLabel(tile, iconPath, 22, 11), 0,
                              Qt::AlignVCenter);

            auto *titleLabel = new QLabel(title, tile);
            titleLabel->setObjectName("playerDanmakuTileTitle");
            titleLabel->setWordWrap(false);
            layout->addWidget(titleLabel, 0, Qt::AlignVCenter);

            auto *input = new ModernTagInput(tile);
            input->setMinimumWidth(320);
            input->setMaximumWidth(QWIDGETSIZE_MAX);
            input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            bindTagInput(input, configKey, true);
            layout->addWidget(input, 1, Qt::AlignVCenter);

            return tile;
        };

    auto *summaryCard = new QFrame(this);
    summaryCard->setObjectName("playerDanmakuSummaryCard");
    auto *summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(14, 12, 14, 12);
    summaryLayout->setSpacing(0);

    auto *promptLabel = new QLabel(
        tr("Adjust danmaku rendering without leaving playback. Changes are saved and applied immediately."),
        summaryCard);
    promptLabel->setObjectName("playerDanmakuSummaryText");
    promptLabel->setWordWrap(true);
    summaryLayout->addWidget(promptLabel);
    contentLayout()->addWidget(summaryCard);

    auto *sliderGrid = new QGridLayout();
    sliderGrid->setContentsMargins(0, 0, 0, 0);
    sliderGrid->setHorizontalSpacing(10);
    sliderGrid->setVerticalSpacing(10);
    sliderGrid->setColumnStretch(0, 1);
    sliderGrid->setColumnStretch(1, 1);
    sliderGrid->setColumnStretch(2, 1);

    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/danmaku-opacity.svg",
                         tr("Danmaku Opacity"),
                         DanmakuOptionUtils::SliderKind::Opacity,
                         ConfigKeys::PlayerDanmakuOpacity),
        0, 0);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/danmaku-size.svg", tr("Danmaku Size"),
                         DanmakuOptionUtils::SliderKind::FontScale,
                         ConfigKeys::PlayerDanmakuFontScale),
        0, 1);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/danmaku-area.svg", tr("Display Area"),
                         DanmakuOptionUtils::SliderKind::Area,
                         ConfigKeys::PlayerDanmakuAreaPercent),
        0, 2);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/danmaku-density.svg",
                         tr("Danmaku Density"),
                         DanmakuOptionUtils::SliderKind::Density,
                         ConfigKeys::PlayerDanmakuDensity),
        1, 0);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/danmaku-speed.svg", tr("Danmaku Speed"),
                         DanmakuOptionUtils::SliderKind::SpeedScale,
                         ConfigKeys::PlayerDanmakuSpeedScale),
        1, 1);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/danmaku-offset.svg", tr("Danmaku Offset"),
                         DanmakuOptionUtils::SliderKind::OffsetMs,
                         ConfigKeys::PlayerDanmakuOffsetMs),
        1, 2);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/appearance-font-size.svg",
                         tr("Danmaku Font Weight"),
                         DanmakuOptionUtils::SliderKind::FontWeight,
                         ConfigKeys::PlayerDanmakuFontWeight),
        2, 0);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/window-player.svg", tr("Danmaku Outline"),
                         DanmakuOptionUtils::SliderKind::OutlineSize,
                         ConfigKeys::PlayerDanmakuOutlineSize),
        2, 1);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/direct-stream.svg", tr("Danmaku Shadow"),
                         DanmakuOptionUtils::SliderKind::ShadowOffset,
                         ConfigKeys::PlayerDanmakuShadowOffset),
        2, 2);
    contentLayout()->addLayout(sliderGrid);

    auto *filterGrid = new QGridLayout();
    filterGrid->setContentsMargins(0, 0, 0, 0);
    filterGrid->setHorizontalSpacing(10);
    filterGrid->setVerticalSpacing(0);
    filterGrid->setColumnStretch(0, 1);
    filterGrid->setColumnStretch(1, 1);
    filterGrid->setColumnStretch(2, 1);

    filterGrid->addWidget(
        createSwitchTile(":/svg/dark/danmaku-hide-scroll.svg",
                         tr("Hide Scrolling Danmaku"), QString(),
                         ConfigKeys::PlayerDanmakuHideScroll, QVariant(false),
                         true, QStringLiteral("playerDanmakuToggleTile")),
        0, 0);
    filterGrid->addWidget(
        createSwitchTile(":/svg/dark/danmaku-hide-top.svg",
                         tr("Hide Top Danmaku"), QString(),
                         ConfigKeys::PlayerDanmakuHideTop, QVariant(false),
                         true, QStringLiteral("playerDanmakuToggleTile")),
        0, 1);
    filterGrid->addWidget(
        createSwitchTile(":/svg/dark/danmaku-hide-bottom.svg",
                         tr("Hide Bottom Danmaku"), QString(),
                         ConfigKeys::PlayerDanmakuHideBottom, QVariant(false),
                         true, QStringLiteral("playerDanmakuToggleTile")),
        0, 2);
    contentLayout()->addLayout(filterGrid);

    contentLayout()->addWidget(
        createTagInputTile(":/svg/dark/danmaku-blocked.svg",
                           tr("Blocked Keywords"),
                           ConfigKeys::PlayerDanmakuBlockedKeywords));

    contentLayout()->addStretch();
}

bool PlayerDanmakuSettingsDialog::requiresReload() const
{
    return m_requiresReload;
}

void PlayerDanmakuSettingsDialog::scheduleLiveReload()
{
    if (m_liveReloadTimer)
    {
        m_liveReloadTimer->start();
    }
}
