#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include "../managers/updatemanager.h"
#include "moderndialogbase.h"

class UpdateDialog : public ModernDialogBase
{
    Q_OBJECT

public:
    enum class Mode { Manual, Automatic };
    enum class Decision { Dismissed, RemindLater, IgnoreVersion, Update };

    explicit UpdateDialog(const UpdateInfo &info,
                          Mode mode,
                          QWidget *parent = nullptr);

    Decision decision() const;

private:
    Decision m_decision = Decision::Dismissed;
};

#endif 
