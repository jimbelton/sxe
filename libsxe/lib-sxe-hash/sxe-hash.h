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

#define SXE_HASH_KEY_NOT_FOUND         ~0U
#define SXE_HASH_FULL                  ~0U
#define SXE_HASH_SHA1_AS_CHAR_LENGTH    40
#define SXE_HASH_SHA1_AS_UINT_LENGTH    5

typedef struct SXE_HASH_KEY_VALUE_PAIR {
    uint32_t      sha1_as_uint[SXE_HASH_SHA1_AS_UINT_LENGTH];
    int           value;
} SXE_HASH_KEY_VALUE_PAIR;

typedef struct SXE_HASH {
    SXE_HASH_KEY_VALUE_PAIR * pool;
    unsigned                  size;
} SXE_HASH;

#include "sxe-hash-proto.h"
#endif

