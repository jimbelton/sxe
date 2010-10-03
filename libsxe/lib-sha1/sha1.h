#ifndef SHA1_DOT_H
#define SHA1_DOT_H

#include <stddef.h>
#include <stdint.h>

#include "sxe-log.h"

typedef struct SOPHOS_SHA1_STRUCT {
    uint32_t word[5];
} SOPHOS_SHA1;

/* SHA1 construction data structure
 */
typedef struct
{
    unsigned Message_Digest[5]; /* Message Digest (output)          */

    unsigned Length_Low;        /* Message length in bits           */
    unsigned Length_High;       /* Message length in bits           */

    unsigned char Message_Block[64]; /* 512-bit message blocks      */
    int      Message_Block_Index;    /* Index into message block array   */

    int      Computed;               /* Is the digest computed?          */
    int      Corrupted;              /* Is the message digest corruped?  */
} sophos_sha1_ctx_t;

#include "lib-sha1-proto.h"

#endif
