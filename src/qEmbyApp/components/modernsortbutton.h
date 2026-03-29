#ifndef MODERNSORTBUTTON_H
#define MODERNSORTBUTTON_H

#include <QPushButton>
#include <QMenu>
#include <QStringList>
#include <QList>


struct SortOption {
    QString name;
    QString key;
};

class ModernSortButton : public QPushButton {
    Q_OBJECT
public:
    explicit ModernSortButton(QWidget *parent = nullptr);

    
    void setSortItems(const QStringList& items);

    
    void addSortOption(const QString& name, const QString& key);

    
    QString currentSortKey() const;
    bool isAscending() const;

    int currentIndex() const { return m_currentIndex; }
    bool isDescending() const { return m_descending; }

    
    void setCurrentIndex(int index);
    void setDescending(bool descending);

Q_SIGNALS:
    
    void sortChanged(const QString& sortKey, bool ascending);

private Q_SLOTS:
    void onActionTriggered(QAction *action);

private:
    void updateAppearance();

    QMenu* m_menu;
    QList<SortOption> m_options; 
    int m_currentIndex;
    bool m_descending;
};

#endif 
