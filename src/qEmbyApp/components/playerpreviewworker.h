#ifndef PLAYERPREVIEWWORKER_H
#define PLAYERPREVIEWWORKER_H

#include "../utils/mpvpreviewutils.h"
#include <QObject>


class PlayerPreviewWorker : public QObject
{
    Q_OBJECT
public:
    explicit PlayerPreviewWorker(QObject *parent = nullptr);

public slots:
    void decode(MpvPreviewUtils::Request request);
    void reset();
    void shutdown();

signals:
    void completed(MpvPreviewUtils::Result result);
    void stopped();

private:
    MpvPreviewUtils::Session m_session;
};

#endif
