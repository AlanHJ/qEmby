#ifndef SETTINGSVIEW_H
#define SETTINGSVIEW_H

#include "../baseview.h"
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QPointer>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QVBoxLayout>

class SlidingStackedWidget;

class SettingsView : public BaseView {
    Q_OBJECT
public:
    explicit SettingsView(QEmbyCore* core, QWidget* parent = nullptr);
    ~SettingsView() override = default;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUi();
    void setupConnections();

    
    
    void ensurePageAt(int row);

    
    
    QScrollArea* wrapInScrollArea(QWidget* page, int row);

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

    
    QList<QPointer<QWidget>>   m_pages;
};

#endif 
