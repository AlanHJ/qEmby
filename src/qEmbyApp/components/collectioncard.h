#ifndef COLLECTIONCARD_H
#define COLLECTIONCARD_H

#include <QFrame>
#include <QPoint>
#include <QPixmap>
#include <QPushButton>

class CollectionGrid;
class QMouseEvent;
class QResizeEvent;




class CollectionCard : public QFrame {
    Q_OBJECT

public:
    explicit CollectionCard(const QString& itemId,
                            const QString& name,
                            int childCount,
                            const QString& type,
                            QWidget* parent = nullptr);

    QString itemId() const { return m_itemId; }
    QString itemName() const { return m_name; }
    bool hasImage() const { return !m_image.isNull(); }
    bool isSelected() const { return m_selected; }
    bool isDragging() const { return m_dragging; }
    void setImage(const QPixmap& pixmap);
    void setDisplayData(const QString& name, int childCount, const QString& type);
    void setSelected(bool selected);
    void setDragging(bool dragging);

signals:
    void renameClicked(const QString& id, const QString& name);
    void deleteClicked(const QString& id, const QString& name);
    void selectionChanged(const QString& id, bool selected);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateButtonLayout();
    void toggleSelected();

    QString m_itemId;
    QString m_name;
    int m_childCount;
    QString m_type;        
    QPixmap m_image;
    bool m_selected = false;
    bool m_dragging = false;
    bool m_pressCandidate = false;
    QPoint m_dragStartPos;

    QPushButton* m_btnRename;
    QPushButton* m_btnDelete;
};

#endif 
