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
#include <string.h>
#include "sxe-log.h"
#include "sxe-util.h"

#define NA 0xff

static unsigned char hex_character_to_nibble[] = {
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  NA, NA, NA, NA, NA, NA,
    NA, 10, 11, 12, 13, 14, 15, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, 10, 11, 12, 13, 14, 15, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA,
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA
};

static char hex_nibble_to_character[] = "0123456789abcdef";

unsigned
sxe_hex_to_unsigned(const char * hex, unsigned hex_length_maximum)
{
    unsigned result = 0;
    unsigned i;

    SXEE83("(hex='%.*s', hex_length_maximum=%u)", hex_length_maximum, hex, hex_length_maximum);

    for (i = 0; (i < hex_length_maximum) && (hex[i] != '\0'); i++) {
        if (hex_character_to_nibble[(unsigned)(hex[i])] == NA) {
            SXEL61("Encountered unexpected hex character '%c'", hex[i]);
            result = SXE_UNSIGNED_MAXIMUM;
            goto SXE_EARLY_OUT;
        }

        result = (result << 4) | hex_character_to_nibble[(unsigned)(hex[i])];
    }

SXE_EARLY_OUT:
    SXER81("return result=0x%x", result);
    return result;
}

SXE_RETURN
sxe_hex_to_bytes(unsigned char * bytes, const char * hex, unsigned hex_length)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    unsigned   i;
    char       character;
    unsigned   nibble_high;
    unsigned   nibble_low;

    SXEA11((hex_length % 1) == 0, "sxe_hex_to_bytes: hex string length %u is odd", hex_length);
    SXEE84("(bytes=%p,hex='%.*s',hex_length=%u)", bytes, hex_length, hex, hex_length);

    for (i = 0; i < hex_length; i++) {
        if (((nibble_high = hex_character_to_nibble[(unsigned)(character = hex[  i])]) == NA)
         || ((nibble_low  = hex_character_to_nibble[(unsigned)(character = hex[++i])]) == NA))
        {
            SXEL32(isprint(character) ? "%s%c'" : "%s\\x%02x'", "sxe_hex_to_bytes: Unexpected hex character '", hex[i]);
            goto SXE_EARLY_OUT;
        }

        bytes[(i - 1) / 2] = (nibble_high << 4) | nibble_low;
    }

    result = SXE_RETURN_OK;

SXE_EARLY_OUT:
    SXER81("return result=%s", sxe_return_to_string(result));
    return result;
}

char *
sxe_hex_from_bytes(char * hex, const unsigned char * bytes, unsigned size)
{
    unsigned i;

    for (i = 0; i < size; i++) {
        hex[2 * i + 0] = hex_nibble_to_character[bytes[i] >> 4];
        hex[2 * i + 1] = hex_nibble_to_character[bytes[i] & 0xF];
    }

    return hex;
}
