#ifndef EMBYCODECPARAMETERSDIALOG_H
#define EMBYCODECPARAMETERSDIALOG_H

#include "moderndialogbase.h"
#include <QJsonObject>
#include <optional>
#include <qcorotask.h>

class QEmbyCore;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QWidget;
class ModernComboBox;
class ModernSwitch;

class EmbyCodecParametersDialog : public ModernDialogBase {
    Q_OBJECT
public:
    explicit EmbyCodecParametersDialog(QEmbyCore* core,
                                       QJsonObject codecInfo,
                                       QWidget* parent = nullptr);

private:
    struct FieldBinding {
        QString id;
        QString editorType;
        QString displayName;
        QString description;
        QString staticItemsSourceId;
        bool allowEmpty = true;
        bool readOnly = false;
        bool advanced = false;
        int decimals = 0;
        int minimumValue = 0;
        int maximumValue = 0;
        bool hasMinimum = false;
        bool hasMaximum = false;
        QWidget* container = nullptr;
        QLineEdit* lineEdit = nullptr;
        ModernComboBox* comboBox = nullptr;
        ModernSwitch* switchControl = nullptr;
    };

    void setupUi();
    void rebuildForm();
    void clearFieldBindings();
    void refreshUiState();
    QString codecTitle() const;
    QString codecSummary() const;
    void appendEditorItems(const QJsonObject& item, QList<QJsonObject>& items) const;
    void setWidgetValue(const FieldBinding& field, const QJsonValue& value);
    QJsonObject collectParameters() const;
    void resetToDefaults();

    QCoro::Task<void> loadParameters();
    QCoro::Task<void> saveParameters();

    QEmbyCore* m_core = nullptr;
    QJsonObject m_codecInfo;
    QString m_codecId;

    QLabel* m_summaryLabel = nullptr;
    QLabel* m_feedbackLabel = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_formPage = nullptr;
    QVBoxLayout* m_formLayout = nullptr;
    QWidget* m_basicCard = nullptr;
    QWidget* m_advancedCard = nullptr;
    QVBoxLayout* m_basicBody = nullptr;
    QVBoxLayout* m_advancedBody = nullptr;
    QPushButton* m_btnCancel = nullptr;
    QPushButton* m_btnReset = nullptr;
    QPushButton* m_btnSave = nullptr;

    QList<FieldBinding> m_fields;
    QJsonObject m_editorContainer;
    QJsonObject m_parameterObject;
    QJsonObject m_defaultObject;
    QString m_feedbackText;
    bool m_isLoading = false;
    bool m_isSaving = false;
    bool m_hasLoaded = false;
    std::optional<QCoro::Task<void>> m_pendingTask;
};

#endif 
