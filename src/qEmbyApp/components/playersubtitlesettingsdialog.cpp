#include "playersubtitlesettingsdialog.h"

#include "moderncombobox.h"
#include "subtitleoptionslider.h"
#include "../managers/thememanager.h"
#include "../utils/subtitleoptionutils.h"

#include <config/config_keys.h>
#include <config/configstore.h>

#include <QComboBox>
#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace
{

QLabel *createAdaptiveIconLabel(QWidget *parent, const QString &iconPath)
{
    auto *iconLabel = new QLabel(parent);
    iconLabel->setObjectName("playerSubtitleTileIcon");
    iconLabel->setFixedSize(28, 28);
    iconLabel->setAlignment(Qt::AlignCenter);

    auto updateIcon = [iconLabel, iconPath]() {
        iconLabel->setPixmap(
            ThemeManager::getAdaptiveIcon(iconPath).pixmap(14, 14));
    };
    updateIcon();

    QObject::connect(ThemeManager::instance(), &ThemeManager::themeChanged,
                     iconLabel, [updateIcon](ThemeManager::Theme) {
                         updateIcon();
                     });
    return iconLabel;
}

ModernComboBox *createSubtitleFontComboBox(QWidget *parent)
{
    auto *combo = new ModernComboBox(parent);
    combo->setObjectName("playerSubtitleFontCombo");
    combo->setProperty("flush-right-scrollbar", true);
    combo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    combo->setMinimumWidth(248);
    combo->setMaximumWidth(320);
    combo->setMaxTextWidth(220);
    combo->addItem(QCoreApplication::translate("PlayerSubtitleSettingsDialog",
                                               "Auto"),
                   SubtitleOptionUtils::defaultFontFamily());

    const QStringList families = SubtitleOptionUtils::availableFontFamilies();
    for (const QString &family : families) {
        combo->addItem(family, family);
    }

    const QString currentFont = SubtitleOptionUtils::normalizeFontFamily(
        ConfigStore::instance()->get<QString>(
            ConfigKeys::PlayerSubtitleFont,
            SubtitleOptionUtils::defaultFontFamily()));
    if (combo->findData(currentFont) < 0) {
        combo->addItem(currentFont, currentFont);
    }

    const int currentIndex = combo->findData(currentFont);
    if (currentIndex >= 0) {
        combo->setCurrentIndex(currentIndex);
    }

    auto *store = ConfigStore::instance();
    QObject::connect(combo,
                     QOverload<int>::of(&QComboBox::currentIndexChanged), combo,
                     [combo, store](int index) {
                         store->set(ConfigKeys::PlayerSubtitleFont,
                                    combo->itemData(index).toString());
                     });
    QObject::connect(store, &ConfigStore::valueChanged, combo,
                     [combo](const QString &key, const QVariant &newValue) {
                         if (key != QLatin1String(ConfigKeys::PlayerSubtitleFont) ||
                             !combo) {
                             return;
                         }

                         const QString resolvedFont =
                             SubtitleOptionUtils::normalizeFontFamily(
                                 newValue.toString());
                         if (combo->findData(resolvedFont) < 0) {
                             combo->addItem(resolvedFont, resolvedFont);
                         }

                         QSignalBlocker blocker(combo);
                         const int index = combo->findData(resolvedFont);
                         if (index >= 0) {
                             combo->setCurrentIndex(index);
                         }
                     });

    return combo;
}

} 

PlayerSubtitleSettingsDialog::PlayerSubtitleSettingsDialog(QWidget *parent)
    : PlayerOverlayDialog(parent)
{
    setSurfaceObjectName("playerSubtitleSettingsDialog");
    setSurfacePreferredSize(QSize(620, 456));
    setTitle(tr("Subtitle Settings"));

    contentLayout()->setContentsMargins(16, 8, 16, 16);
    contentLayout()->setSpacing(10);

    auto createInfoTile =
        [this](const QString &iconPath, const QString &title,
               const QString &description, QWidget *control,
               const QString &objectName) {
            auto *tile = new QFrame(this);
            tile->setObjectName(objectName);
            tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

            auto *layout = new QVBoxLayout(tile);
            layout->setContentsMargins(14, 12, 14, 12);
            layout->setSpacing(8);

            auto *headerRow = new QHBoxLayout();
            headerRow->setContentsMargins(0, 0, 0, 0);
            headerRow->setSpacing(10);

            headerRow->addWidget(createAdaptiveIconLabel(tile, iconPath), 0,
                                 Qt::AlignTop);

            auto *textContainer = new QWidget(tile);
            auto *textLayout = new QVBoxLayout(textContainer);
            textLayout->setContentsMargins(0, 0, 0, 0);
            textLayout->setSpacing(description.isEmpty() ? 0 : 3);

            auto *titleLabel = new QLabel(title, textContainer);
            titleLabel->setObjectName("playerSubtitleTileTitle");
            titleLabel->setWordWrap(true);
            textLayout->addWidget(titleLabel);

            if (!description.isEmpty()) {
                auto *descLabel = new QLabel(description, textContainer);
                descLabel->setObjectName("playerSubtitleTileDesc");
                descLabel->setWordWrap(true);
                textLayout->addWidget(descLabel);
            }

            headerRow->addWidget(textContainer, 1, Qt::AlignVCenter);
            layout->addLayout(headerRow);

            if (control) {
                control->setParent(tile);
                layout->addWidget(control);
            }

            return tile;
        };

    auto createSliderTile =
        [this, &createInfoTile](const QString &iconPath, const QString &title,
                                SubtitleOptionUtils::SliderKind kind,
                                const QString &configKey) {
            auto *control = new SubtitleOptionSlider(kind, configKey, this);
            control->setMinimumWidth(0);
            control->setMaximumWidth(QWIDGETSIZE_MAX);
            control->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            return createInfoTile(iconPath, title, QString(), control,
                                  QStringLiteral("playerSubtitleSliderTile"));
        };

    auto *summaryCard = new QFrame(this);
    summaryCard->setObjectName("playerSubtitleSummaryCard");
    auto *summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(14, 12, 14, 12);
    summaryLayout->setSpacing(0);

    auto *promptLabel = new QLabel(
        tr("Adjust subtitle rendering without leaving playback. Changes are saved and applied immediately."),
        summaryCard);
    promptLabel->setObjectName("playerSubtitleSummaryText");
    promptLabel->setWordWrap(true);
    summaryLayout->addWidget(promptLabel);
    contentLayout()->addWidget(summaryCard);

    auto *fontTile = new QFrame(this);
    fontTile->setObjectName("playerSubtitleInlineComboTile");
    fontTile->setToolTip(tr("Choose the font family used for subtitle rendering"));
    fontTile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *fontLayout = new QHBoxLayout(fontTile);
    fontLayout->setContentsMargins(14, 10, 14, 10);
    fontLayout->setSpacing(10);

    fontLayout->addWidget(createAdaptiveIconLabel(fontTile, ":/svg/dark/subtitle-lang.svg"),
                          0, Qt::AlignVCenter);

    auto *fontTitleLabel = new QLabel(tr("Subtitle Font"), fontTile);
    fontTitleLabel->setObjectName("playerSubtitleTileTitle");
    fontTitleLabel->setWordWrap(false);
    fontTitleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    fontLayout->addWidget(fontTitleLabel, 0, Qt::AlignVCenter);

    fontLayout->addStretch();

    auto *fontCombo = createSubtitleFontComboBox(fontTile);
    fontCombo->setToolTip(tr("Choose the font family used for subtitle rendering"));
    fontLayout->addWidget(fontCombo, 0, Qt::AlignVCenter);
    contentLayout()->addWidget(fontTile);

    auto *sliderGrid = new QGridLayout();
    sliderGrid->setContentsMargins(0, 0, 0, 0);
    sliderGrid->setHorizontalSpacing(10);
    sliderGrid->setVerticalSpacing(10);
    sliderGrid->setColumnStretch(0, 1);
    sliderGrid->setColumnStretch(1, 1);
    sliderGrid->setColumnStretch(2, 1);

    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/danmaku-offset.svg", tr("Subtitle Timing"),
                         SubtitleOptionUtils::SliderKind::DelayMs,
                         ConfigKeys::PlayerSubtitleDelayMs),
        0, 0);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/danmaku-size.svg", tr("Subtitle Size"),
                         SubtitleOptionUtils::SliderKind::FontSize,
                         ConfigKeys::PlayerSubtitleFontSize),
        0, 1);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/player.svg", tr("Subtitle Position"),
                         SubtitleOptionUtils::SliderKind::Position,
                         ConfigKeys::PlayerSubtitlePosition),
        0, 2);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/appearance-font-size.svg",
                         tr("Subtitle Scale"),
                         SubtitleOptionUtils::SliderKind::ScalePercent,
                         ConfigKeys::PlayerSubtitleScale),
        1, 0);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/window-player.svg", tr("Outline Size"),
                         SubtitleOptionUtils::SliderKind::OutlineSize,
                         ConfigKeys::PlayerSubtitleOutlineSize),
        1, 1);
    sliderGrid->addWidget(
        createSliderTile(":/svg/dark/direct-stream.svg", tr("Shadow Offset"),
                         SubtitleOptionUtils::SliderKind::ShadowOffset,
                         ConfigKeys::PlayerSubtitleShadowOffset),
        1, 2);
    contentLayout()->addLayout(sliderGrid);

    contentLayout()->addStretch();
}
