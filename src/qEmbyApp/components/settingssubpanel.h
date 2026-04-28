#ifndef SETTINGSSUBPANEL_H
#define SETTINGSSUBPANEL_H

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>


class SettingsSubPanel : public QFrame {
    Q_OBJECT
    Q_PROPERTY(int panelHeight READ panelHeight WRITE setPanelHeight)

public:
    
    explicit SettingsSubPanel(const QString &iconPath, QWidget *parent = nullptr);

    
    QHBoxLayout *contentLayout() const { return m_contentLayout; }

    
    bool isExpanded() const { return m_expanded; }

public slots:
    void expand();
    void collapse();
    void setExpanded(bool expanded);
    void setExpandedImmediately(bool expanded);
    
    void initExpanded();

signals:
    void expandedChanged(bool expanded);

private:
    int panelHeight() const;
    void setPanelHeight(int h);
    int contentHeight() const;
    void updateStyleSheet();
    void updateIcon();

    bool m_expanded = false;
    QString m_iconPath;
    QLabel *m_iconLabel;
    QHBoxLayout *m_outerLayout;   
    QHBoxLayout *m_contentLayout; 
    QPropertyAnimation *m_animation;
};

#endif 
