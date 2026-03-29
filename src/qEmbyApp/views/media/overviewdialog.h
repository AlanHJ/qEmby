#ifndef OVERVIEWDIALOG_H
#define OVERVIEWDIALOG_H

#include "../../components/moderndialogbase.h"
#include <models/media/mediaitem.h>
#include <QPixmap>

class QLabel;

class OverviewDialog : public ModernDialogBase {
    Q_OBJECT
public:
    explicit OverviewDialog(QWidget *parent = nullptr);
    void setMediaItem(const MediaItem& item, const QPixmap& posterPix);

private:
    QLabel* m_posterLabel;
    QLabel* m_titleLabel;
    QLabel* m_overviewLabel;
};

#endif 
