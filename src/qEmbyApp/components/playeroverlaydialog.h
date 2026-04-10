#ifndef PLAYEROVERLAYDIALOG_H
#define PLAYEROVERLAYDIALOG_H

#include <QPoint>
#include <QSize>
#include <QWidget>

class QLabel;
class QEvent;
class QMouseEvent;
class QShowEvent;
class QResizeEvent;
class QPaintEvent;
class QVBoxLayout;
class QWidget;

class PlayerOverlayDialog : public QWidget
{
    Q_OBJECT

public:
    enum DialogCode
    {
        Rejected = 0,
        Accepted = 1
    };

    explicit PlayerOverlayDialog(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setSurfaceObjectName(const QString &name);
    void setSurfacePreferredSize(const QSize &size);
    QWidget *surfaceWidget() const;

signals:
    void finished(int result);
    void accepted();
    void rejected();

public slots:
    void open();
    void accept();
    void reject();

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    QVBoxLayout *contentLayout() const { return m_contentLayout; }

private:
    void syncOverlayGeometry();
    void updateSurfaceBounds();
    bool isInsideSurface(const QPoint &pos) const;

    QWidget *m_surface = nullptr;
    QWidget *m_titleBarWidget = nullptr;
    QLabel *m_titleLabel = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QSize m_surfacePreferredSize;
    bool m_resultEmitted = false;

    
    bool m_isDragging = false;
    QPoint m_dragStartGlobalPos;
    QPoint m_surfaceDragStartPos;
    QPoint m_surfaceOffset;
};

#endif 
