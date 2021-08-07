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

#include "sxe-log.h"
#include "tap.h"

int
main(void)
{
    char buffer[16];

    tap_plan(8, 0, NULL);

    tap_test_case_name("sxe_strlcpy");
    is(sxe_strlcpy(buffer, "Hello, world", sizeof(buffer)),         sizeof("Hello, world") - 1,          "Length of 'Hello, world' returned");
    is(strlen(buffer),                                              sizeof("Hello, world") - 1,          "Length of buffer correct");
    is(sxe_strlcpy(buffer, "Goodbye, cruel world", sizeof(buffer)), sizeof("Goodbye, cruel world") - 1,  "Length of 'Goodbye, cruel world' returned");
    is(strlen(buffer),                                              sizeof(buffer) - 1,                  "Length of buffer shows truncation occurred");

    tap_test_case_name("sxe_strlcat");
    sxe_strlcpy(buffer, "Hello, ", sizeof(buffer));
    is(sxe_strlcat(buffer, "world", sizeof(buffer)),                sizeof("Hello, world") - 1,          "Length of 'Hello, world' returned");
    is(strlen(buffer),                                              sizeof("Hello, world") - 1,          "Length of buffer correct");
    is(sxe_strlcat(buffer, ". Goodbye", sizeof(buffer)),            sizeof("Hello, world. Goodbye") - 1, "Length of 'Hello, world. Goodbye' returned");
    is(strlen(buffer),                                              sizeof(buffer) - 1,                  "Length of buffer shows truncation occurred");

    return exit_status();
}
