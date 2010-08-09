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
#include "tap.h"

int
main(void)
{
    char     string[] = "0.AMNZ-amnz/";
    char     buffer[] = "XXXXXXXXXXXXX";
    unsigned i;

    plan_tests(11);
    is_eq(sxe_strn_rot13_in_place(string, strlen(string)), "0.NZAM-nzam/",       "ROT13 of '0.AMNZ-amnz/'    is '0.NZAM-nzam/'" );
    is_eq(sxe_strn_rot13_in_place(string, 6),              "0.AMNZ-nzam/",       "ROT13 of '0.NZAM-nzam/:6   is '0.AMNZ-nzam/'" );
    is_eq(sxe_strn_rot13(buffer, string, strlen(string)),  "0.NZAM-amnz/X",      "ROT13 of '0.AMNZ-nzam/':12 is '0.NZAM-amnz/X'");
    is_eq(sxe_strn_rot13(buffer, string, 32),              "0.NZAM-amnz/",       "ROT13 of '0.AMNZ-nzam/':32 is '0.NZAM-amnz/\\0'");
    is(sxe_strn_rot13_in_place(string, strlen(string)),    string,               "sxe_strn_rot13_in_place    is in place"      );
    is(sxe_strn_rot13(buffer, string, strlen(string)),     buffer,               "sxe_strn_rot13             is a copy"        );
    is(sxe_rot13_hex_to_unsigned("0",     2),              0,                    "'0':2    -> 0");
    is(sxe_rot13_hex_to_unsigned("snpr",  4),              0xface,               "'snpr':4 -> 0xface");
    is(sxe_rot13_hex_to_unsigned("O00O",  2),              0xb0,                 "'O00O':2 -> 0xb0");
    is(sxe_rot13_hex_to_unsigned("XXXX",  4),              SXE_UNSIGNED_MAXIMUM, "'XXXX':4 -> 0x%x (SXE_UNSIGNED_MAXIMUM)",
       sxe_rot13_hex_to_unsigned("XXXX",  4));

    for (i = 0; i < 256; i++) {
        if ((('a' <= i) && (i <= 'm')) || (('A' <= i) && (i <= 'M'))) {
            if (SXE_ROT13_CHAR(i) != i + 13) {
                break;
            }
        }
        else if ((('n' <= i) && (i <= 'z')) || (('N' <= i) && (i <= 'Z'))) {
            if (SXE_ROT13_CHAR(i) != i - 13) {
                break;
            }
        }
        else if (SXE_ROT13_CHAR(i) != i) {
            break;
        }
    }

    is(i, 256, "All 256 characters are rot13 encoded correctly");
    return exit_status();
}
