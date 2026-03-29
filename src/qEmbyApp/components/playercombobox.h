#ifndef PLAYERCOMBOBOX_H
#define PLAYERCOMBOBOX_H

#include "moderncombobox.h"
#include <QStyledItemDelegate>
#include <QFileIconProvider>


class PlayerItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit PlayerItemDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    
    int hitDeleteButton(const QStyleOptionViewItem &option,
                        const QPoint &pos) const;

signals:
    void deleteClicked(int index);

private:
    QRect deleteButtonRect(const QStyleOptionViewItem &option) const;
    mutable QFileIconProvider m_iconProvider;
};





class PlayerComboBox : public ModernComboBox {
    Q_OBJECT
public:
    
    static constexpr const char *SourceAuto   = "auto";
    static constexpr const char *SourceManual = "manual";
    static constexpr const char *SourceCustom = "custom";

    static constexpr int SourceRole = Qt::UserRole + 1;

    explicit PlayerComboBox(QWidget *parent = nullptr);

    
    void addPlayer(const QString &displayName, const QString &path,
                   const QString &source);

    
    void removePlayer(int index);

    
    void removeAutoDetected();

    
    QString currentPlayerPath() const;

signals:
    void playerRemoved(int index, const QString &path);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    PlayerItemDelegate *m_delegate;
    QFileIconProvider m_iconProvider;
};

#endif 
