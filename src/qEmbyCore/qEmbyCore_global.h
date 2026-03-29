#ifndef QEMBYCORE_GLOBAL_H
#define QEMBYCORE_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(QEMBYCORE_LIBRARY)
#define QEMBYCORE_EXPORT Q_DECL_EXPORT
#else
#define QEMBYCORE_EXPORT Q_DECL_IMPORT
#endif

#endif 
