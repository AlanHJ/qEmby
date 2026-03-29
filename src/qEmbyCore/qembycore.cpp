#include "qembycore.h"
#include "api/networkmanager.h"
#include "services/manager/servermanager.h"
#include "services/auth/authservice.h"
#include "services/media/mediaservice.h" 
#include "services/admin/adminservice.h" 

QEmbyCore::QEmbyCore(QObject *parent)
    : QObject(parent)
{
    
    m_networkManager = new NetworkManager(this);

    
    m_serverManager = new ServerManager(m_networkManager, this);

    
    m_authService = new AuthService(m_networkManager, m_serverManager, this);

    
    m_mediaService = new MediaService(m_serverManager, this);

    
    m_adminService = new AdminService(m_serverManager, this);
}

QEmbyCore::~QEmbyCore()
{
    
    
    
}

ServerManager* QEmbyCore::serverManager() const
{
    return m_serverManager;
}

AuthService* QEmbyCore::authService() const
{
    return m_authService;
}

MediaService* QEmbyCore::mediaService() const
{
    return m_mediaService;
}

AdminService* QEmbyCore::adminService() const
{
    return m_adminService;
}
