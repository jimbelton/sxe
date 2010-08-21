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

static char hello_world[]      = "hello, world";
static char very_long_string[] =
"\n12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
"01234567890123456789012345678901234567890123456789012345678901234";

int
main(void)
{
    const char * str;

    plan_tests(5);
    is(sxe_str_to_printable(hello_world), hello_world,           "'hello, world' is already printable");
    is_eq(sxe_str_to_printable("\n\t \x7F"), "\\x0a\\x09 \\x7f", "Printable version of '\\n\\t \\x7F' is '\\x0a\\x09 \\x7f");
    ok((str = sxe_str_to_printable(very_long_string)) != very_long_string, "Very long string is not already printable");
    is(strlen(str), 1023,                                        "Very long printable string truncated at expected length");
    is_eq(&str[1020], "...",                                     "Truncated printable string ends with ...");
}
