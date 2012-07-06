/* Copyright (c) 2009 Jim Belton
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

#ifndef __SXE_BUFFER_H__
#define __SXE_BUFFER_H__

#include <limits.h>
#include <stdarg.h>    /* For sxe_buffer_vprintf */
#include <stdbool.h>
#include <time.h>      /* For sxe_buffer_ftime   */

#include "sxe-list.h"

#define SXE_BUFFER_EMPTY      ((const SXE_BUFFER *)sxe_buffer_empty)
#define SXE_BUFFER_CAST(text) ((const SXE_BUFFER *)((text)[0] == '\0' ? sxe_buffer_empty : (const char *)(text)))
#define SXE_BUFFER_MAX_LEN    USHRT_MAX

/* A sxe_buffer is a counted string
 */
typedef struct SXE_BUFFER
{
    char                   literal;      /* 0 if this is not a literal string                             */
    unsigned char          flags;        /* Used to store flags                                           */
    unsigned short         size;         /* Size of buffer                                                */
    unsigned short         length;       /* Length of buffer                                              */
    unsigned short         consumed;     /* Number of bytes consumed out of buffer                        */
    char                 * space;        /* Pointer to buffer space                                       */
    struct SXE_LIST_NODE   link;         /* List node (link to next buffer in a list)                     */
    void                 * user_data;    /* Passed to on_empty, which is called when all data is consumed */
    void                (* on_empty)(void * user_data, struct SXE_BUFFER * buffer);
} SXE_BUFFER;

extern char sxe_buffer_empty[];

#include "lib-sxe-buffer-proto.h"

static inline bool sxe_buffer_eq(const SXE_BUFFER * left, const SXE_BUFFER * right) {return sxe_buffer_cmp(left, right) == 0;}
static inline bool sxe_buffer_ge(const SXE_BUFFER * left, const SXE_BUFFER * right) {return sxe_buffer_cmp(left, right) >= 0;}
static inline bool sxe_buffer_gt(const SXE_BUFFER * left, const SXE_BUFFER * right) {return sxe_buffer_cmp(left, right) >  0;}
static inline bool sxe_buffer_le(const SXE_BUFFER * left, const SXE_BUFFER * right) {return sxe_buffer_cmp(left, right) <= 0;}
static inline bool sxe_buffer_lt(const SXE_BUFFER * left, const SXE_BUFFER * right) {return sxe_buffer_cmp(left, right) <  0;}
static inline bool sxe_buffer_ne(const SXE_BUFFER * left, const SXE_BUFFER * right) {return sxe_buffer_cmp(left, right) != 0;}

static inline void sxe_buffer_list_construct(SXE_LIST * list) {SXE_LIST_CONSTRUCT(list, 0, SXE_BUFFER, link);}

#endif
