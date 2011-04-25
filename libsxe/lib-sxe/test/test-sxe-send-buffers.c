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
#include <string.h>
#include <stdlib.h>

#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-mmap.h"
#include "sxe-socket.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_WAIT          2
#define TEST_COPIES        10

static void
test_event_connect(SXE * this)
{
    tap_ev_queue q = SXE_USER_DATA(this);
    SXEE60I("test_event_connect()");
    tap_ev_queue_push(q, __func__, 1, "this", this);
    SXER60I("return");
}

static void
test_event_read(SXE * this, int length)
{
    tap_ev_queue q = SXE_USER_DATA(this);
    SXE_UNUSED_PARAMETER(length);
    SXEE61I("test_event_read(length=%d)", length);
    tap_ev_queue_push(q, __func__, 3, "this", this, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)), "used", SXE_BUF_USED(this));
    sxe_buf_clear(this);
    SXER60I("return");
}

static void
test_event_close(SXE * this)
{
    tap_ev_queue q = SXE_USER_DATA(this);
    SXEE60I("test_event_close()");
    tap_ev_queue_push(q, __func__, 1, "this", this);
    SXER60I("return");
}

static void
test_event_sent(SXE * this, SXE_RETURN sxe_return)
{
    tap_ev_queue q = SXE_USER_DATA(this);
    SXEE62I("test_event_sent(sxe_return=%s/%u)", sxe_return_to_string(sxe_return), sxe_return);
    tap_ev_queue_push(q, __func__, 2, "this", this, "sxe_return", sxe_return);
    SXER60I("return");
}

int
main(int argc, char *argv[])
{
    SXE             * client;
    SXE             * listener;
    SXE             * server;
    SXE_RETURN        result;
    SXE_BUFFER        buffers[1024];
    SXE_LIST          buflist;
    tap_ev_queue      q_client;
    tap_ev_queue      q_server;
    tap_ev            ev;
    char              readbuf[1024];

    SXE_UNUSED_PARAMETER(argc);

    sxe_log_level = SXE_LOG_LEVEL_LIBRARY_TRACE;
    plan_tests(7);
    sxe_register(3, 0);

    q_client = tap_ev_queue_new();
    q_server = tap_ev_queue_new();

    sxe_init();

    listener = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_connect, test_event_read, test_event_close);
    client   = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_connect, test_event_read, test_event_close);

    SXE_USER_DATA(listener) = q_server;
    SXE_USER_DATA(client)   = q_client;

    sxe_listen(listener);
    sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener));

    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "test_event_connect", "Client connected");
    is_eq(test_tap_ev_queue_identifier_wait(q_server, TEST_WAIT, &ev), "test_event_connect", "Server connected");
    server = SXE_CAST_NOCONST(SXE *, tap_ev_arg(ev, "this"));

    buffers[0].ptr  = "Hello";
    buffers[0].len  = 5;
    buffers[0].sent = 0;
    buffers[1].ptr  = ", world!";
    buffers[1].len  = 8;
    buffers[1].sent = 0;

    SXE_LIST_CONSTRUCT(&buflist, 0, SXE_BUFFER, node);
    sxe_list_push(&buflist, &buffers[0]);
    sxe_list_push(&buflist, &buffers[1]);

    result = sxe_send_buffers(client, &buflist, test_event_sent);
    if (result == SXE_RETURN_IN_PROGRESS) {
        is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "test_event_sent", "Client send completed");
    }
    else {
        is(result, SXE_RETURN_OK, "sxe_send_buffers sent all data at once");
    }

    memset(readbuf, 0, sizeof readbuf);
    test_ev_queue_wait_read(q_server, TEST_WAIT, &ev, server, "test_event_read", readbuf, 5 + 8, "server");
    is_eq(readbuf, "Hello, world!", "Server got correct contents");

    /* Now send the current binary file TEST_COPIES times. That ought to require
     * multiple attempts! */
    {
        char *tempbuf;
        SXE_MMAP self;
        int i;

        sxe_mmap_open(&self, argv[0]);

        tempbuf = calloc(TEST_COPIES, self.size);
        SXEA11(tempbuf != NULL, "failed to allocate %u bytes", TEST_COPIES * self.size);

        SXE_LIST_CONSTRUCT(&buflist, 0, SXE_BUFFER, node);
        for (i = 0; i < TEST_COPIES; i++) {
            buffers[i].ptr = self.addr;
            buffers[i].len = self.size;
            buffers[i].sent = 0;
            sxe_list_push(&buflist, &buffers[i]);
        }

        result = sxe_send_buffers(client, &buflist, test_event_sent);
        test_ev_queue_wait_read(q_server, TEST_WAIT, &ev, server, "test_event_read", tempbuf, TEST_COPIES * self.size, "server");

        if (result == SXE_RETURN_IN_PROGRESS) {
            is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "test_event_sent", "Client send completed");
        }
        else {
            is(result, SXE_RETURN_OK, "sxe_send_buffers sent all data at once");
        }

        sxe_mmap_close(&self);
    }

    return exit_status();
}

/* vim: set expandtab list: */
