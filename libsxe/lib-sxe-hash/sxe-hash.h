/* Copyright (c) 2010 Sophos Group.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __SXE_HASH_H__
#define __SXE_HASH_H__

#include <stdint.h>

#include "sha1.h"
#include "sxe-pool.h"
#include "sxe-util.h"

#define SXE_HASH_KEY_NOT_FOUND       ~0U
#define SXE_HASH_FULL                ~0U
#define SXE_HASH_SHA1_AS_HEX_LENGTH  SHA1_IN_HEX_LENGTH

#define SXE_HASH_OPTION_UNLOCKED      0
#define SXE_HASH_OPTION_LOCKED        SXE_BIT_OPTION(0)
#define SXE_HASH_OPTION_PREHASHED     0
#define SXE_HASH_OPTION_COMPUTED_HASH SXE_BIT_OPTION(1)

/* For backward compatibility. Lookup3 (the default algorithm) can now be overridden.
 */
#define SXE_HASH_OPTION_LOOKUP3_HASH SXE_HASH_OPTION_COMPUTED_HASH

/* Classic hash: prehashed SHA1 key to unsigned value
 */
typedef struct SXE_HASH_KEY_VALUE_PAIR {
    SOPHOS_SHA1 sha1;
    int         value;
} SXE_HASH_KEY_VALUE_PAIR;

typedef struct SXE_HASH {
    void      * pool;
    unsigned    count;
    unsigned    size;
    unsigned    key_offset;
    unsigned    key_size;
    unsigned    options;
    unsigned (* hash_key)(const void * key, unsigned size);
} SXE_HASH;

extern uint32_t (*sxe_hash_sum)(const void *key, unsigned length);

#include "lib-sxe-hash-proto.h"
#endif

