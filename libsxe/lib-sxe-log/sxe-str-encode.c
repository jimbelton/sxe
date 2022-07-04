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

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>   /* For snprintf on Windows */

#include "sxe-log.h"

char *
sxe_strn_encode(char * buffer, unsigned size, const char * string, unsigned length)
{
    unsigned i;
    unsigned j;

    SXEE6("sxe_strn_encode(buffer=%p,size=%u,string='%.*s',length=%u)", buffer, size, length, string, length);

    for (i = 0, j = 0; (j < length) && (string[j] != '\0'); j++) {
        if (string[j] == ' ') {
            buffer[i++] = '_';
        }
        else if ((string[j] == '_') || (string[j] == '=') || isspace(string[j]) || !isprint(string[j])) {
            if (i + 3 >= size) {
                break;
            }

            snprintf(&buffer[i], 4, "=%02X", string[j]);
            i += 3;
        }
        else {
            buffer[i++] = string[j];
        }

        if (i == size - 1) {
            break;
        }
    }

    assert(i < size);
    buffer[i] = '\0';

    if ((j < length) && (string[j] != '\0')) {
        buffer = NULL;
    }

    SXER6("return buffer=%s", buffer);
    return buffer;
}
