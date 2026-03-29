#ifndef MODERNTAGINPUT_H
#define MODERNTAGINPUT_H

#include <QFrame>
#include <QSize>
#include <QStringList>

class QHBoxLayout;
class QLineEdit;
class QMenu;
class QPushButton;
class QResizeEvent;
class QShowEvent;
class QToolButton;
class QWidget;


class ModernTagInput : public QFrame {
    Q_OBJECT
public:
    
    enum PopupMode {
        Auto,           
        ForceCompact,   
        ForcePopup      
    };

    explicit ModernTagInput(QWidget *parent = nullptr);

    
    void addPreset(const QString &label, const QString &value);
    void clearPresets();

    
    void setValue(const QString &commaSeparated);

    
    QString value() const;
    void setEditable(bool editable);

    
    void setMaxPopupHeight(int height);

    
    void setPopupMode(PopupMode mode);

    
    void setPopupThreshold(int threshold);

    bool isEditable() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    
    void valueChanged(const QString &newValue);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void rebuildTags();
    void scheduleRebuild(bool shouldEmitChange = false);
    void addCustomTag(const QString &text);
    void showPresetMenu();
    void showPresetCompactMenu();
    void showPresetPopup();
    void clearAll();
    void emitChange();
    void updateClearButtonVisibility();
    QString displayTextForValue(const QString &value) const;
    QString selectionToolTip() const;
    int availableChipWidth() const;
    QPushButton *createChipButton(const QString &text,
                                  const QString &objectName = "TagChipInline");

    struct PresetItem {
        QString label;
        QString value;
    };

    QHBoxLayout *m_mainLayout = nullptr;
    QHBoxLayout *m_tagArea;     
    QLineEdit   *m_lineEdit;    
    QWidget     *m_buttonSpacer = nullptr; 
    QToolButton *m_clearBtn;    
    QToolButton *m_moreBtn;     
    QList<PresetItem> m_presets;
    QStringList m_selectedValues;
    bool m_isEditable = true;
    bool m_isRebuilding = false;
    bool m_isRebuildQueued = false;
    bool m_emitChangeQueued = false;
    int m_maxPopupHeight = 320;
    int m_popupThreshold = 10;
    PopupMode m_popupMode = Auto;
};

#endif 
