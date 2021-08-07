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
#include <unistd.h>

#include "sxe-cstr.h"
#include "sxe-log.h"

#define SXE_CSTR_FLAG_IS_CSTR  0x01
#define SXE_CSTR_FLAG_OVERFLOW 0x02
#define SXE_CSTR_FLAG_IS_CONST 0x04

/* NOTE: CSTRs do not guarantee '\0' termination; don't rely on it! */

char sxe_cstr_empty[2] = {'\0', '\0'};    /* The "really" empty string */

SXE_CSTR *
sxe_cstr_cat(SXE_CSTR * cstr, const SXE_CSTR * ccat)
{
    const char * ccat_str;
    unsigned     ccat_len;
    unsigned     size;

    SXEA12(!(cstr->flags & SXE_CSTR_FLAG_IS_CONST), "%s: SXE_CSTR %p is constant", __func__, cstr);

    if (!ccat->literal && (ccat->flags != 0)) {
        ccat_str = ccat->buf;
        ccat_len = ccat->len;
    }
    else {
        ccat_str = (const char *)ccat;
        ccat_len = strlen((const char *)ccat);
    }

    size = cstr->size - cstr->len;
    strncat(&cstr->buf[cstr->len], ccat_str, size);
    cstr->len += ccat_len;

    if (cstr->len > cstr->size) {
        cstr->flags |= SXE_CSTR_FLAG_OVERFLOW;
        cstr->len    = cstr->size;
    }

    return cstr;
}

void
sxe_cstr_clear(SXE_CSTR * cstr)
{
    SXEA12(!(cstr->flags & SXE_CSTR_FLAG_IS_CONST), "%s: SXE_CSTR %p is constant", __func__, cstr);
    cstr->flags &= ~SXE_CSTR_FLAG_OVERFLOW;
    cstr->len    = 0;
    cstr->buf[0] = '\0';
}

int
sxe_cstr_cmp(const SXE_CSTR * left, const SXE_CSTR * right)
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
sxe_cstr_ftime(SXE_CSTR * cstr, const char * format, const struct tm * tm)
{
    size_t ret;

    SXEA12(!(cstr->flags & SXE_CSTR_FLAG_IS_CONST), "%s: SXE_CSTR %p is constant", __func__, cstr);
    ret= strftime(&cstr->buf[cstr->len], cstr->size - cstr->len, format, tm);
    cstr->len += ret;
    return ret;
}

const char *
sxe_cstr_get_str(SXE_CSTR * cstr)
{
    SXEA12(!(cstr->flags & SXE_CSTR_FLAG_IS_CONST), "%s: SXE_CSTR %p is constant", __func__, cstr);

    if (cstr->len >= cstr->size) {
        cstr->len    = cstr->size - 1;
        cstr->flags |= SXE_CSTR_FLAG_OVERFLOW;
    }

    cstr->buf[cstr->len] = '\0';
    return cstr->buf;
}

bool
sxe_cstr_is_overflow(const SXE_CSTR * cstr)
{
    return (cstr->flags & SXE_CSTR_FLAG_OVERFLOW) != 0;
}

unsigned
sxe_cstr_length(const SXE_CSTR * cstr)
{
    if (cstr->literal || (cstr->flags == 0)) {
        return strlen((const char *)cstr);
    }

    return cstr->len;
}

void
sxe_cstr_construct(SXE_CSTR * cstr, char * buf, unsigned len, unsigned size)
{
    SXEA10(cstr != NULL, "sxe_cstr_construct: Can't construct SXE_CSTR with cstr == NULL");
    SXEA10(buf  != NULL, "sxe_cstr_construct: Can't construct SXE_CSTR with buf == NULL");
    SXEA10(size != 0,    "sxe_cstr_construct: Can't construct SXE_CSTR with size == 0");
    SXEA10(len <= size,  "sxe_cstr_construct: Can't construct SXE_CSTR if the length > size");

    cstr->literal   = 0;
    cstr->flags     = SXE_CSTR_FLAG_IS_CSTR;    /* Force flags to be non-zero to allow us to detect empty strings */
    cstr->size      = size;
    cstr->len       = len;
    cstr->buf       = buf;
}

void
sxe_cstr_construct_const(SXE_CSTR * cstr, const char * buf, unsigned len)
{
    sxe_cstr_construct(cstr, (char *)(long)buf, len, len);
    cstr->flags |= SXE_CSTR_FLAG_IS_CONST;
}

int
sxe_cstr_printf(SXE_CSTR * cstr, const char * fmt, ...)
{
    va_list ap;
    int     ret;

    SXEA12(!(cstr->flags & SXE_CSTR_FLAG_IS_CONST), "%s: SXE_CSTR %p is constant", __func__, cstr);
    va_start(ap, fmt);
    ret = sxe_cstr_vprintf(cstr, fmt, ap);
    va_end(ap);
    return ret;
}

int
sxe_cstr_vprintf(SXE_CSTR * cstr, const char * fmt, va_list ap)
{
    unsigned size;
    int      ret;

    SXEA12(!(cstr->flags & SXE_CSTR_FLAG_IS_CONST), "%s: SXE_CSTR %p is constant", __func__, cstr);
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

    cstr->flags |= SXE_CSTR_FLAG_OVERFLOW;
    cstr->len    = cstr->size - 1;
    return ret;
}

unsigned
sxe_cstr_cspn(const SXE_CSTR * cstr, unsigned start_at, const char * reject)
{
    unsigned length;

    if (cstr->literal || (cstr->flags == 0)) {
        return strcspn((const char *)cstr + start_at, reject);
    }

    for (length = 0; start_at + length < cstr->len; length++) {
        if (strchr(reject, cstr->buf[start_at + length]) != NULL) {
            return length;
        }
    }

    return length;
}
