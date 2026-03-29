#ifndef SPLITPLAYERBUTTON_H
#define SPLITPLAYERBUTTON_H

#include <QWidget>
#include <QIcon>
#include <QMenu>

struct DetectedPlayer;



class SplitPlayerButton : public QWidget {
    Q_OBJECT

    
    Q_PROPERTY(QColor hoverColor     READ hoverColor     WRITE setHoverColor)
    Q_PROPERTY(QColor pressedColor   READ pressedColor   WRITE setPressedColor)
    Q_PROPERTY(QColor separatorColor READ separatorColor WRITE setSeparatorColor)
    Q_PROPERTY(QColor arrowColor     READ arrowColor     WRITE setArrowColor)
    Q_PROPERTY(QColor textColor      READ textColor      WRITE setTextColor)
    Q_PROPERTY(int    borderRadius   READ borderRadius   WRITE setBorderRadius)

public:
    explicit SplitPlayerButton(QWidget *parent = nullptr);

    
    void setPlayers(const QList<DetectedPlayer> &players, const QString &activePlayerPath);
    void clear();

    
    void setIconOnly(bool iconOnly);
    bool isIconOnly() const { return m_iconOnly; }

    
    QColor hoverColor()     const { return m_hoverColor; }
    QColor pressedColor()   const { return m_pressedColor; }
    QColor separatorColor() const { return m_separatorColor; }
    QColor arrowColor()     const { return m_arrowColor; }
    QColor textColor()      const { return m_textColor; }
    int    borderRadius()   const { return m_borderRadius; }

    void setHoverColor    (const QColor &c) { m_hoverColor     = c; update(); }
    void setPressedColor  (const QColor &c) { m_pressedColor   = c; update(); }
    void setSeparatorColor(const QColor &c) { m_separatorColor = c; update(); }
    void setArrowColor    (const QColor &c) { m_arrowColor     = c; update(); }
    void setTextColor     (const QColor &c) { m_textColor      = c; update(); }
    void setBorderRadius  (int r)           { m_borderRadius   = r; update(); }

signals:
    
    void playRequested(const QString &playerPath);

    
    void playerSelected(const QString &playerPath);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    QSize sizeHint() const override;

private:
    enum Zone { None, MainZone, ArrowZone };

    Zone zoneAt(const QPoint &pos) const;
    int  arrowAreaWidth() const;
    void showPlayerMenu();

    
    QIcon   m_activeIcon;
    QString m_activeText;
    QString m_activePath;
    bool    m_hasMultiple = false;

    QMenu  *m_menu;

    Zone m_hoverZone   = None;
    Zone m_pressedZone = None;

    
    QColor m_hoverColor     {255, 255, 255, 30};
    QColor m_pressedColor   {255, 255, 255, 20};
    QColor m_separatorColor {255, 255, 255, 50};
    QColor m_arrowColor     {255, 255, 255, 180};
    QColor m_textColor      {255, 255, 255, 180};
    int    m_borderRadius   = 6;
    bool   m_iconOnly       = false;
};

#endif 
