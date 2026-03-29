#ifndef COLLECTIONGRID_H
#define COLLECTIONGRID_H

#include <QList>
#include <QPoint>
#include <QStringList>
#include <QWidget>

class CollectionCard;
class QResizeEvent;
class QScrollArea;
class QTimer;




class CollectionGrid : public QWidget {
    Q_OBJECT
public:
    explicit CollectionGrid(QWidget* parent = nullptr);

    void setCards(const QList<CollectionCard*>& cards);
    QList<CollectionCard*> cards() const { return m_cards; }
    void relayout(bool animate = false);

    
    static constexpr int CardWidth = 170;
    static constexpr int CardHeight = 246;
    static constexpr int CardSpacing = 12;
    static constexpr int LeftPadding = 4;
    static constexpr int RightPadding = 20;

signals:
    void orderChanged(const QStringList& newOrderIds);

public slots:
    void onCardDragStart(CollectionCard* card, const QPoint& globalPos);
    void onCardDragMove(CollectionCard* card, const QPoint& globalPos);
    void onCardDragEnd(CollectionCard* card);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    int cardHeightForWidth(int width) const;
    int columnsForWidth(int width) const;
    int contentWidth() const;
    int cardWidthForColumn(int column, int columns) const;
    int columnX(int column, int columns) const;
    int columnForX(int x, int columns) const;
    QPoint posForIndex(int index, int columns) const;
    int indexForPos(const QPoint& pos, int columns) const;
    QScrollArea* parentScrollArea() const;
    int autoScrollDelta(const QPoint& globalPos) const;
    void updateDragPosition(CollectionCard* card, const QPoint& globalPos);

    QList<CollectionCard*> m_cards;
    CollectionCard* m_dragCard = nullptr;
    int m_dragOriginalIndex = -1;
    int m_dragCurrentIndex = -1;
    int m_columns = 3;
    int m_rowHeight = CardHeight;
    QPoint m_lastDragGlobalPos;
    QTimer* m_autoScrollTimer = nullptr;
};

#endif 
