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

#include "sha1.h"
#include "sxe-log.h"
#include "tap.h"

#define SHA1_HEX "2ce679528627da7780f8a4fec07cb34f902468a0"

static unsigned char sxe_sha1_expected_bytes[] = {0x2c, 0xe6, 0x79, 0x52, 0x86, 0x27, 0xda, 0x77, 0x80, 0xf8,
                                                  0xa4, 0xfe, 0xc0, 0x7c, 0xb3, 0x4f, 0x90, 0x24, 0x68, 0xa0};

int
main(void)
{
    SOPHOS_SHA1  sha1_expected;
    SOPHOS_SHA1  sha1_got;
    char         sha1_in_hex[41];

    plan_tests(5);

    /* from hex to bytes */
    ok(sha1_from_hex(&sha1_got, "goofy goober") != SXE_RETURN_OK, "Conversion from hex 'goofy goober' to SHA1 failed");
    is(sha1_from_hex(&sha1_got, SHA1_HEX),         SXE_RETURN_OK, "Conversion from hex '%s' to SHA1 succeeded", SHA1_HEX);

    memcpy(&sha1_expected, sxe_sha1_expected_bytes, sizeof(sha1_expected));

    if (memcmp(&sha1_got, &sha1_expected, sizeof(SOPHOS_SHA1)) == 0) {
        pass("SHA1 is as expected");
    }
    else {
        SXEL1("Expected:");
        SXED1(&sha1_expected, sizeof(sha1_expected));
        SXEL1("Got:");
        SXED1(&sha1_got,      sizeof(sha1_got));
        fail("SHA1 is not as expected");
    }

    /* from bytes to hex */
    ok(sha1_to_hex(&sha1_got, sha1_in_hex, sizeof(sha1_in_hex)) == SXE_RETURN_OK, "sha1_to_hex ran successfully");
    ok(memcmp(sha1_in_hex, SHA1_HEX, sizeof(SHA1_HEX)) == 0, "sha1_to_hex is accurate");

    return exit_status();
}
