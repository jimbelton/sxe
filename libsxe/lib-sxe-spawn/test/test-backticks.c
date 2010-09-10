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

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ev.h"
#include "sxe.h"
#include "sxe-spawn.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

int
main(int argc, char ** argv)
{
    char buf[1024];
    char cmd[1024];
    char cmd_quote;

    SXE_UNUSED_PARAMETER(argc);
    SXE_UNUSED_PARAMETER(argv);

    plan_tests(7);
    is(sxe_spawn_init(), SXE_RETURN_OK, "Initialized: sxe-spawn");

#ifdef _WIN32
    cmd_quote = '\"';
#else
    cmd_quote = '\'';
#endif

    snprintf(cmd, sizeof(cmd),
             "perl -e %c print qq#foo# . chr(10) . qq#bar# . chr(10) . qq#baz# %c", cmd_quote, cmd_quote);
    is(sxe_spawn_backticks(cmd, buf, sizeof(buf)), 11, "Ran a perl command");
    is(strcmp("foo\nbar\nbaz", buf), 0, "The strings match");

    /* Note: sxe_spawn_backticks asserts if buffer is less then 2 */

    buf[0] = 'X'; buf[1] = 'X'; buf[2] = 'X';
    /* buffer is 2 */
    is(sxe_spawn_backticks(cmd, buf, 2), 1, "Ran another perl command");
    is(buf[0],  'f',  "length of 2 buffer[0]");
    is(buf[1], '\0',      "length of 2 buffer[1]");
    is(buf[2], 'X',       "length of 2 buffer[2]");

    return exit_status();
}
