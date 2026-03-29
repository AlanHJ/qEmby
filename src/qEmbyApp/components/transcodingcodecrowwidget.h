#ifndef TRANSCODINGCODECROWWIDGET_H
#define TRANSCODINGCODECROWWIDGET_H

#include <QFrame>
#include <QPoint>
#include <QString>

class ElidedLabel;
class ModernSwitch;
class QPushButton;

class TranscodingCodecRowWidget : public QFrame {
    Q_OBJECT
    Q_PROPERTY(QPoint pos READ pos WRITE move)

public:
    explicit TranscodingCodecRowWidget(const QString& codecId,
                                       const QString& title,
                                       const QString& description,
                                       const QString& infoToolTip,
                                       const QString& dragToolTip,
                                       QWidget* parent = nullptr);

    QString codecId() const { return m_codecId; }
    bool isCodecEnabled() const;
    void setCodecEnabled(bool enabled);
    void setDragging(bool dragging);
    QSize sizeHint() const override;

signals:
    void infoRequested(const QString& codecId);
    void enabledChanged(const QString& codecId, bool enabled);
    void dragStarted(TranscodingCodecRowWidget* row, const QPoint& globalPos);
    void dragMoved(TranscodingCodecRowWidget* row, const QPoint& globalPos);
    void dragFinished(TranscodingCodecRowWidget* row);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QString m_codecId;
    ElidedLabel* m_titleLabel = nullptr;
    ElidedLabel* m_descriptionLabel = nullptr;
    QPushButton* m_infoButton = nullptr;
    QPushButton* m_dragHandle = nullptr;
    ModernSwitch* m_switch = nullptr;
    QPoint m_dragStartPos;
    bool m_dragCandidate = false;
    bool m_dragActive = false;
};

#endif 
