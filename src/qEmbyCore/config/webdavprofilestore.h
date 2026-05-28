#ifndef WEBDAVPROFILESTORE_H
#define WEBDAVPROFILESTORE_H

#include "../api/webdav/webdavprofile.h"
#include "../qEmbyCore_global.h"

#include <QObject>
#include <QString>






























class QEMBYCORE_EXPORT WebdavProfileStore : public QObject
{
    Q_OBJECT
public:
    explicit WebdavProfileStore(QObject *parent = nullptr);
    ~WebdavProfileStore() override;

    
    
    
    bool load();

    
    bool save();

    
    bool clear();

    bool hasProfile() const;

    
    WebdavProfile profile() const;

    
    void setProfile(WebdavProfile profile);

    
    bool updateLastSyncAt(qint64 unixMs);

    
    static QString filePath();

signals:
    
    void profileChanged();

private:
    WebdavProfile m_profile;
    bool m_hasProfile = false;
};

#endif 
