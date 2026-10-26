#ifndef PAGETURNOVERLAYWIDGET_H
#define PAGETURNOVERLAYWIDGET_H

#include <QList>
#include <QPixmap>
#include <QPointer>
#include <QWidget>
#include "detailcontentwidget.h"



class PageTurnOverlayWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PageTurnOverlayWidget(DetailContentWidget* source);
    QPixmap captureWidgets(const QList<QWidget*>& widgets, const QSize& sceneSize) const;
    QPixmap currentFrame();
    void setFrames(QPixmap outgoing, QPixmap incoming);
    void setIncomingFrame(QPixmap incoming);
    bool hasFrames() const { return !m_outgoing.isNull() && !m_incoming.isNull(); }
    void clearFrames();

Q_SIGNALS:
    void finished();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void synchronize();
    void updateCanvas();
    QPointer<DetailContentWidget> m_source;
    QPixmap m_outgoing;
    QPixmap m_incoming;
    QPixmap m_frame;
    bool m_dirty = true;
};

#endif 
