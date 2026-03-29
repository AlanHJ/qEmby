#ifndef MODERNSCROLLPANEL_H
#define MODERNSCROLLPANEL_H

#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QPushButton>
#include <QVariant>
#include <QString>
#include <QList>
#include <QWheelEvent>

class ModernScrollPanel : public QFrame {
    Q_OBJECT
public:
    explicit ModernScrollPanel(QWidget *parent = nullptr);
    ~ModernScrollPanel() override = default;

    
    void addItem(const QString &text, const QVariant &userData, bool isSelected = false);

    
    void finalizeLayout(int maxHeight, int maxWidth = 250);

signals:
    void itemTriggered(const QVariant &userData, const QString &text);

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    
    struct MenuItem {
        QPushButton* btn;
        QString fullText;
    };

    QVBoxLayout *m_mainLayout;
    QScrollArea *m_scrollArea;
    QWidget *m_container;
    QVBoxLayout *m_layout;

    QList<MenuItem> m_items;
    int m_maxContentWidth; 
};

#endif 
