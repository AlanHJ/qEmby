#ifndef EXTERNALPLAYERDETECTOR_H
#define EXTERNALPLAYERDETECTOR_H

#include <QList>
#include <QString>

struct DetectedPlayer {
    QString name;
    QString path;
};


class ExternalPlayerDetector {
public:
    
    static QList<DetectedPlayer> detect();

    
    static void saveToConfig(const QList<DetectedPlayer> &players);

    
    static QList<DetectedPlayer> loadFromConfig();
};

#endif 
