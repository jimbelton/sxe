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

#ifndef __SXE_RING_BUFFER_H__
#define __SXE_RING_BUFFER_H__

#include "sxe-log.h"
#include "sxe.h"

typedef void * SXE_RING_BUFFER;

typedef struct SXE_RING_BUFFER_CONTEXT {
    unsigned itteration;
    char *   data_block;
    unsigned data_block_len;
    char *   writable_block;
    unsigned writable_block_len;
} SXE_RING_BUFFER_CONTEXT;

#define SXE_RING_BUFFER_BLOCK(x)        ((x)->data_block)
#define SXE_RING_BUFFER_BLOCK_LENGTH(x) ((x)->data_block_len)

#define SXE_RING_BUFFER_WRITABLE_BLOCK(x)        ((x)->writable_block)
#define SXE_RING_BUFFER_WRITABLE_BLOCK_LENGTH(x) ((x)->writable_block_len)

#include "lib-sxe-ring-buffer-proto.h"

#endif

