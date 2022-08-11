#ifndef __SOPHOS_MD5_H__
#define __SOPHOS_MD5_H__

#include <stddef.h>
#include <stdint.h>

#include "sxe-log.h"

#define MD5_SIZE          16
#define MD5_IN_HEX_LENGTH (2 * MD5_SIZE)

typedef struct SOPHOS_MD5_STRUCT {
    unsigned word[MD5_SIZE / sizeof(unsigned)];
} SOPHOS_MD5;

#ifndef SXE_DISABLE_OPENSSL
#   include <openssl/md5.h>

#   if MD5_DIGEST_LENGTH != MD5_SIZE
#       error "MD5_DIGEST_LENGTH in <openssl/md5.h> is not 16"
#   endif

typedef union MD5_CTX_UNION {
    MD5_CTX          md5_openssl;
} MD5_CTX_UNION;

static inline void
md5_ctx_construct(MD5_CTX_UNION * ctx)
{
    MD5_Init((MD5_CTX *)ctx);
}

static inline void
md5_ctx_update(MD5_CTX_UNION * ctx, const void * data, unsigned long size)
{
    MD5_Update((MD5_CTX *)ctx, data, size);
}

static inline void
md5_ctx_get_sum( MD5_CTX_UNION * ctx, SOPHOS_MD5 * result)
{
    MD5_Final((unsigned char *)result, (MD5_CTX *)ctx);
}

#   define MD5_CTX                     MD5_CTX_UNION
#   define MD5_Init(ctx)               md5_ctx_construct(ctx)
#   define MD5_Update(ctx, data, size) md5_ctx_update((ctx), (data), (size))
#   define MD5_Final(result, ctx)      md5_ctx_get_sum((ctx), (SOPHOS_MD5 *)(result))
#endif

SXE_RETURN md5_from_hex(SOPHOS_MD5 * md5, const char * md5_in_hex);
SXE_RETURN md5_to_hex(  SOPHOS_MD5 * md5, char * md5_in_hex, unsigned md5_in_hex_length);

#endif
