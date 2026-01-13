#ifndef NHS_DRIVER_GLOBAL_H
#define NHS_DRIVER_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(NHS_DRIVER_LIBRARY)
#define NHS_DRIVER_EXPORT Q_DECL_EXPORT
#else
#define NHS_DRIVER_EXPORT Q_DECL_IMPORT
#endif

#endif // NHS_DRIVER_GLOBAL_H
