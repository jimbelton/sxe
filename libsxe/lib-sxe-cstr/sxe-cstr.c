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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sxe-cstr.h"
#include "sxe-log.h"

#define SXE_CSTR_OVERFLOW   0x01
#define SXE_CSTR_STATIC     0x02

sxe_cstr *
sxe_cstr_cat(sxe_cstr * cstr, const sxe_cstr * ccat)
{
    const char * ccat_str;
    unsigned     ccat_len;
    unsigned     size;

    if (!ccat->literal && (ccat->flags != 0)) {
        ccat_str = ccat->buf;
        ccat_len = ccat->len;
    }
    else {
        ccat_str = (const char *)ccat;
        ccat_len = strlen((const char *)ccat);
    }

    size = cstr->size - cstr->len;
    strncat(&cstr->buf[cstr->len], ccat_str, size - 1);
    cstr->len += ccat_len;

    if (cstr->len < cstr->size) {
        return cstr;
    }

    cstr->flags |= SXE_CSTR_OVERFLOW;
    cstr->len    = cstr->size - 1;
    return cstr;
}

void
sxe_cstr_clear(sxe_cstr * cstr)
{
    cstr->flags &= ~SXE_CSTR_OVERFLOW;
    cstr->len    = 0;
    cstr->buf[0] = '\0';
}

int
sxe_cstr_cmp(const sxe_cstr * left, const sxe_cstr * right)
{
    const char * left_str  = (const char *)left;
    const char * right_str = (const char *)right;

    if (left == right) {
        return 0;
    }

    if (!left->literal && (left->flags != 0)) {
        left_str = left->buf;
    }

    if (!right->literal && (right->flags != 0)) {
        right_str = right->buf;
    }

    return strcmp(left_str, right_str);
}

size_t
sxe_cstr_ftime(sxe_cstr * cstr, const char * format, const struct tm * tm)
{
    size_t ret;

    ret= strftime(&cstr->buf[cstr->len], cstr->size - cstr->len, format, tm);
    cstr->len += ret;
    return ret;
}

const char *
sxe_cstr_get_str(const sxe_cstr * cstr)
{
    if (cstr->literal || (cstr->flags == 0)) {
        return (const char *)cstr;
    }

    return cstr->buf;
}

int
sxe_cstr_is_overflow(const sxe_cstr * cstr)
{
    return (cstr->flags & SXE_CSTR_OVERFLOW ? 1 : 0);
}

unsigned
sxe_cstr_length(const sxe_cstr * cstr)
{
    if (cstr->literal || (cstr->flags == 0)) {
        return strlen((const char *)cstr);
    }

    return cstr->len;
}

void
sxe_cstr_make(sxe_cstr * cstr, char * buf, unsigned size)
{
    SXEA10(cstr != NULL, "sxe_cstr_make: Can't construct sxe_cstr with cstr == NULL");
    SXEA10(buf  != NULL, "sxe_cstr_make: Can't construct sxe_cstr with buf == NULL");
    SXEA10(size != 0,    "sxe_cstr_make: Can't construct sxe_cstr with size == 0");

    cstr->literal   = 0;
    cstr->flags     = SXE_CSTR_STATIC;
    cstr->size      = size;
    cstr->len       = 0;
    cstr->buf       = buf;
    cstr->buf[0]    = '\0';
}

int
sxe_cstr_printf(sxe_cstr * cstr, const char * fmt, ...)
{
    va_list ap;
    int     ret;

    va_start(ap, fmt);
    ret = sxe_cstr_vprintf(cstr, fmt, ap);
    va_end(ap);
    return ret;
}

int
sxe_cstr_vprintf(sxe_cstr * cstr, const char * fmt, va_list ap)
{
    unsigned size;
    int      ret;

    size = cstr->size - cstr->len;
    ret  = vsnprintf(&cstr->buf[cstr->len], size, fmt, ap);

    /* Under Windows, -1 is returned on overflow.
     */
    if (ret < 0) {
        ret = size;                                                                      /* Coverage Exclusion - Windows only */
        cstr->buf[cstr->size - 1] = '\0';    /* Force NUL termination */                 /* Coverage Exclusion - Windows only */
    }

    if ((unsigned)ret < size) {
        cstr->len += ret;
        return ret;
    }

    cstr->flags |= SXE_CSTR_OVERFLOW;
    cstr->len    = cstr->size - 1;
    return ret;
}
