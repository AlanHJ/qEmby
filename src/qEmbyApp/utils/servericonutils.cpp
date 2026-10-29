#include "servericonutils.h"

#include <models/profile/serverprofile.h>
#include <QDebug>
#include <QPainter>
#include <QSvgRenderer>
#include <QtMath>

namespace ServerIconUtils {

QPixmap loadIcon(const ServerProfile &profile, const QSize &logicalSize,
                 qreal devicePixelRatio)
{
    if (logicalSize.isEmpty())
        return {};

    const bool jellyfin = profile.type == ServerProfile::Jellyfin;
    
    if (!jellyfin && !profile.iconBase64.isEmpty()) {
        QPixmap cached;
        if (cached.loadFromData(QByteArray::fromBase64(profile.iconBase64.toUtf8())))
            return cached;

        
        qWarning() << "[ServerIconUtils] Cached server icon could not be decoded; using brand icon";
    }

    const QString resource = jellyfin ? QStringLiteral(":/svg/jellyfin.svg")
                                     : QStringLiteral(":/svg/emby.svg");
    
    QSvgRenderer renderer(resource);
    if (!renderer.isValid()) {
        qWarning() << "[ServerIconUtils] Invalid bundled brand icon:" << resource;
        return {};
    }

    const qreal dpr = qMax(qreal(1), devicePixelRatio);
    QPixmap result(QSize(qCeil(logicalSize.width() * dpr),
                         qCeil(logicalSize.height() * dpr)));
    result.setDevicePixelRatio(dpr);
    result.fill(Qt::transparent);
    {
        QPainter painter(&result);
        renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(logicalSize)));
    }
    qDebug() << "[ServerIconUtils] Rendered brand icon:" << resource
             << "| logicalSize:" << logicalSize << "| dpr:" << dpr;
    return result;
}

} 
