#ifndef PAGETRANSCODING_H
#define PAGETRANSCODING_H

#include "managepagebase.h"
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QVariant>
#include <optional>
#include <qcorotask.h>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QShowEvent;
class QSpinBox;
class QVBoxLayout;
class QWidget;
class ModernComboBox;
class ModernSwitch;
class ModernTagInput;
class TranscodingCodecRowWidget;

class PageTranscoding : public ManagePageBase {
    Q_OBJECT
public:
    explicit PageTranscoding(QEmbyCore* core, QWidget* parent = nullptr);
    QScrollArea* scrollArea() const { return m_scrollArea; }

protected:
    void showEvent(QShowEvent* event) override;

private:
    struct EmbyCodecState {
        QString codecId;
        QString direction;
        QString mediaType;
        QString title;
        QString description;
        int priority = 0;
        bool enabled = false;
        TranscodingCodecRowWidget* row = nullptr;
    };

    void setupToolbar();
    void setupContentArea();
    void buildPageLayout();
    void buildJellyfinLayout(QVBoxLayout* contentLayout);
    void buildEmbyLayout(QVBoxLayout* contentLayout);
    void connectInputs();

    QCoro::Task<void> loadData(bool isManual = false);
    QCoro::Task<void> saveData();

    QWidget* createSectionCard(const QString& title,
                               const QString& subtitle,
                               QVBoxLayout*& bodyLayout,
                               QWidget* parent);
    QWidget* createFieldBlock(const QString& title,
                              const QString& description,
                              QWidget* control,
                              QWidget* parent,
                              bool compactControl = false);
    QWidget* createSwitchBlock(const QString& title,
                               const QString& description,
                               ModernSwitch* control,
                               QWidget* parent);
    QWidget* createGroupBlock(const QString& title,
                              const QString& description,
                              QVBoxLayout*& bodyLayout,
                              QWidget* parent);
    QLineEdit* createTextInput(int width = 240, bool readOnly = false);
    QSpinBox* createSpinBox(int minimum,
                            int maximum,
                            int singleStep = 1,
                            int width = 168);
    QDoubleSpinBox* createDoubleSpinBox(double minimum,
                                        double maximum,
                                        double singleStep,
                                        int decimals = 1,
                                        int width = 168);
    ModernComboBox* createComboBox(int width = 220);
    ModernTagInput* createTagInput(const QStringList& presetValues = {});
    void addComboOption(ModernComboBox* combo, const QString& text, const QVariant& value);
    void setComboCurrentValue(ModernComboBox* combo, const QVariant& value);
    QVariant comboCurrentValue(ModernComboBox* combo) const;
    QStringList stringListFromJsonValue(const QJsonValue& value) const;
    QJsonArray jsonArrayFromStringList(const QStringList& values) const;

    void addBlock(QVBoxLayout* body,
                  QWidget* sectionCard,
                  const QString& key,
                  QWidget* block,
                  bool actualKey = true);
    void setBlockVisible(const QString& key, bool visible);
    void updateSectionVisibility();
    void updateDynamicVisibility();
    void refreshDerivedUi();
    void updateStatusText(const QJsonObject& config);
    void updateContentState(bool hasConfig);

    void populateForm(const QJsonObject& config);
    QJsonObject collectConfig() const;

    void populateJellyfinCodecSwitches(const QJsonObject& config);
    QStringList collectJellyfinCodecValues() const;
    QStringList knownJellyfinDecoderCodecs() const;
    QString jellyfinCodecLabel(const QString& codec) const;
    bool jellyfinCodecSupportedByHardware(const QString& codec,
                                          const QString& hardwareType) const;

    void populateEmbyCodecLists(const QJsonObject& config);
    QJsonObject embyCodecInfoById(const QString& codecId) const;
    QStringList sortedEmbyCodecIds(const QString& direction,
                                   const QString& mediaType,
                                   const QJsonArray& configs) const;
    QString embyGroupKey(const QString& direction, const QString& mediaType) const;
    void showEmbyCodecInfo(const QString& codecId);
    void showEmbyCodecParameters(const QString& codecId);
    QJsonArray collectEmbyCodecConfigurations() const;

    QString serverTypeText() const;
    QString hardwareTypeText(QString type) const;
    QString embyHardwareModeText(int mode) const;
    QString accelerationSummary(const QJsonObject& config) const;
    QString decoderSummary(const QJsonObject& config) const;
    QString toneMappingSummary(const QJsonObject& config) const;
    bool isToneMappingEnabled(const QJsonObject& config) const;
    bool isEmbyAdvancedMode(const QJsonObject& config) const;
    void playRefreshAnimation();

    QPushButton* m_btnRefresh = nullptr;
    QPushButton* m_btnSave = nullptr;
    QLabel* m_statsLabel = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollContentWidget = nullptr;
    QWidget* m_contentContainer = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    QWidget* m_emptyStateContainer = nullptr;
    QLabel* m_emptyLabel = nullptr;

    QList<QWidget*> m_sectionCards;
    QHash<QWidget*, QStringList> m_sectionBlocks;
    QHash<QString, QWidget*> m_blocks;
    QSet<QString> m_actualBlockKeys;

    QHash<QString, ModernSwitch*> m_switches;
    QHash<QString, QSpinBox*> m_spinBoxes;
    QHash<QString, QDoubleSpinBox*> m_doubleSpinBoxes;
    QHash<QString, QLineEdit*> m_lineEdits;
    QHash<QString, ModernComboBox*> m_comboBoxes;
    QHash<QString, ModernTagInput*> m_tagInputs;

    QLineEdit* m_encoderAppPathDisplay = nullptr;
    QHash<QString, ModernSwitch*> m_jellyfinCodecSwitches;
    QHash<QString, QWidget*> m_jellyfinCodecBlocks;
    QStringList m_unknownJellyfinCodecValues;

    QVBoxLayout* m_embyDecoderLayout = nullptr;
    QVBoxLayout* m_embyEncoderLayout = nullptr;
    QVBoxLayout* m_embySoftwareLayout = nullptr;
    QHash<QString, EmbyCodecState> m_embyCodecStates;
    QHash<QString, QStringList> m_embyCodecOrders;
    QStringList m_embyCodecGroupOrder;

    QJsonObject m_rawConfig;
    QJsonArray m_embyCodecInfo;
    QJsonArray m_embyDefaultCodecConfigs;
    QSet<QString> m_supportedKeys;
    bool m_isJellyfin = false;
    bool m_loaded = false;
    bool m_hasLoadedData = false;
    bool m_isLoading = false;
    bool m_isSaving = false;
    std::optional<QCoro::Task<void>> m_pendingTask;
};

#endif 
