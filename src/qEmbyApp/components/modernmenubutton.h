#ifndef MODERNMENUBUTTON_H
#define MODERNMENUBUTTON_H

#include <QPushButton>
#include <QMenu>
#include <QVariant>
#include <QList>

class ModernMenuButton : public QPushButton {
    Q_OBJECT
public:
    explicit ModernMenuButton(QWidget *parent = nullptr);

    void clear();
    void addItem(const QString& title, const QString& subtitle,
                 const QString& secondLine = QString(),
                 const QVariant& userData = QVariant());
    void setCurrentIndex(int index);

    int currentIndex() const { return m_currentIndex; }
    QVariant currentData() const;
    int count() const { return m_actions.size(); }

Q_SIGNALS:
    void currentIndexChanged(int index);

private Q_SLOTS:
    void onActionTriggered(QAction *action);

private:
    void updateAppearance();

    QMenu* m_menu;
    QList<QAction*> m_actions;
    int m_currentIndex;
};

#endif 
