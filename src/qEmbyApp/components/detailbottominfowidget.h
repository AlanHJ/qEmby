#ifndef DETAILBOTTOMINFOWIDGET_H
#define DETAILBOTTOMINFOWIDGET_H

#include <QWidget>
#include "models/media/mediaitem.h"

class FlowLayout;
class HorizontalWidgetGallery;
class QLabel;
class QVBoxLayout;
class QGridLayout;

class DetailBottomInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit DetailBottomInfoWidget(QWidget* parent = nullptr);

    void setInfo(const MediaItem& item, const QList<MediaSourceInfo>& sources);
    void clear();

signals:
    void filterClicked(const QString& filterType, const QString& filterValue);

private:
    void clearLayout(QLayout* layout);
    QString formatSize(long long bytes);
    void addInfoRow(QGridLayout* layout, int& row, const QString& key, const QString& value);
    QWidget* wrapMaxWidth(QWidget* child, int maxW);

    QLabel* m_tagsBottomTitle;
    QWidget* m_tagsBottomWidget;
    QWidget* m_tagsBottomWrapper;
    FlowLayout* m_tagsBottomLayout;

    QLabel* m_studiosTitle;
    QWidget* m_studiosWidget;
    QWidget* m_studiosWrapper;
    FlowLayout* m_studiosLayout;

    
    QLabel* m_externalLinksTitle;
    QWidget* m_externalLinksWidget;
    QWidget* m_externalLinksWrapper;
    FlowLayout* m_externalLinksLayout;

    QLabel* m_mediaInfoTitle;
    QWidget* m_fileInfoWidget;
    QLabel* m_filePathLabel;
    QLabel* m_fileMetaLabel;
    HorizontalWidgetGallery* m_mediaInfoGallery;
};

#endif 
