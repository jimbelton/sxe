/* Copyright (c) 2021 Jim Belton
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

#include "sxe-log.h"
#include "sxe-unicode.h"

/* From https://www.ietf.org/rfc/rfc3629.txt
 *
 * Char. number range  |        UTF-8 octet sequence
 *    (hexadecimal)    |              (binary)
 * --------------------+---------------------------------------------
 * 0000 0000-0000 007F | 0xxxxxxx
 * 0000 0080-0000 07FF | 110xxxxx 10xxxxxx
 * 0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
 * 0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 */

unsigned
sxe_unicode_to_utf8(unsigned unicode, char *utf_out)
{
    if (unicode <= 0x7F) {
        utf_out[0] = (char)unicode;
        return 1;
    }

    if (unicode <= 0x7FF) {
        utf_out[0] = (char)(0xC0 | (unicode >> 6));
        utf_out[1] = (char)(0x80 | (unicode & 0x3F));
        return 2;
    }

    if (unicode <= 0xFFFF) {
        utf_out[0] = (char)(0xE0 | (unicode >> 12));
        utf_out[1] = (char)(0x80 | ((unicode >> 6) & 0x3F));
        utf_out[2] = (char)(0x80 | (unicode & 0x3F));
        return 3;
    }

    if (unicode <= 0x10FFFF) {
        utf_out[0] = (char)(0xF0 | (unicode >> 18));
        utf_out[1] = (char)(0x80 | ((unicode >> 12) & 0x3F));
        utf_out[2] = (char)(0x80 | ((unicode >> 6) & 0x3F));
        utf_out[3] = (char)(0x80 | (unicode & 0x3F));
        return 4;
    }

    SXEL2("sxe_unicode_to_utf8: %u is not a valid unicode code point", unicode);
    return 0;
}
