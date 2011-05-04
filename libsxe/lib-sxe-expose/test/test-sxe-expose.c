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

static unsigned g_unsigned1 = 0;
static unsigned g_unsigned2 = 0;
static unsigned g_unsigned3 = 0;

static int g_int1             = 0;
static int g_int2             = 0;
static int g_int3             = 0;
static int g_int_not_writable = 0;
static int g_int_not_readable = 0;

#define G_CHAR_ARY1_LEN 16
#define G_CHAR_ARY2_LEN 25
#define G_CHAR_ARY3_LEN 15

static char g_char_ary1[G_CHAR_ARY1_LEN] = "g_char_ary1_str";
static char g_char_ary2[G_CHAR_ARY2_LEN] = "g_char_ary2_str";
static char g_char_ary3[G_CHAR_ARY3_LEN] = "g_char_ary3_str"; /* compiler skips the trailing null */

static void
test_basic_get_and_set(void)
{
    char buf[1024];

    /* unsigned */
    is(g_unsigned1, 0, "g_unsigned1 is zero");
    is(g_unsigned2, 0, "g_unsigned2 is zero");
    is(g_unsigned3, 0, "g_unsigned3 is zero");

    sxe_expose_parse("g_unsigned2=7", buf, sizeof(buf));
    is_strstr(buf, "g_unsigned2=7\n", "Set g_unsigned2 to 7");
    is(g_unsigned1, 0, "g_unsigned1 is zero");
    is(g_unsigned2, 7, "g_unsigned2 is now 7");
    is(g_unsigned3, 0, "g_unsigned3 is zero");

    sxe_expose_parse("g_unsigned1=3", buf, sizeof(buf));
    is_strstr(buf, "g_unsigned1=3\n", "Set g_unsigned1 to 3");
    is(g_unsigned1, 3, "g_unsigned1 is now 3");
    is(g_unsigned2, 7, "g_unsigned2 is 7");
    is(g_unsigned3, 0, "g_unsigned3 is zero");

    sxe_expose_parse("g_unsigned2", buf, sizeof(buf));
    is_strstr(buf, "g_unsigned2=7\n", "get g_unsigned2 returns 7");
    is(g_unsigned1, 3, "g_unsigned1 is 3");
    is(g_unsigned2, 7, "g_unsigned2 is 7");
    is(g_unsigned3, 0, "g_unsigned3 is zero");

    /* int */
    sxe_expose_parse("g_int3", buf, sizeof(buf));
    is_strstr(buf, "g_int3=0\n", "get on g_int3 returns 0");

    sxe_expose_parse("g_int3=5", buf, sizeof(buf));
    is_strstr(buf, "g_int3=5\n", "set on g_int3 returns 5");
    is(g_int3, 5, "g_int3 is 5");

    sxe_expose_parse("g_int3", buf, sizeof(buf));
    is_strstr(buf, "g_int3=5\n", "get on g_int3 returns 5");

    /* char_ary */
    sxe_expose_parse("g_char_ary1", buf, sizeof(buf));
    is_strstr(buf, "g_char_ary1=\"g_char_ary1_str\"\n", "get on g_char_ary1 returns \"g_char_ary1_str\"");

    sxe_expose_parse("g_char_ary1=\"awesome\"", buf, sizeof(buf));
    is_strstr(buf, "g_char_ary1=\"awesome\"\n", "set on g_char_ary1 returns \"awesome\"");
    is(g_char_ary1[0], 'a', "g_char_ary1[0] is 'a'");

    sxe_expose_parse("g_char_ary1", buf, sizeof(buf));
    is_strstr(buf, "g_char_ary1=\"awesome\"\n", "set on g_char_ary1 returns \"awesome\"");

    return;
}

static void
test_parse(const char * cmd, const char * res)
{
    char buf[1024];
    sxe_expose_parse(cmd, buf, sizeof(buf));
    is_strstr(buf, res, "test_parse() - expected result");
    is(strlen(buf), strlen(res), "test_parse() - expected result length");
}

static void
test_set_get_error_cases(void)
{
    test_parse("",      "No command\n");
    test_parse(" ",     "No command\n");
    test_parse(" \t",   "No command\n");
    test_parse("1",     "Bad command: 1\n");
    test_parse("123 ",  "Bad command: 123 \n");
    test_parse("123",   "Bad command: 123\n");
    test_parse(" 123 ", "Bad command: 123 \n");
    test_parse("=",     "Bad command: =\n");
    test_parse(" =",    "Bad command: =\n");
    test_parse("= ",    "Bad command: = \n");
    test_parse("=a",    "Bad command: =a\n");
    test_parse("a=",    "Bad command: a=\n");
    test_parse("a= ",   "Bad command: a= \n");
    test_parse("a- ",   "Bad command: a- \n");
    test_parse("a",     "Variable not exposed: a\n");
    test_parse("g_",    "Variable not exposed: g_\n");

    /* Failures and oddities */
    test_parse("g_unsigned1=-1",                    "g_unsigned1=4294967295\n");
    test_parse("g_int1=2.2",                        "g_int1=2\n");
    test_parse("g_char_ary1=\"12345678901234567\"", "Failed g_char_ary1=\"12345678901234567\", array too short\n");
}

static void
test_permissions(void)
{
    // read/write
    test_parse("g_int1=10",             "g_int1=10\n");

    // read
    test_parse("g_int_not_writable=10", "Variable 'g_int_not_writable' is not writable\n");

    // write
    is(g_int_not_readable, 0, "g_int_not_readable is zero");
    test_parse("g_int_not_readable",    "Variable 'g_int_not_readable' is not readable\n");
    test_parse("g_int_not_readable=10", "g_int_not_readable=10\n");
    is(g_int_not_readable, 10, "g_int_not_readable is now 10");
}

static void
test_dump_command(void)
{
    char buf[1024];

    sxe_expose_parse("DUMP", buf, sizeof(buf));
    is_strstr(buf, "g", "Ran DUMP command");
}

int
main(void)
{
    plan_tests(72);

    // So the compiler doesn't complain that they're unused...
    g_unsigned3    = 0;
    g_int2         = 0;
    g_char_ary2[G_CHAR_ARY2_LEN - 1] = '\0';


    /*             GLOBAL NAME         TYPE      SIZE (if array)  PERM  COMMENT                                                           */
    SXE_EXPOSE_REG(g_unsigned1,        UINT32,   0,               "RW", "The comment for g_unsigned1"                                     );
    SXE_EXPOSE_REG(g_unsigned2,        UINT32,   0,               "RW", ""                                                                );
    SXE_EXPOSE_REG(g_int1,             INT32,    0,               "RW", ""                                                                );
    SXE_EXPOSE_REG(g_int3,             INT32,    0,               "RW", "Another comment"                                                 );
    SXE_EXPOSE_REG(g_char_ary1,        CHAR_ARY, G_CHAR_ARY1_LEN, "RW", "g_char_ary1 comment"                                             );
    SXE_EXPOSE_REG(g_char_ary3,        CHAR_ARY, G_CHAR_ARY3_LEN, "RW", "The sort of very very long long comment that is kind of long-ish");
    SXE_EXPOSE_REG(g_int_not_writable, INT32,    0,               "R",  "This var is not writable"                                        );
    SXE_EXPOSE_REG(g_int_not_readable, INT32,    0,               "W",  "This var is not readable"                                        );

    test_basic_get_and_set();
    test_set_get_error_cases();
    test_permissions();
    test_dump_command();

    return exit_status();
}

