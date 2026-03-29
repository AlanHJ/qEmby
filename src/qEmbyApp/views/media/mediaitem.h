#ifndef MEDIAITEM_H
#define MEDIAITEM_H

#include <QString>
#include <QJsonObject>
#include <QMetaType> 

struct MediaItem {
    QString id;
    QString name;
    QString type;           
    QString collectionType; 
    QString primaryImageTag; 
    int productionYear = 0;  

    
    static MediaItem fromJson(const QJsonObject& obj) {
        MediaItem item;
        item.id = obj["Id"].toString();
        item.name = obj["Name"].toString();
        item.type = obj["Type"].toString();
        item.collectionType = obj["CollectionType"].toString();

        if (obj.contains("ProductionYear")) {
            item.productionYear = obj["ProductionYear"].toInt();
        }

        if (obj.contains("ImageTags")) {
            QJsonObject tags = obj["ImageTags"].toObject();
            if (tags.contains("Primary")) {
                item.primaryImageTag = tags["Primary"].toString();
            }
        }
        return item;
    }
};


Q_DECLARE_METATYPE(MediaItem)

#endif 
