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

#include "ev.h"
#include "sxe-log.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include <stdlib.h>
#include <string.h>

static void
test_event_timeout(struct ev_loop * loop, ev_timer * timer, int events)
{
    SXEE6("test_event_timeout(loop=%p, timer=%p, events=%d) // should never happen in a correctly running test", loop, timer, events);
    SXEL2("test_event_timeout: Test timer expired while waiting for an event");
    SXE_UNUSED_PARAMETER(loop);
    SXE_UNUSED_PARAMETER(timer);
    SXE_UNUSED_PARAMETER(events);

    SXER6("return");
}

void
test_ev_loop_wait(ev_tstamp seconds)
{
    ev_timer timeout_watcher;

    //special case: reduce log noise: SXEE6("test_ev_loop_wait(seconds=%f)", seconds);
    SXEL6("test_ev_loop_wait(%fs){}", seconds);
    ev_timer_init(&timeout_watcher, test_event_timeout, seconds, 0.);
    ev_timer_start(ev_default_loop(EVFLAG_AUTO), &timeout_watcher);
    ev_loop       (ev_default_loop(EVFLAG_AUTO), EVLOOP_ONESHOT  );
    ev_timer_stop (ev_default_loop(EVFLAG_AUTO), &timeout_watcher);
    //special case: reduce log noise: SXER6("return");
}

void
test_process_all_libev_events(void)
{
    SXEE6("test_process_all_libev_events()");

    /* TODO: Once we've upgraded libev, there is a better way to do this */

    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    SXER6("return");
}

unsigned
test_tap_ev_length_nowait(void)
{
    test_process_all_libev_events();
    return tap_ev_length();
}
