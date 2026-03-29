#ifndef MODERNTOAST_H
#define MODERNTOAST_H

#include <QWidget>
#include <QTimer>
#include <QPointer>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QParallelAnimationGroup>

class ModernToast : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal textScale READ textScale WRITE setTextScale)

public:
    
    static void showMessage(const QString &msg, int durationMs = 2500);

protected:
    explicit ModernToast(QWidget *parent = nullptr);
    ~ModernToast() override;

    void paintEvent(QPaintEvent *event) override;

private:
    
    void showWithAnimation(const QString &msg, int durationMs);
    static QWidget* getMainWindow();

    qreal textScale() const { return m_textScale; }
    void setTextScale(qreal scale);

    static QPointer<ModernToast> s_instance;

    QString m_message;
    qreal m_textScale;

    int m_comboCount;
    bool m_isCrtFadingOut; 

    QTimer *m_stayTimer;

    
    QParallelAnimationGroup *m_showGroup;
    QPropertyAnimation *m_showOpacity;
    QPropertyAnimation *m_showSlide;

    
    QSequentialAnimationGroup *m_popSequentialGroup;
    QParallelAnimationGroup *m_expandGroup;
    QParallelAnimationGroup *m_shrinkGroup;
    QPropertyAnimation *m_shellExpand;
    QPropertyAnimation *m_shellShrink;
    QPropertyAnimation *m_textExpand;
    QPropertyAnimation *m_textShrink;

    
    QSequentialAnimationGroup *m_hideGroup;
    QPropertyAnimation *m_crtSquashY;
    QParallelAnimationGroup *m_crtPhase2Group;
    QPropertyAnimation *m_crtSquashX;
    QPropertyAnimation *m_hideOpacity;

    QRect m_baseGeometry;
};

#endif 
