#ifndef MEDIACARDDELEGATE_H
#define MEDIACARDDELEGATE_H

#include <QStyledItemDelegate>
#include <QWidget>
#include <QColor>
#include <QFlags>
#include <QSet>


struct MediaItem;


class MediaCardThemeHelper : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor hoverBg READ hoverBg WRITE setHoverBg)
    Q_PROPERTY(QColor selectedBg READ selectedBg WRITE setSelectedBg)
    Q_PROPERTY(QColor titleColor READ titleColor WRITE setTitleColor)
    Q_PROPERTY(QColor subTitleColor READ subTitleColor WRITE setSubTitleColor)
    Q_PROPERTY(QColor placeholderBg READ placeholderBg WRITE setPlaceholderBg)
    Q_PROPERTY(QColor placeholderTextColor READ placeholderTextColor WRITE setPlaceholderTextColor)

public:
    explicit MediaCardThemeHelper(QWidget *parent = nullptr);

    QColor hoverBg() const { return m_hoverBg; }
    void setHoverBg(const QColor &c) { m_hoverBg = c; }

    QColor selectedBg() const { return m_selectedBg; }
    void setSelectedBg(const QColor &c) { m_selectedBg = c; }

    QColor titleColor() const { return m_titleColor; }
    void setTitleColor(const QColor &c) { m_titleColor = c; }

    QColor subTitleColor() const { return m_subTitleColor; }
    void setSubTitleColor(const QColor &c) { m_subTitleColor = c; }

    QColor placeholderBg() const { return m_placeholderBg; }
    void setPlaceholderBg(const QColor &c) { m_placeholderBg = c; }

    QColor placeholderTextColor() const { return m_placeholderTextColor; }
    void setPlaceholderTextColor(const QColor &c) { m_placeholderTextColor = c; }

private:
    QColor m_hoverBg;
    QColor m_selectedBg;
    QColor m_titleColor;
    QColor m_subTitleColor;
    QColor m_placeholderBg;
    QColor m_placeholderTextColor;
};

class MediaCardDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    enum CardStyle {
        Poster,      
        LibraryTile, 
        Cast,        
        EpisodeList  
    };

    enum HoverControl {
        HoverControlNone = 0x0,
        HoverControlPlay = 0x1,
        HoverControlFavorite = 0x2,
        HoverControlMore = 0x4,
        HoverControlAll =
            HoverControlPlay | HoverControlFavorite | HoverControlMore
    };
    Q_DECLARE_FLAGS(HoverControls, HoverControl)

    explicit MediaCardDelegate(CardStyle style = Poster, QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void setTileSize(const QSize &size) { m_tileSize = size; }
    void setStyle(CardStyle style) { m_style = style; }
    void setHighlightedItemId(const QString &id) { m_highlightedItemId = id; }
    void setTextPixelSizes(int titlePx, int subTitlePx)
    {
        m_titleFontPixelSize = titlePx;
        m_subTitleFontPixelSize = subTitlePx;
    }
    void setContentPadding(int padding)
    {
        m_contentPadding = padding < 0 ? 0 : padding;
    }
    int contentPadding() const { return m_contentPadding; }
    void setShowMoreButtonForNonPlayableTiles(bool enabled)
    {
        m_showMoreButtonForNonPlayableTiles = enabled;
    }
    void setHoverControls(HoverControls controls)
    {
        m_hoverControls = controls;
    }
    HoverControls hoverControls() const { return m_hoverControls; }

signals:
    
    void playRequested(const MediaItem &item);
    void moreMenuRequested(const MediaItem &item, const QPoint &globalPos);
    void favoriteRequested(const MediaItem &item); 

protected:
    
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    CardStyle m_style;
    QSize m_tileSize;
    QString m_highlightedItemId; 
    mutable MediaCardThemeHelper *m_themeHelper; 
    mutable QSet<QWidget*> m_installedViewports; 
    bool m_showMoreButtonForNonPlayableTiles = false;
    HoverControls m_hoverControls = HoverControlAll;
    int m_titleFontPixelSize = 13;
    int m_subTitleFontPixelSize = 12;
    int m_contentPadding = 8;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MediaCardDelegate::HoverControls)

#endif 
