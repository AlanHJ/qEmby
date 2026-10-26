#ifndef MEDIACAROUSELWIDGET_H
#define MEDIACAROUSELWIDGET_H

#include <QFrame>
#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QPointer>
#include <QSet>
#include <models/media/mediaitem.h>
#include <qcorotask.h>

class ElidedLabel;
class QLabel;
class QTimer;
class TextOutlineEffect;
class CarouselNavigationWidget;
class MediaService;
class GlassBackdropWidget;
class DetailContentWidget;
class PageTurnOverlayWidget;


class MediaCarouselWidget : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(QColor textOutlineColor READ textOutlineColor WRITE setTextOutlineColor)
    Q_PROPERTY(qreal textOutlineWidth READ textOutlineWidth WRITE setTextOutlineWidth)
    Q_PROPERTY(QColor textShadowColor READ textShadowColor WRITE setTextShadowColor)
    Q_PROPERTY(qreal textShadowRadius READ textShadowRadius WRITE setTextShadowRadius)
    Q_PROPERTY(qreal textShadowOffset READ textShadowOffset WRITE setTextShadowOffset)
public:
    explicit MediaCarouselWidget(MediaService* service, QWidget* parent = nullptr);
    ~MediaCarouselWidget() override;
    void setItems(const QList<MediaItem>& items);
    void setSelectionLoading(bool loading);
    void updateItem(const MediaItem& item);
    void updateItemMetadata(const MediaItem& item);
    void removeItem(const QString& itemId);
    void setBackdropSource(DetailContentWidget* source);
    
    void setPreferredHeight(int height);
    QColor textOutlineColor() const { return m_textOutlineColor; }
    void setTextOutlineColor(const QColor& color);
    qreal textOutlineWidth() const { return m_textOutlineWidth; }
    void setTextOutlineWidth(qreal width);
    QColor textShadowColor() const { return m_textShadowColor; }
    void setTextShadowColor(const QColor& color);
    qreal textShadowRadius() const { return m_textShadowRadius; }
    void setTextShadowRadius(qreal radius);
    qreal textShadowOffset() const { return m_textShadowOffset; }
    void setTextShadowOffset(qreal offset);
    QSize minimumSizeHint() const override {
        return QSize(1, QFrame::minimumSizeHint().height());
    }

Q_SIGNALS:
    void itemClicked(const MediaItem& item);
    void backdropChanged(const QPixmap& image, int direction);
    void changeSelectionRequested();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool event(QEvent* event) override;

private:
    void selectSlide(int index);
    void updateText(const MediaItem& item);
    void updateTimer();
    void showImage(const QPixmap& image);
    void updateHeight();
    void updateLogo();
    void updateNavigationGeometry();
    QPixmap captureForeground(const QSize& sceneSize);
    void updateForegroundFrame();
    void requestArtwork(const MediaItem& item, bool foreground);
    void storeArtwork(QString itemId, QString imageType, QPixmap image);
    QCoro::Task<void> loadImage(MediaItem item, int generation, bool foreground,
                               QString imageType);

    QPointer<MediaService> m_service;
    QList<MediaItem> m_items;
    QHash<QString, QPixmap> m_artwork;
    QSet<QString> m_pendingArtwork;
    int m_index = -1;
    int m_presentedIndex = -1;
    int m_generation = 0;
    int m_preferredHeight = 0;
    bool m_hovered = false;
    bool m_capturingForeground = false;
    QPixmap m_image;
    QPixmap m_logoPixmap;
    int m_direction = 1;
    ElidedLabel* m_title = nullptr;
    ElidedLabel* m_metadata = nullptr;
    QLabel* m_ratingStar = nullptr;
    ElidedLabel* m_tags = nullptr;
    ElidedLabel* m_overview = nullptr;
    QLabel* m_logo = nullptr;
    QWidget* m_logoLayer = nullptr;
    CarouselNavigationWidget* m_navigation = nullptr;
    QWidget* m_content = nullptr;
    QWidget* m_textContent = nullptr;
    QPointer<DetailContentWidget> m_backdropSource;
    QPointer<GlassBackdropWidget> m_glassBackdrop;
    QPointer<PageTurnOverlayWidget> m_pageOverlay;
    QTimer* m_timer = nullptr;
    QColor m_textOutlineColor;
    qreal m_textOutlineWidth = 0.8;
    QList<TextOutlineEffect*> m_textOutlines;
    QColor m_textShadowColor;
    qreal m_textShadowRadius = 4.0;
    qreal m_textShadowOffset = 1.5;
    QList<TextOutlineEffect*> m_textShadows;
};

#endif 
