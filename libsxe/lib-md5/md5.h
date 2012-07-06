/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * See md5.c for more information.
 */

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

typedef unsigned int MD5_u32plus;    /* Any 32-bit or wider unsigned integer data type will do */

/* This is the MD5 context defined by Openwall. It must be at least as big as openssl's to be compatible
 */
typedef struct MD5_CTX_OPENWALL {
    MD5_u32plus   lo, hi;
    MD5_u32plus   a, b, c, d;
    unsigned char buffer[64];
    MD5_u32plus   block[16];
} MD5_CTX_OPENWALL;

#ifndef SXE_DISABLE_OPENSSL    /* Use openssl */

#include <openssl/md5.h>

#if MD5_DIGEST_LENGTH != MD5_SIZE
#   error "MD5_DIGEST_LENGTH in <openssl/md5.h> is not 16"
#endif

typedef union MD5_CTX_UNION {
    MD5_CTX          md5_openssl;
    MD5_CTX_OPENWALL md5_openwall;
} MD5_CTX_UNION;

static inline void
md5_ctx_construct(MD5_CTX_UNION * ctx)
{
    SXEA6(sizeof(MD5_CTX_OPENWALL) >= sizeof(MD5_CTX), "Increase size of MD5_CTX_OPENWALL from %u to %u",
           (unsigned)sizeof(MD5_CTX_OPENWALL), (unsigned)sizeof(MD5_CTX));
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

#define MD5_CTX                     MD5_CTX_UNION
#define MD5_Init(ctx)               md5_ctx_construct(ctx)
#define MD5_Update(ctx, data, size) md5_ctx_update((ctx), (data), (size))
#define MD5_Final(result, ctx)      md5_ctx_get_sum((ctx), (SOPHOS_MD5 *)(result))

#else    /* Use Openwall's implementation */

#ifdef HEADER_MD5_H
#   error "<openssl/md5.h> included when SXE_DISABLE_OPENSSL is defined"
#endif

#define OPENSSL_NO_MD5 1    /* This should cause <openssl/md5.h> to blow up, if included */

#define MD5_DIGEST_LENGTH MD5_SIZE       /* For openssl compatibility */

typedef MD5_CTX_OPENWALL MD5_CTX;

void MD5_Init(  MD5_CTX *ctx);
void MD5_Update(MD5_CTX *ctx, const void *data, unsigned long size);
void MD5_Final( unsigned char *result, MD5_CTX *ctx);

static inline void
md5_ctx_get_sum(MD5_CTX * ctx, SOPHOS_MD5 * result)
{
    MD5_Final((unsigned char *)result, ctx);
}

#define MD5_Final(result, ctx) md5_ctx_get_sum((ctx), (SOPHOS_MD5 *)(result))

#endif /* Openwall specific stuff */

SXE_RETURN md5_from_hex(SOPHOS_MD5 * md5, const char * md5_in_hex);
SXE_RETURN md5_to_hex(  SOPHOS_MD5 * md5, char * md5_in_hex, unsigned md5_in_hex_length);

#endif
