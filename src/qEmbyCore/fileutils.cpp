#include "fileutils.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace FileUtils {

QString formatSize(qint64 bytes) {
  if (bytes < 1024)
    return QString("%1 B").arg(bytes);
  if (bytes < 1024 * 1024)
    return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
  if (bytes < 1024LL * 1024 * 1024)
    return QString("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
  return QString("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

qint64 calcDirSize(const QString &path) {
  qint64 total = 0;
  QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    total += it.fileInfo().size();
  }
  return total;
}

} 
