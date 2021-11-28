#ifndef SXE_FACTORY_H
#define SXE_FACTORY_H

/* Memory allocating factory
 */
struct sxe_factory {
    size_t len;
    size_t size;
    size_t maxincr;
    char  *data;
};

#include "sxe-factory-proto.h"

#endif
