#ifndef HWDECPRIORITYDIALOG_H
#define HWDECPRIORITYDIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

// 硬件解码配置弹窗: 硬解总开关 + 解码器优先级链 (自定义排序, 自动兜底)。
// 保存为逗号分隔的 mpv hwdec 回退链; 链尾恒有 auto-safe, mpv 全链失败后
// 自动落到 CPU 软解, 保证任何配置都不会无解码可用。
class HwdecPriorityDialog : public QDialog {
    Q_OBJECT
public:
    explicit HwdecPriorityDialog(QWidget *parent = nullptr);

    QString priorityChain() const;
    bool hwdecEnabled() const;

private:
    void addDecoder(const QString &id);
    void moveCurrent(int delta);
    void removeCurrent();
    void restoreDefaults();
    void updateSummary();
    static QStringList defaultChain();
    static QStringList availableDecoders();

    QCheckBox *m_enableCheck = nullptr;
    QListWidget *m_list = nullptr;
    QComboBox *m_addCombo = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_upButton = nullptr;
    QPushButton *m_downButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_defaultsButton = nullptr;
    QLabel *m_summaryLabel = nullptr;
};

#endif
