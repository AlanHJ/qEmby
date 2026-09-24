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
#include <QPaintEvent>
#include <QPointer>
#include <QShowEvent>

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
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
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
    QPointer<QPushButton> m_selectedItem;
    int m_maxContentWidth; 
};

#endif 
