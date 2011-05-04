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

/* This module implements the sxe_strlcpy() and sxe_strlcat(), which are like the strlcpy and strlcat functions from OpenBSD.
 *  - See the rationale for these functions here: http://www.gratisoft.us/todd/papers/strlcpy.html
 *  - These functions are implemented here due their use in the log package
 */

#include <string.h>
#include "sxe-log.h"

/**
 * Length limited string copy that sucks less than strncpy
 *
 * @param destination Pointer to destination memory
 * @param source      Pointer to NUL terminated source string to copy
 * @param size        Size of destination memory
 *
 * @return The length of the source string; if this is greater than size, the string has been truncated to length (size - 1)
 */
size_t
sxe_strlcpy(char * destination, const char * source, size_t size)
{
    size_t i;

    for (i = 0; i < size; i++) {
        destination[i] = source[i];

        if (destination[i] == '\0') {
            goto SXE_EARLY_OUT;
        }
    }

    destination[size - 1] = '\0';

    for (; source[i] != '\0'; i++) {
    }

SXE_EARLY_OUT:
    return i;
}

/**
 * Length limited string concatenation that sucks less than strncat
 *
 * @param destination Pointer to destination memory
 * @param source      Pointer to NUL terminated source string to concatenate
 * @param size        Size of destination memory
 *
 * @return The length of the resulting string, if this is less than size; otherwise, the length the concatenated string would
 *         have been if it wasn't truncated
 */
size_t
sxe_strlcat(char * destination, const char * source, size_t size)
{
    size_t length;
    size_t i;

    /* Skip over the existing string
     */
    for (length = 0; length < size && destination[length] != '\0'; length++) {
    }

    for (i = 0; i < size - length; i++) {
        destination[i + length] = source[i];

        if (source[i] == '\0') {
            goto SXE_EARLY_OUT;
        }
    }

    destination[size - 1] = '\0';

    for (; source[i] != '\0'; i++) {
    }

SXE_EARLY_OUT:
    return length + i;
}
