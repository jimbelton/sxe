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

#include <unistd.h>

#include "sxe-spinlock.h"
#include "sxe-thread.h"
#include "tap.h"

SXE_SPINLOCK ping;
SXE_SPINLOCK pong;

static SXE_THREAD_RETURN SXE_STDCALL
test_thread_main(void * lock)
{
    SXEE81("test_thread_main(lock=%p)", lock);

    SXEA10(lock                     == &ping,                     "Ping lock not passed to the thread");
    SXEA10(sxe_spinlock_take(&pong) == SXE_SPINLOCK_STATUS_TAKEN, "Pong lock not taken by thread");
    SXEA10(sxe_spinlock_take(&ping) == SXE_SPINLOCK_STATUS_TAKEN, "Ping lock not taken by thread");
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

    plan_tests(3);

    sxe_spinlock_construct(&ping);
    sxe_spinlock_construct(&pong);
    is(sxe_spinlock_take(&ping), SXE_SPINLOCK_STATUS_TAKEN, "Ping lock taken by main");

    is((result = sxe_thread_create(&thread, test_thread_main, &ping, SXE_THREAD_OPTION_DEFAULTS)), SXE_RETURN_OK,
                                                            "Created SXE thread");

    sxe_spinlock_give(&ping);
    is(sxe_spinlock_take(&pong), SXE_SPINLOCK_STATUS_TAKEN, "Pong lock taken by main");

    return exit_status();
}
