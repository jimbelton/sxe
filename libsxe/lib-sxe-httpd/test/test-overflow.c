/* Copyright 2010 Sophos Limited. All rights reserved. Sophos is a registered
 * trademark of Sophos Limited.
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

#include "tap.h"
#include "sxe-util.h"
#include "sxe-httpd.h"

static int connections;

static void
handle_connect(SXE * this)
{
    ++connections;
    SXEL91I("handle_connect(): connections=%d", connections);
    SXE_WRITE_LITERAL(this, "GET / HTTP/1.1\r\n\r\n");
}

static void
handle_read(SXE * this, int length)
{
    SXE_UNUSED_PARAMETER(length);
    SXE_UNUSED_PARAMETER(this);
    SXEL91I("handle_read(): connections=%d, closing self", connections);
    sxe_close(this);
    --connections;
}

static void
handle_close(SXE * this)
{
    SXE_UNUSED_PARAMETER(this);
    --connections;
    SXEL91I("handle_close(): connections=%d", connections);
}

static void
handle_timer(EV_P_ ev_timer *w, int revents)
{
    SXE_UNUSED_PARAMETER(loop);
    SXE_UNUSED_PARAMETER(revents);

    SXEL91("connections: %d", connections);
    if (connections == 0) {
        sxe_close((SXE*)w->data);
        sxe_timer_stop(w);
    }
}

int
main(void)
{
    SXE_HTTPD httpd;
    SXE     * listener;
    ev_timer  w;
    int       i;

    plan_tests(1);

    sxe_register(1000, 0);
    sxe_init();

    sxe_httpd_construct(&httpd, 2, 10, 512, 0);
    listener = sxe_httpd_listen(&httpd, "0.0.0.0", 0);
    httpd.user_data = listener;

    for (i = 0; i < 100; i++) {
        SXE *c = sxe_new_tcp(NULL, "0.0.0.0", 0, handle_connect, handle_read, handle_close);

        if (c) {
            sxe_connect(c, "127.0.0.1", SXE_LOCAL_PORT(listener));
        }
    }

    sxe_timer_init(&w, handle_timer, 0.1, 0.1);
    w.data = listener;
    sxe_timer_start(&w);

    ev_loop(ev_default_loop(EVFLAG_AUTO), 0);

    ok(1, "Hello");

    return exit_status();
}

/* vim: set expandtab list listchars=tab\:^.,trail\:@ : */
