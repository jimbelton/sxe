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
#include "sxe-test.h"

#include <unistd.h>

#include <setjmp.h> /* for setjmp/longjmp - testing ASSERTs */
#include <signal.h> /* for..  um, signal() */
#include <unistd.h> /* for __func__ on Windows */

static jmp_buf           abort_jmpbuf_env;

static void
test_signal_handler_sigabrt(int signum)
{
    /* SIGNAL HANDLER -- DO NOT USE SXEExx/SXERxx macros */
    if (signum == SIGABRT) {
        longjmp(abort_jmpbuf_env, 1);
        /* does not return */
    }
    return;
}

int
test_expect_abort_in_function(void (*func)(void *), void * user_data)
{
    int     result = 0;
    int     savesigs = 0;
    void*   old_signal_handler = NULL;

    SXEE6("%s(user_data=%p)", __func__, user_data);

    SXE_UNUSED_ARGUMENT(savesigs); /* sigsetjmp may not 'use' this, but its required by the API */

    if (setjmp(abort_jmpbuf_env) == 0) {
        SXEL6("Trap abort() so we can run a test which is expected to call it");
        old_signal_handler = signal(SIGABRT, test_signal_handler_sigabrt);

        func(user_data); /* not expected to return */

        /* If we get here, the code did not abort, so the test failed */
        result = 0;
    }
    else
    {
        /* If we get here, the code aborted, which is what we want - the test passed */
        result = 1; /* success */
    }

    signal(SIGABRT, old_signal_handler);

    SXER6("return result=%d", result);
    return result; /* 0 means failure */
}
