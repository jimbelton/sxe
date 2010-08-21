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

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sxe-log.h"
#include "sxe-util.h"

#define SXE_STR_PRINTABLE_SIMULTANEOUSLY 10
#define SXE_STR_PRINTABLE_LENGTH_MAXIMUM 1020

static unsigned sxe_str_next = 0;
static char     sxe_str_printable[SXE_STR_PRINTABLE_SIMULTANEOUSLY][SXE_STR_PRINTABLE_LENGTH_MAXIMUM + sizeof("...")];

/**
 * Convert a string into a printable string
 *
 * @param str = String to convert
 *
 * @return str if the string is already printable or is NULL, or up to the first 1020 characters of the string converted into a
 *         printable form, returned in the least recently used of a set of 10 buffers.
 *
 * @note This function is not thread safe. Also, as it is expected to be used in logging, it does not itself log.
 */

const char *
sxe_str_to_printable(const char * str)
{
    unsigned from;
    unsigned to;
    unsigned this;

    if (str == NULL) {
        return NULL;
    }

    for (from = 0; (str[from] != '\0') && (isprint((unsigned char)str[from]) || (str[from] == ' ')); from++) {
    }

    if (str[from] == '\0') {
        return str;
    }

    /* Note: Currently not thread safe.
     */
    this         = sxe_str_next;
    sxe_str_next = (this + 1) % SXE_STR_PRINTABLE_SIMULTANEOUSLY;

    memcpy(sxe_str_printable[this], str, from > SXE_STR_PRINTABLE_LENGTH_MAXIMUM ? SXE_STR_PRINTABLE_LENGTH_MAXIMUM : from);

    for (to = from; from < SXE_STR_PRINTABLE_LENGTH_MAXIMUM; from++) {
        if (str[from] == '\0') {
            sxe_str_printable[this][to++] = '\0';
            break;
        }

        if (isprint((unsigned char)str[from]) || (str[from] == ' ')) {
            sxe_str_printable[this][to++] = str[from];
            continue;
        }

        sxe_str_printable[this][to++] = '\\';
        sxe_str_printable[this][to++] = 'x';
        snprintf(&sxe_str_printable[this][to], 3, "%02x", (unsigned char)str[from]);
        to += 2;
    }

    if (sxe_str_printable[this][to - 1] != '\0') {
        memcpy(&sxe_str_printable[this][SXE_STR_PRINTABLE_LENGTH_MAXIMUM], "...", sizeof("..."));
    }

    return sxe_str_printable[this];
}
