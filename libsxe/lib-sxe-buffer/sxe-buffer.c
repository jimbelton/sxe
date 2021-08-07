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
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sxe-buffer.h"
#include "sxe-log.h"
#include "sxe-util.h"

#define SXE_BUFFER_FLAG_IS_CSTR  0x01
#define SXE_BUFFER_FLAG_OVERFLOW 0x02
#define SXE_BUFFER_FLAG_IS_CONST 0x04

/* NOTE: buffers do not guarantee '\0' termination; don't rely on it! */

char sxe_buffer_empty[2] = {'\0', '\0'};    /* The "really" empty string */

SXE_BUFFER *
sxe_buffer_cat(SXE_BUFFER * buffer, const SXE_BUFFER * ccat)
{
    const char * ccat_str;
    unsigned     ccat_len;

    SXEA1(!(buffer->flags & SXE_BUFFER_FLAG_IS_CONST), ": SXE_BUFFER %p is constant", buffer);

    if (!ccat->literal && (ccat->flags != 0)) {
        ccat_str = ccat->space;
        ccat_len = ccat->length;
    }
    else {
        ccat_str = (const char *)ccat;
        ccat_len = strlen((const char *)ccat);
    }

    sxe_buffer_memcpy(buffer, ccat_str, ccat_len);
    return buffer;
}

void
sxe_buffer_clear(SXE_BUFFER * buffer)
{
    SXEA1(!(buffer->flags & SXE_BUFFER_FLAG_IS_CONST), ": SXE_BUFFER %p is constant", buffer);
    buffer->flags &= ~SXE_BUFFER_FLAG_OVERFLOW;
    buffer->length    = 0;
    buffer->space[0] = '\0';
}

int
sxe_buffer_cmp(const SXE_BUFFER * left, const SXE_BUFFER * right)
{
    const char * left_str  = (const char *)left;
    const char * right_str = (const char *)right;

    if (left == right) {
        return 0;
    }

    if (!left->literal && (left->flags != 0)) {
        left_str = left->space;
    }

    if (!right->literal && (right->flags != 0)) {
        right_str = right->space;
    }

    return strcmp(left_str, right_str);
}

void
sxe_buffer_consume(SXE_BUFFER * buffer, unsigned bytes)
{
    SXEE6("(buffer=%p,bytes=%u)", buffer, bytes);
    SXEA1(!buffer->literal && (buffer->flags != 0),               ": Can't consume data from a literal string");
    SXEA1(bytes <= (unsigned)(buffer->length - buffer->consumed), ": Can't consume %u bytes from buffer: only %u left", bytes,
          (unsigned)(buffer->length - buffer->consumed));
    buffer->consumed += bytes;
    SXER6("return");
}

SXE_BUFFER *
sxe_buffer_memcpy(SXE_BUFFER * buffer, const char * data, unsigned length)
{
    SXEA1(!(buffer->flags & SXE_BUFFER_FLAG_IS_CONST), ": SXE_BUFFER %p is constant", buffer);
    SXEA6(buffer->length <= buffer->size,              ": SXE_BUFFER %p has length %u > size %u",
          buffer, buffer->length, buffer->size);

    if (length > (unsigned)(buffer->size - buffer->length)) {
        buffer->flags |= SXE_BUFFER_FLAG_OVERFLOW;
        length         = buffer->size - buffer->length;
    }

    memcpy(&buffer->space[buffer->length], data, length);
    buffer->length += length;
    return buffer;
}

size_t
sxe_buffer_ftime(SXE_BUFFER * buffer, const char * format, const struct tm * tm)
{
    size_t ret;

    SXEA1(!(buffer->flags & SXE_BUFFER_FLAG_IS_CONST), ": SXE_BUFFER %p is constant", buffer);
    ret= strftime(&buffer->space[buffer->length], buffer->size - buffer->length, format, tm);
    buffer->length += ret;
    return ret;
}

const char *
sxe_buffer_get_data(const SXE_BUFFER * buffer)
{
    if (buffer->literal || (buffer->flags == 0)) {
        return (const char *)buffer;
    }

    return buffer->space + buffer->consumed;
}

unsigned
sxe_buffer_get_room(const SXE_BUFFER * buffer)
{
    SXEA1(!buffer->literal && (buffer->flags != 0), ": Can't get the room left in a literal");
    return buffer->size - buffer->length;
}

const char *
sxe_buffer_get_str(SXE_BUFFER * buffer)
{
    SXEA1(!(buffer->flags & SXE_BUFFER_FLAG_IS_CONST), ": SXE_BUFFER %p is constant", buffer);

    if (buffer->length >= buffer->size) {
        buffer->length    = buffer->size - 1;
        buffer->flags |= SXE_BUFFER_FLAG_OVERFLOW;
    }

    buffer->space[buffer->length] = '\0';
    return buffer->space;
}

bool
sxe_buffer_is_overflow(const SXE_BUFFER * buffer)
{
    return (buffer->flags & SXE_BUFFER_FLAG_OVERFLOW ? true : false);
}

unsigned
sxe_buffer_length(const SXE_BUFFER * buffer)
{
    if (buffer->literal || (buffer->flags == 0)) {
        return strlen((const char *)buffer);
    }

    return buffer->length - buffer->consumed;
}

void
sxe_buffer_construct(SXE_BUFFER * buffer, char * space, unsigned length, unsigned size)
{
    SXEE6("(buffer=%p,space=%p,length=%u,size=%u)", buffer, space, length, size);
    SXEA1(size   <= USHRT_MAX, ": Can't construct a SXE_BUFFER of %u bytes (maximum is %u bytes)", size, USHRT_MAX);
    SXEA1(length <= size,      ": Can't construct a %u byte buffer containing %u bytes of data", size, length);
    SXEA1(buffer != NULL,      ": Can't construct SXE_BUFFER with buffer == NULL");
    SXEA1(space  != NULL,      ": Can't construct SXE_BUFFER with space == NULL");
    SXEA1(size   != 0,         ": Can't construct SXE_BUFFER with size == 0");
    SXEA1(length <= size,      ": Can't construct SXE_BUFFER if the length > size");

    buffer->literal  = 0;
    buffer->flags    = SXE_BUFFER_FLAG_IS_CSTR;    /* Force flags to be non-zero to allow us to detect empty strings */
    buffer->size     = size;
    buffer->length   = length;
    buffer->consumed = 0;
    buffer->space    = space;
    SXER6("return");
}

void
sxe_buffer_construct_const(SXE_BUFFER * buffer, const char * buf, unsigned length)
{
    sxe_buffer_construct(buffer, SXE_CAST_NOCONST(char *, buf), length, length);
    buffer->flags |= SXE_BUFFER_FLAG_IS_CONST;
}

int
sxe_buffer_printf(SXE_BUFFER * buffer, const char * fmt, ...)
{
    va_list ap;
    int     ret;

    SXEA1(!(buffer->flags & SXE_BUFFER_FLAG_IS_CONST), ": SXE_BUFFER %p is constant", buffer);
    va_start(ap, fmt);
    ret = sxe_buffer_vprintf(buffer, fmt, ap);
    va_end(ap);
    return ret;
}

int
sxe_buffer_vprintf(SXE_BUFFER * buffer, const char * fmt, va_list ap)
{
    unsigned size;
    int      ret;

    SXEA1(!(buffer->flags & SXE_BUFFER_FLAG_IS_CONST), ": SXE_BUFFER %p is constant", buffer);
    size = buffer->size - buffer->length;
    ret  = vsnprintf(&buffer->space[buffer->length], size, fmt, ap);

    /* Under Windows, -1 is returned on overflow.
     */
    if (ret < 0) {
        ret = size;                                                                      /* Coverage Exclusion - Windows only */
        buffer->space[buffer->size - 1] = '\0';    /* Force NUL termination */           /* Coverage Exclusion - Windows only */
    }

    if ((unsigned)ret < size) {
        buffer->length += ret;
        return ret;
    }

    buffer->flags |= SXE_BUFFER_FLAG_OVERFLOW;
    buffer->length    = buffer->size - 1;
    return ret;
}

unsigned
sxe_buffer_cspn(const SXE_BUFFER * buffer, unsigned start_at, const char * reject)
{
    unsigned length;

    if (buffer->literal || (buffer->flags == 0)) {
        return strcspn((const char *)buffer + start_at, reject);
    }

    for (length = 0; start_at + length < buffer->length; length++) {
        if (strchr(reject, buffer->space[start_at + length]) != NULL) {
            return length;
        }
    }

    return length;
}
