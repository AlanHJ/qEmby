#ifndef LIBRARYGRID_H
#define LIBRARYGRID_H

#include <QWidget>
#include <QStringList>

class LibraryCard;
class QScrollArea;
class QTimer;




class LibraryGrid : public QWidget {
    Q_OBJECT
public:
    explicit LibraryGrid(QWidget* parent = nullptr);

    void setCards(const QList<LibraryCard*>& cards);
    QList<LibraryCard*> cards() const { return m_cards; }
    void relayout(bool animate = false);

    
    static constexpr int CardWidth = 170;
    static constexpr int CardHeight = 200;
    static constexpr int CardSpacing = 12;
    static constexpr int LeftPadding = 4;
    static constexpr int RightPadding = 20;

signals:
    void orderChanged(const QStringList& newOrderIds);

public slots:
    void onCardDragStart(LibraryCard* card, const QPoint& globalPos);
    void onCardDragMove(LibraryCard* card, const QPoint& globalPos);
    void onCardDragEnd(LibraryCard* card);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    int columnsForWidth(int width) const;
    int contentWidth() const;
    int cardWidthForColumn(int column, int columns) const;
    int columnX(int column, int columns) const;
    int columnForX(int x, int columns) const;
    QPoint posForIndex(int index, int columns) const;
    int indexForPos(const QPoint& pos, int columns) const;
    QScrollArea* parentScrollArea() const;
    int autoScrollDelta(const QPoint& globalPos) const;
    void updateDragPosition(LibraryCard* card, const QPoint& globalPos);

    QList<LibraryCard*> m_cards;
    LibraryCard* m_dragCard = nullptr;
    int m_dragOriginalIndex = -1;
    int m_dragCurrentIndex = -1;
    int m_columns = 3;
    QPoint m_lastDragGlobalPos;
    QTimer* m_autoScrollTimer = nullptr;
};

#endif 
