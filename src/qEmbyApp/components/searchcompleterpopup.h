#ifndef SEARCHCOMPLETERPOPUP_H
#define SEARCHCOMPLETERPOPUP_H

#include <QListView>
#include <QRect>
#include <QString>

class QAbstractItemModel;
class QShowEvent;
class QStyledItemDelegate;
class QWidget;

class SearchCompleterPopup : public QListView
{
    Q_OBJECT

public:
    explicit SearchCompleterPopup(QWidget *parent = nullptr);

    void setHighlightText(QString text);
    QString highlightText() const;
    void syncWidthToAnchor(const QWidget *anchor);
    void setMaxVisibleRows(int rows);
    QRect popupRectForAnchor(const QWidget *anchor,
                             int verticalOffset = 3) const;

protected:
    void setModel(QAbstractItemModel *model) override;
    void showEvent(QShowEvent *event) override;
    int sizeHintForRow(int row) const override;

private:
    int desiredPopupHeight() const;
    void ensureDelegate();

    QString m_highlightText;
    QStyledItemDelegate *m_itemDelegate = nullptr;
    int m_maxVisibleRows = 8;
};

#endif 
