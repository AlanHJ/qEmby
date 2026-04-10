#ifndef MODERNMESSAGEBOX_H
#define MODERNMESSAGEBOX_H

#include "moderndialogbase.h"
#include <QString>

class ModernMessageBox : public ModernDialogBase {
    Q_OBJECT
public:
    enum ButtonType { Primary, Danger };
    enum IconType { NoIcon, Info, Question, Warning, Error }; 

    explicit ModernMessageBox(QWidget *parent = nullptr);

    
    static bool question(QWidget *parent,
                         const QString &title,
                         const QString &text,
                         const QString &yesText = "Yes",
                         const QString &noText = "Cancel",
                         ButtonType yesType = Primary,
                         IconType iconType = Question);

    static bool questionWithToggle(QWidget *parent,
                                   const QString &title,
                                   const QString &text,
                                   const QString &toggleText,
                                   bool *toggleChecked,
                                   const QString &yesText = "Yes",
                                   const QString &noText = "Cancel",
                                   ButtonType yesType = Primary,
                                   IconType iconType = Question);

    
    static void information(QWidget *parent,
                            const QString &title,
                            const QString &text,
                            const QString &okText = "OK");

    
    static void warning(QWidget *parent,
                        const QString &title,
                        const QString &text,
                        const QString &okText = "OK");

    
    static void critical(QWidget *parent,
                         const QString &title,
                         const QString &text,
                         const QString &okText = "OK");

private:
    void setupUi(const QString &title,
                 const QString &text,
                 const QString &yesText,
                 const QString &noText,
                 ButtonType yesType,
                 IconType iconType,
                 const QString &toggleText = QString(),
                 bool toggleChecked = false);
    bool m_result = false;
    bool m_toggleChecked = false;
};

#endif 
