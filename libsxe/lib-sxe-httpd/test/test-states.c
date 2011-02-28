/* Copyright 2010 Sophos Limited. All rights reserved. Sophos is a registered
 * trademark of Sophos Limited.
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

#include "tap.h"
#include "sxe-util.h"
#include "sxe-httpd.h"

int
main(void)
{
    SXE_HTTPD_CONN_STATE state;
    const char         * string;

    plan_tests(1 + SXE_HTTPD_CONN_NUMBER_OF_STATES);

    for (state = SXE_HTTPD_CONN_FREE; state < SXE_HTTPD_CONN_NUMBER_OF_STATES; state++) {
        ok((string = sxe_httpd_state_to_string(state)) != NULL, "SXE HTTPD state %u == %s", state, string);
    }

    ok(sxe_httpd_state_to_string(state) == NULL, "SXE HTTPD state %u == NULL", state);

    return exit_status();
}
