#ifndef DETAILBOTTOMINFOWIDGET_H
#define DETAILBOTTOMINFOWIDGET_H

#include <QWidget>
#include "models/media/mediaitem.h"

class FlowLayout;
class HorizontalWidgetGallery;
class QLabel;
class QVBoxLayout;
class QGridLayout;
class QEvent;
class QResizeEvent;
class QShowEvent;

class DetailBottomInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit DetailBottomInfoWidget(QWidget* parent = nullptr);

    void setInfo(const MediaItem& item, const QList<MediaSourceInfo>& sources);
    void clear();
    
    void setLeadingContentInset(int inset);

signals:
    void filterClicked(const QString& filterType, const QString& filterValue);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void clearLayout(QLayout* layout);
    QString formatSize(long long bytes);
    void addInfoRow(QGridLayout* layout, int& row, const QString& key, const QString& value);
    QWidget* wrapMaxWidth(QWidget* child, int maxW);
    QWidget* createSection(QLabel* title, const QList<QWidget*>& contents);
    int resolveFlowLayoutWidth(QWidget* widget) const;
    void updateFlowLayoutHeight(QWidget* widget, FlowLayout* layout);
    void updateFlowLayoutHeights();
    void scheduleFlowLayoutHeightUpdate();
    void updateSectionSpacing();

    QWidget* m_tagsSection;
    QWidget* m_tagsBottomWidget;
    QWidget* m_tagsBottomWrapper;
    FlowLayout* m_tagsBottomLayout;

    QWidget* m_studiosSection;
    QWidget* m_studiosWidget;
    QWidget* m_studiosWrapper;
    FlowLayout* m_studiosLayout;

    
    QWidget* m_externalLinksSection;
    QWidget* m_externalLinksWidget;
    QWidget* m_externalLinksWrapper;
    FlowLayout* m_externalLinksLayout;

    QWidget* m_mediaInfoSection;
    QWidget* m_fileInfoWidget;
    QLabel* m_filePathLabel;
    QLabel* m_fileMetaLabel;
    HorizontalWidgetGallery* m_mediaInfoGallery;

    bool m_flowLayoutHeightUpdatePending = false;
    int m_leadingContentInset = 0;
};

#endif 
