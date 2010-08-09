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
            return (char *)(unsigned long)buf;
        }
    }

    return NULL;
}

char *
sxe_strnstr(const char * buf, const char * str, unsigned n)
{
    unsigned     length = strlen(str);
    const char * end;

    for (end = &buf[n - length]; buf <= end; buf++) {
        if (*buf == '\0') {
            break;
        }

        if (memcmp(buf, str, length) == 0) {
            return (char *)(unsigned long)buf;
        }
    }

    return NULL;
}

char *
sxe_strncasestr(const char * buf, const char * str, unsigned n)
{
    unsigned     length = strlen(str);
    const char * end;

    for (end = &buf[n - length]; buf <= end; buf++) {
        if (*buf == '\0') {
            break;
        }

        if (strncasecmp(buf, str, length) == 0) {
            return (char *)(unsigned long)buf;
        }
    }

    return NULL;
}

