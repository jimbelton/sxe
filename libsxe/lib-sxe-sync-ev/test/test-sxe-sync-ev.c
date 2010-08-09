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

#include "sxe-socket.h"
#include "sxe-sync-ev.h"
#include "sxe-test.h"
#include "tap.h"

static void
test_event(void * sync_point, void * user_data)
{
    SXEE82("test_event(sync_point=%p, user_data=%p)", sync_point, user_data);
    tap_ev_push("test_event", 2, "sync_point", sync_point, "user_data", user_data);
    SXER80("return");
}

int
main(void)
{
    void * sync_point;
    tap_ev event;

    plan_tests(1);
    sxe_socket_init();
    sxe_sync_ev_init(1, test_event);
    sync_point = sxe_sync_ev_new((void *)0xB16B00B5);
    sxe_sync_ev_post(sync_point);
    event = test_tap_ev_shift_wait(5.0);
    is_eq(tap_ev_identifier(event), "test_event", "Got the expected event");
    sxe_sync_ev_delete(sync_point);
    return exit_status();
}
