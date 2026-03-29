#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <QString>
#include <QtGlobal>
#include "qEmbyCore_global.h"


namespace FileUtils {


QEMBYCORE_EXPORT QString formatSize(qint64 bytes);


QEMBYCORE_EXPORT qint64 calcDirSize(const QString &path);

} 

#endif 
