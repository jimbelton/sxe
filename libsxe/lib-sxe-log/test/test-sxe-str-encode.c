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

#include "sxe-log.h"
#include "tap.h"

int
main(void)
{
    char buffer[80];

    plan_tests(8);
    is_eq(sxe_strn_encode(buffer, 8, "",                  0),   "",   "Returns an empty encoded string on empty input string");
    ok(sxe_strn_encode(buffer, 8, "Too long for buffer", 32) == NULL, "Correctly returns NULL if string too long");
    is_eq(buffer, "Too_lon",                                          "Correctly truncates string if too long");
    ok(sxe_strn_encode(buffer, 8, "Too long for buffer", 8)  == NULL, "String limit (strNishness) respected");
    is(sxe_strn_encode(buffer, sizeof(buffer), "blank tab\tdel\x7Funderscore_equals=0xABCD", 64), buffer,
                                                                      "Correctly returns buffer if string not too long");
    is_eq(buffer, "blank_tab=09del=7Funderscore=5Fequals=3D0xABCD",   "Correctly encodes all special cases");
    ok(sxe_strn_encode(buffer, 8, "\t\x7f_=", 32) == NULL,            "Correctly returns NULL if encoded string too long");
    is_eq(buffer, "=09=7F",                                           "Correctly truncates encoded string at end of a character");
}
