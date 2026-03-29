#ifndef MANAGEVIEW_H
#define MANAGEVIEW_H

#include "../baseview.h"
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QVBoxLayout>

class SlidingStackedWidget;

class ManageView : public BaseView {
    Q_OBJECT
public:
    explicit ManageView(QEmbyCore* core, QWidget* parent = nullptr);
    ~ManageView() override = default;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUi();
    void setupConnections();

private slots:
    void onThemeChanged();

private:
    QWidget* m_leftPanel;
    QLabel* m_titleLabel;
    QListWidget* m_navMenu;
    SlidingStackedWidget* m_stack;

    
    QList<QScrollArea*>        m_scrollAreas;
    QList<QPropertyAnimation*> m_scrollAnims;
    QList<int>                 m_scrollTargets;
};

#endif 
