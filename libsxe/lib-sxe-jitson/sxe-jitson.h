#ifndef SXE_JITSON_H
#define SXE_JITSON_H

#include <stdbool.h>

#define SXE_JITSON_TYPE_INVALID 0
#define SXE_JITSON_TYPE_NUMBER  1
#define SXE_JITSON_TYPE_STRING  2
#define SXE_JITSON_TYPE_OBJECT  3
#define SXE_JITSON_TYPE_ARRAY   4
#define SXE_JITSON_TYPE_BOOL    5
#define SXE_JITSON_TYPE_NULL    6

#define SXE_JITSON_STACK_ERROR  (~0U)

struct sxe_jitson {
    unsigned type;
    unsigned size;
    union {
        void    *pointer;
        intptr_t integer;
        double   number;
        bool     boolean;
    };
};

struct sxe_jitson_stack {
    unsigned           maximum;
    unsigned           count;
    struct sxe_jitson *jitsons;
};

#include "sxe-jitson-proto.h"

#define MOCK_FAIL_STACK_NEW_OBJECT  ((char *)sxe_jitson_new + 0)
#define MOCK_FAIL_STACK_NEW_JITSONS ((char *)sxe_jitson_new + 1)
#define MOCK_FAIL_STACK_DUP         ((char *)sxe_jitson_new + 2)
#define MOCK_FAIL_STACK_NEXT        ((char *)sxe_jitson_new + 3)

#endif
