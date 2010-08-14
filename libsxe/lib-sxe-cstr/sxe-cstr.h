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

#ifndef __SXE_CSTR_H__
#define __SXE_CSTR_H__

#include <stdarg.h>    /* For sxe_cstr_vprintf */
#include <time.h>      /* For sxe_cstr_ftime   */

#define SXE_CSTR_EMPTY         (const sxe_cstr *)"\0"     /* Empty sxe_cstr   */
#define SXE_CSTR_LITERAL(text) (const sxe_cstr *)#text    /* Literal sxe_cstr */

/* A sxe_cstr is a counted string (pronounced keester)
 */
typedef struct SXE_CSTR_IMPL
{
    char            literal;    /* 0 if this is not a literal string. */
    unsigned char   flags;      /* Used to store flags.               */
    unsigned        size;       /* Size of buffer.                    */
    unsigned short  len;        /* Length of buffer.                  */
    char *          buf;        /* Pointer to buffer if static.       */
} sxe_cstr;

#include "lib-sxe-cstr-proto.h"

inline int sxe_cstr_eq(const sxe_cstr * left, const sxe_cstr * right) {return sxe_cstr_cmp(left, right) == 0;}
inline int sxe_cstr_ge(const sxe_cstr * left, const sxe_cstr * right) {return sxe_cstr_cmp(left, right) >= 0;}
inline int sxe_cstr_gt(const sxe_cstr * left, const sxe_cstr * right) {return sxe_cstr_cmp(left, right) >  0;}
inline int sxe_cstr_le(const sxe_cstr * left, const sxe_cstr * right) {return sxe_cstr_cmp(left, right) <= 0;}
inline int sxe_cstr_lt(const sxe_cstr * left, const sxe_cstr * right) {return sxe_cstr_cmp(left, right) <  0;}
inline int sxe_cstr_ne(const sxe_cstr * left, const sxe_cstr * right) {return sxe_cstr_cmp(left, right) != 0;}

#endif
