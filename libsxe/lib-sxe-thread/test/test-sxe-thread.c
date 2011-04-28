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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if SXE_DEBUG != 0
#define LOCAL_SXE_DEBUG 1
#endif

#undef  SXE_DEBUG      /* Since we are testing diagnostic functions, the test program forces debug mode */
#define SXE_DEBUG 1

#include "sxe-spinlock.h"
#include "sxe-thread.h"
#include "tap.h"

#define TEST_YIELD_MAX 1000000

//#ifndef LOCAL_SXE_DEBUG
//__thread unsigned        sxe_log_indent_maximum = ~0U;
//#endif

SXE_LOG_LEVEL     test_log_level;
SXE_SPINLOCK      ping;
SXE_SPINLOCK      pong;
volatile unsigned thread_indent = 0;
volatile unsigned main_indent   = 0;

static void
test_log_line(SXE_LOG_LEVEL level, const char * line)
{
    char * tag;

    if      ((tag = strstr(line, "thread:")) != NULL) {
        thread_indent = tag - line;
    }
    else if ((tag = strstr(line, "main:"  )) != NULL) {
        main_indent   = tag - line;
    }

    /* Stuff unwanted diagnostics
     */
    if (level >= test_log_level) {
        fputs(line, stderr);
        fflush(stderr);
    }
}

static SXE_THREAD_RETURN SXE_STDCALL
test_thread_main(void * lock)
{
    SXEE81("test_thread_main(lock=%p)", lock);

    SXEA10(lock                     == &ping,                     "Ping lock not passed to the thread");
    SXEA10(sxe_spinlock_take(&pong) == SXE_SPINLOCK_STATUS_TAKEN, "Pong lock not taken by thread");
    SXEA10(sxe_spinlock_take(&ping) == SXE_SPINLOCK_STATUS_TAKEN, "Ping lock not taken by thread");
    SXEL10("thread: about to pong the main thread");
    sxe_spinlock_give(&pong);

    for (;;) {
        sleep(1);
    }

    SXER80("return NULL");
    return (SXE_THREAD_RETURN)0;
}

int
main(void)
{
    SXE_THREAD thread;
    SXE_RETURN result;
    unsigned   i;

    plan_tests(6);
    sxe_log_hook_line_out(test_log_line);
    test_log_level = sxe_log_set_level(SXE_LOG_LEVEL_LIBRARY_TRACE);    /* Required to do indentation test */
    sxe_spinlock_construct(&ping);
    sxe_spinlock_construct(&pong);
    is(sxe_spinlock_take(&ping), SXE_SPINLOCK_STATUS_TAKEN, "Ping lock taken by main");

    is((result = sxe_thread_create(&thread, test_thread_main, &ping, SXE_THREAD_OPTION_DEFAULTS)), SXE_RETURN_OK,
                                                            "Created SXE thread");
    sxe_spinlock_give(&ping);    /* Allow thread to proceed */

    /* Wait for thread_indent to be set.
     */
    for (i = 0; thread_indent == 0 && i < TEST_YIELD_MAX; i++) {
        SXE_YIELD();
    }

    ok(i < TEST_YIELD_MAX,                                  "Thread log indent set to %u after %u yeilds", thread_indent, i);
    SXEL10("main: about to confirm the pong from the thread");
    is(sxe_spinlock_take(&pong), SXE_SPINLOCK_STATUS_TAKEN, "Pong lock taken by main");
    ok(main_indent > 0,                                     "Main log indent set to %u", main_indent);
    ok(thread_indent > main_indent,                         "Thread indent is greater than main indent");
    return exit_status();
}
