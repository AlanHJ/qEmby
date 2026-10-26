#ifndef IMAGEPREVIEWDIALOG_H
#define IMAGEPREVIEWDIALOG_H

#include "moderndialogbase.h"
#include <QPixmap>

class QLabel;
class QPushButton;
class QScrollArea;

class ImagePreviewDialog : public ModernDialogBase
{
    Q_OBJECT
public:
    explicit ImagePreviewDialog(QString imageName, QWidget* parent = nullptr);
    void setImage(QPixmap image);
    void setMessage(const QString& message);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateImage();

    QScrollArea* m_scrollArea = nullptr;
    QLabel* m_imageLabel = nullptr;
    QPushButton* m_sizeButton = nullptr;
    QPixmap m_image;
    bool m_fitToWindow = true;
};

#endif 
