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

#include <assert.h>
#include <string.h>

#include "sxe-log.h"
#include "tap.h"
#include "sxe-expose.h"

static unsigned global_unsigned_1 = 0;
static unsigned global_unsigned_2 = 0;
static unsigned global_unsigned_3 = 0;

static void
test_basic_set_and_get(void)
{
    char buf[1024];

    is(global_unsigned_1, 0, "global_unsigned_1 is zero");
    is(global_unsigned_2, 0, "global_unsigned_2 is zero");
    is(global_unsigned_3, 0, "global_unsigned_3 is zero");

    sxe_expose_parse("set global_unsigned_2 1", buf, sizeof(buf));
    is_strstr(buf, "", "Set global_unsigned_2 to 1");
    is(global_unsigned_1, 0, "global_unsigned_1 is zero");
    is(global_unsigned_2, 1, "global_unsigned_2 is now 1");
    is(global_unsigned_3, 0, "global_unsigned_3 is zero");

    sxe_expose_parse("set global_unsigned_1 3", buf, sizeof(buf));
    is_strstr(buf, "", "Set global_unsigned_1 to 3");
    is(global_unsigned_1, 3, "global_unsigned_1 is now 3");
    is(global_unsigned_2, 1, "global_unsigned_2 is 1");
    is(global_unsigned_3, 0, "global_unsigned_3 is zero");

    sxe_expose_parse("get global_unsigned_2", buf, sizeof(buf));
    is_strstr(buf, "1", "get global_unsigned_2 returns 1");
    is(global_unsigned_1, 3, "global_unsigned_1 is 3");
    is(global_unsigned_2, 1, "global_unsigned_2 is 1");
    is(global_unsigned_3, 0, "global_unsigned_3 is zero");

    return;
}

static void
test_parse(const char * cmd, const char * res)
{
    char buf[1024];
    sxe_expose_parse(cmd, buf, sizeof(buf));
    is_strstr(buf, res, "test_parse() - expected result");
}

static void
test_set_get_error_cases(void)
{
    test_parse("awsome global_unsigned_1", "Unknown action");
    test_parse("set awesome 3",            "Variable not exposed");
    test_parse("set global_unsigned_1",    "Missing parameter for set");
    test_parse("get global",               "Variable not exposed");

    is(global_unsigned_1, 0, "global_unsigned_1 is zero");
    is(global_unsigned_2, 0, "global_unsigned_2 is zero");
    is(global_unsigned_3, 0, "global_unsigned_3 is zero");
}

int
main(void)
{
    plan_tests(22);

    global_unsigned_3 = 0; // So the compiler doesn't complain that it's unused...

    SXE_EXPOSE_REG(global_unsigned_1, UINT32, "R" );
    SXE_EXPOSE_REG(global_unsigned_2, UINT32, "RW");

    test_set_get_error_cases();
    test_basic_set_and_get();

    return exit_status();
}

