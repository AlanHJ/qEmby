#ifndef PLAYERSEEKINDICATOR_H
#define PLAYERSEEKINDICATOR_H

#include <QWidget>
#include <QPoint>
#include <QImage>

class QLabel;


class PlayerSeekIndicator : public QWidget
{
    Q_OBJECT
public:
    explicit PlayerSeekIndicator(QWidget *parent = nullptr);
    void setTimeText(const QString &text);
    void setPreviewImage(const QImage &image);
    
    void setAnchor(const QPoint &anchor);

private:
    void updateIndicatorLayout();
    QLabel *m_timeLabel = nullptr;
    QLabel *m_imageLabel = nullptr;
    QImage m_previewImage;
    QSize m_previewSize;
    QWidget *m_stem = nullptr;
    QWidget *m_marker = nullptr;
    QPoint m_anchor;
};

#endif
