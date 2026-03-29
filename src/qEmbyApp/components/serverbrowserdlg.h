#ifndef SERVERBROWSERDLG_H
#define SERVERBROWSERDLG_H

#include "moderndialogbase.h"
#include <qcorotask.h>

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QEmbyCore;

class ServerBrowserDialog : public ModernDialogBase {
    Q_OBJECT
public:
    explicit ServerBrowserDialog(QEmbyCore* core, QWidget* parent = nullptr);

    QString selectedPath() const;

private:
    QCoro::Task<void> loadDrives();
    void onItemExpanded(QTreeWidgetItem* item);
    QCoro::Task<void> loadChildren(QTreeWidgetItem* parentItem, const QString& path);
    QString iconPrefix() const;

    QEmbyCore* m_core;
    QTreeWidget* m_tree;
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;
    QString m_selectedPath;

    
    QCoro::Task<void> m_currentTask;
};

#endif 
