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
#include "sxe-log.h"
#include "tap.h"

#define SHA1_HEX "2ce679528627da7780f8a4fec07cb34f902468a0"

typedef struct TEST_SHA1_STRUCT {
    unsigned word[5];
} TEST_SHA1;

static unsigned char sxe_sha1_expected_bytes[] = {0x2c, 0xe6, 0x79, 0x52, 0x86, 0x27, 0xda, 0x77, 0x80, 0xf8,
                                                  0xa4, 0xfe, 0xc0, 0x7c, 0xb3, 0x4f, 0x90, 0x24, 0x68, 0xa0};

int
main(void)
{
    TEST_SHA1 sha1_expected;
    TEST_SHA1 sha1_got;
    char      hex_buffer[sizeof(SHA1_HEX)];

    plan_tests(10);
    is(sxe_hex_to_unsigned("0",     2), 0,                            "'0':2    -> 0");
    is(sxe_hex_to_unsigned("face",  4), 0xface,                       "'face':4 -> 0xface");
    is(sxe_hex_to_unsigned("B00B",  2), 0xb0,                         "'B00B':2 -> 0xb0");
    is(sxe_hex_to_unsigned("XXXX",  4), SXE_UNSIGNED_MAXIMUM,         "'XXXX':4 -> 0x%x (SXE_UNSIGNED_MAXIMUM)",
       sxe_hex_to_unsigned("XXXX",  4));

    ok(sxe_hex_to_bytes((unsigned char *)&sha1_got, "goofy goober", 12) != SXE_RETURN_OK, "Conversion from hex 'goofy goober' to bytes failed");
    is(sxe_hex_to_bytes((unsigned char *)&sha1_got, SHA1_HEX,       40),   SXE_RETURN_OK, "Conversion from hex '%s' to bytes succeeded", SHA1_HEX);

    memcpy(&sha1_expected, sxe_sha1_expected_bytes, sizeof(sha1_expected));

    if (memcmp(&sha1_got, &sha1_expected, sizeof(TEST_SHA1)) == 0) {
        pass(                                                         "bytes are as expected");
    }
    else {
        SXEL10("Expected:");
        SXED10(&sha1_expected, sizeof(sha1_expected));
        SXEL10("Got:");
        SXED10(&sha1_got,      sizeof(sha1_got));
        fail(                                                         "bytes are not as expected");
    }

    tap_test_case_name("sxe_hex_from_bytes");
    hex_buffer[sizeof(hex_buffer) - 1] = 0xBE;
    is(sxe_hex_from_bytes(hex_buffer, sxe_sha1_expected_bytes, sizeof(sxe_sha1_expected_bytes)), hex_buffer, "Returns hex buffer");
    is_strncmp(hex_buffer, SHA1_HEX, SXE_LITERAL_LENGTH(SHA1_HEX),    "SHA1 converted to hex as expected");
    is((unsigned char)hex_buffer[sizeof(hex_buffer) - 1], 0xBE,       "Guard byte is intact");
    return exit_status();
}
