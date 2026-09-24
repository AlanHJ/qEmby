#ifndef MODERNDIALOGBASE_H
#define MODERNDIALOGBASE_H

#include <QDialog>
#include <QString>

class QVBoxLayout;
class QLabel;
class QWidget;
class QShowEvent;

class ModernDialogBase : public QDialog {
    Q_OBJECT
public:
    explicit ModernDialogBase(QWidget *parent = nullptr,
                              bool disableNativeTransitions = false);
    void setTitle(const QString &title);

public slots:
    int exec() override;

protected:
    void showEvent(QShowEvent *event) override;
    
    QVBoxLayout* contentLayout() const { return m_contentLayout; }

private:
    QLabel *m_titleLabel;
    QVBoxLayout *m_contentLayout;
    QWidget *m_titleBarWidget;
};

#endif 
