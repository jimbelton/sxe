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
#include <stdlib.h>

#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-socket.h"
#include "sxe-util.h"
#include "tap.h"

static int callback_count = 0;

static void test_cb(EV_P_ ev_timer* timer, int revents)
{
    SXEL12("test_cb(timer=%p, revents=%d)", timer, revents);
    SXE_UNUSED_PARAMETER(loop);
    SXE_UNUSED_PARAMETER(timer);
    SXE_UNUSED_PARAMETER(revents);
    ++callback_count;
    SXEL11("  callback_count=%d", callback_count);
    switch(callback_count)
    {
        case 1:
            SXEL10("  sxe_timer_set");
            sxe_timer_stop(timer);
            sxe_timer_set(timer, 0.0001, 0.);
            sxe_timer_start(timer);
            break;
        case 2:
            SXEL10("  sxe_timer_again");
            sxe_timer_stop(timer);
            sxe_timer_again(timer);
            sxe_timer_start(timer);
            break;
        case 3:
            SXEL10("  unlooping");
            ev_unloop(EV_A, EVUNLOOP_ALL);
            break;
    }
    return;
}

int
main(int argc, char *argv[])
{
    ev_timer timer;

    SXE_UNUSED_PARAMETER(argc);
    SXE_UNUSED_PARAMETER(argv);

    plan_tests(1);

    sxe_register(1, 0); /* first plugin registers */
    sxe_init();

    sxe_timer_init(&timer, test_cb, 0.0001,0.);
    sxe_timer_start(&timer);

    ev_loop(ev_default_loop(EVFLAG_AUTO), 0);

    is(callback_count, 3, "Correct number of callbacks executed");
    sxe_timer_stop(&timer);

    return exit_status();
}

