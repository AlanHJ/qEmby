#include "hlsplaylistutils.h"

bool HlsPlaylistUtils::sameOrigin(const QUrl& a, const QUrl& b)
{
    return a.scheme().compare(b.scheme(), Qt::CaseInsensitive) == 0 &&
           a.host().compare(b.host(), Qt::CaseInsensitive) == 0 &&
           a.port(a.scheme() == "https" ? 443 : 80) == b.port(b.scheme() == "https" ? 443 : 80);
}

QByteArray HlsPlaylistUtils::rewrite(const QByteArray& manifest, const QUrl& base,
                                    const std::function<QUrl(const QUrl&)>& mapResource)
{
    QByteArray result;
    for (QByteArray line : manifest.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith('#')) {
            line = mapResource(base.resolved(QUrl::fromEncoded(trimmed))).toEncoded();
        } else if (trimmed.startsWith('#')) {
            
            
            int position = line.indexOf(':') + 1;
            while (position > 0 && position < line.size()) {
                while (position < line.size() && (line[position] == ',' || line[position] == ' ')) ++position;
                const int equal = line.indexOf('=', position);
                if (equal < 0) break;
                const QByteArray key = line.mid(position, equal - position).trimmed();
                int start = equal + 1;
                if (start < line.size() && line[start] == '"') {
                    ++start;
                    const int end = line.indexOf('"', start);
                    if (end < 0) break;
                    if (key == "URI") {
                        const QUrl remote = base.resolved(QUrl::fromEncoded(line.mid(start, end - start)));
                        const QByteArray mapped = mapResource(remote).toEncoded();
                        line.replace(start, end - start, mapped);
                        position = start + mapped.size() + 1;
                    } else position = end + 1;
                } else {
                    const int end = line.indexOf(',', start);
                    position = end < 0 ? line.size() : end + 1;
                }
            }
        }
        result += line + '\n';
    }
    return result;
}
