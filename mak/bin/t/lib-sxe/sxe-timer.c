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
#include "sxe.h"
#include "sxe-log.h"

extern struct ev_loop * sxe_private_main_loop;

void
sxe_timer_init(ev_timer* timer, void(*cb)(EV_P_ ev_timer *w, int revents), double after, double repeat)
{
    SXEE84("sxe_timer_init(ev_timer=%p, cb=%p, after=%f, repeat=%p)", timer, cb, after, repeat);
    ev_timer_init(timer, cb, after, repeat);
    SXER80("return");
}

void sxe_timer_start(ev_timer* timer)
{
    SXEE81("sxe_timer_start(ev_timer=%p)", timer);
    ev_timer_start(sxe_private_main_loop, timer);
    SXER80("return");
}

void sxe_timer_stop(ev_timer* timer)
{
    SXEE81("sxe_timer_stop(ev_timer=%p)", timer);
    ev_timer_stop(sxe_private_main_loop, timer);
    SXER80("return");
}

void sxe_timer_set(ev_timer* timer, double after, double repeat)
{
    SXEE83("sxe_timer_set(ev_timer=%p, after=%f, repeat=%p)", timer, after, repeat);
    ev_timer_set(timer, after, repeat);
    SXER80("return");
}

void sxe_timer_again(ev_timer* timer)
{
    SXEE81("sxe_timer_again(ev_timer=%p)", timer);
    ev_timer_again(sxe_private_main_loop, timer);
    SXER80("return");
}
