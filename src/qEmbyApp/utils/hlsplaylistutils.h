#pragma once
#include <QByteArray>
#include <QUrl>
#include <functional>
namespace HlsPlaylistUtils {

QByteArray rewrite(const QByteArray& manifest, const QUrl& base,
                   const std::function<QUrl(const QUrl&)>& mapResource);
bool sameOrigin(const QUrl& a, const QUrl& b);
}
