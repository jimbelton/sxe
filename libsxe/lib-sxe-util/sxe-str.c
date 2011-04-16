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

#include <string.h>
#include "sxe-util.h"

char *
sxe_strnchr(const char * buf, char c, unsigned n)
{
    const char * end;

    for (end = &buf[n - 1]; buf <= end; buf++) {
        if (*buf == '\0') {
            break;
        }

        if (*buf == c) {
            return SXE_CAST_NOCONST(char *, buf);
        }
    }

    return NULL;
}

char *
sxe_strncspn(const char * buf, const char * reject, unsigned n)
{
    unsigned i;
    unsigned j;

    for (i = 0; (i < n) && (buf[i] != '\0'); i++) {
        for (j = 0; reject[j] != '\0'; j++) {
            if (buf[i] == reject[j]) {
                return SXE_CAST_NOCONST(char *, &buf[i]);
            }
        }
    }

    return NULL;
}

char *
sxe_strnstr(const char * buf, const char * str, unsigned n)
{
    unsigned     length = strlen(str);
    const char * end;

    if (n < length) {
        return NULL;
    }

    for (end = &buf[n - length]; buf <= end; buf++) {
        if (*buf == '\0') {
            break;
        }

        if (memcmp(buf, str, length) == 0) {
            return SXE_CAST_NOCONST(char *, buf);
        }
    }

    return NULL;
}

char *
sxe_rstrnstr(const char * haystack, const char * needle, unsigned haystack_len)
{
    unsigned needle_len = strlen(needle);
    const char * ptr;

    for (ptr = haystack + haystack_len - needle_len; ptr >= haystack; ptr--) {
        if (memcmp(ptr, needle, needle_len) == 0) {
            return SXE_CAST_NOCONST(char *, ptr);
        }
    }

    return NULL;
}

char *
sxe_strncasestr(const char * buf, const char * str, unsigned n)
{
    unsigned     length = strlen(str);
    const char * end;

    if (n < length) {
        return NULL;
    }

    for (end = &buf[n - length]; buf <= end; buf++) {
        if (*buf == '\0') {
            break;
        }

        if (strncasecmp(buf, str, length) == 0) {
            return SXE_CAST_NOCONST(char *, buf);
        }
    }

    return NULL;
}

