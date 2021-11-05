#ifndef SXE_JITSON_H
#define SXE_JITSON_H

#include <stdbool.h>
#include <stdint.h>

#define SXE_JITSON_TYPE_INVALID 0
#define SXE_JITSON_TYPE_NUMBER  1
#define SXE_JITSON_TYPE_STRING  2
#define SXE_JITSON_TYPE_OBJECT  3
#define SXE_JITSON_TYPE_ARRAY   4
#define SXE_JITSON_TYPE_BOOL    5
#define SXE_JITSON_TYPE_NULL    6

#define SXE_JITSON_STACK_ERROR  (~0U)
#define SXE_JITSON_TOKEN_SIZE   16

/* A jitson token. Copied strings of > 7 bytes length continue into the next token.
 */
struct sxe_jitson {
    uint32_t type;
    uint32_t size;
    union {
        void   *pointer;
        int64_t integer;
        double  number;
        bool    boolean;
        char    string[8];
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
