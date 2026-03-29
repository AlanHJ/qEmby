#ifndef MPVCONTROLLER_H
#define MPVCONTROLLER_H

#include <QObject>
#include <QVariant>
#include <QString>
#include <mpv/client.h>

class MpvController : public QObject {
    Q_OBJECT
public:
    explicit MpvController(QObject *parent = nullptr);
    ~MpvController() override;

    mpv_handle *mpv() const { return m_mpv; }

    
    bool init();

    
    void forceCleanup();

    void observeProperty(const QString &property, mpv_format format, uint64_t id = 0);
    int setProperty(const QString &property, const QVariant &value);
    QVariant getProperty(const QString &property);
    QVariant command(const QVariant &params);

signals:
    
    void propertyChanged(const QString &property, const QVariant &value);

    
    void positionChanged(double position);
    void durationChanged(double duration);
    void playbackStateChanged(bool isPaused);
    void fileLoaded();
    void endOfFile(const QString &reason);
    void errorOccurred(const QString &errorMsg);

public slots:
    void eventHandler();

private:
    static void onMpvEvents(void *ctx);

    
    void setNode(mpv_node *dst, const QVariant &src);
    void freeNode(mpv_node *dst);
    QVariant nodeToVariant(const mpv_node *node);
    mpv_node_list *createList(mpv_node *dst, bool is_map, int num);

    mpv_handle *m_mpv;
};

#endif 
