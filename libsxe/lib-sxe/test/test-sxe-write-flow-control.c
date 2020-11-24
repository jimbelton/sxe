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
#include "sxe-socket.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_WAIT     2
#define SXE_MTU_SIZE  1500
#define MTUS_TO_BLOCK 100000    // Number of MTUs to send to insure TCP connection blocks

SXE *        server_side_connected_sxe;
tap_ev_queue tap_q_client;
tap_ev_queue tap_q_server;
unsigned     do_writes_inside_writable_callback = 0;
char         buf[SXE_MTU_SIZE]; // dumby data buffer

static void
test_event_connected_server(SXE * this)
{
    SXEE60I("test_event_connected()");
    tap_ev_queue_push(tap_q_server, __func__, 1, "this", this);
    server_side_connected_sxe = this;
    SXER60I("return");
}

static void
test_event_read_client(SXE * this, int length)
{
    SXE_UNUSED_PARAMETER(this);
    SXEE61I("test_event_read_client(length=%d)", length);
    tap_ev_queue_push(tap_q_client, __func__, 1, "length", length);

    // Note: We are intentionally not clearing this buffer...
    //SXE_BUF_CLEAR(this);
    SXER60I("return");
}

static void
test_event_read_server(SXE * this, int length)
{
    SXEE61I("test_event_read(length=%d)", length);
    tap_ev_queue_push(tap_q_server, __func__, 1, "length", length);
    SXE_BUF_CLEAR(this);
    SXER60I("return");
}

static void
test_event_close_server(SXE * this)
{
    SXEE60I("test_event_close_server()");
    tap_ev_queue_push(tap_q_server, __func__, 1, "this", this);
    SXER60I("return");
}

static void
test_cb_server_sxe_writable(SXE * this, SXE_RETURN sxe_return)
{
    unsigned    x;
    SXE_RETURN  result;

    SXEE61I("test_cb_server_sxe_writable(sxe_return=%s)", sxe_return_to_string(sxe_return));
    tap_ev_queue_push(tap_q_server, __func__, 2, "this", this, "sxe_return", sxe_return);

    if (do_writes_inside_writable_callback) {
        SXEL60("Writing inside writable callback");
        do_writes_inside_writable_callback = 0;

        for(x = 0; x < MTUS_TO_BLOCK; x++) {
            result = sxe_write(server_side_connected_sxe, buf, sizeof(buf));
            if (result != SXE_RETURN_OK) {
                is(result, SXE_RETURN_WARN_WOULD_BLOCK, "Write '%u' failed because the connection backed up", x);
                break;
            }
        }
        // Bomb out if we never made the write fail...
        if (result == SXE_RETURN_OK) { fail("Write never failed..."); exit(-1); }

        sxe_notify_writable(server_side_connected_sxe, test_cb_server_sxe_writable);
    }

    SXER60I("return");
}

int
main(void)
{
    SXE       * client;
    SXE       * server;
    tap_ev      ev;
    unsigned    x;
    SXE_RETURN  result;

    plan_tests(29);
    sxe_register(3, 0);
    tap_q_client = tap_ev_queue_new();
    tap_q_server = tap_ev_queue_new();
    is(sxe_init(), SXE_RETURN_OK, "SXE initialized");

    // Server - Writer
    ok((server = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_connected_server, test_event_read_server, test_event_close_server)) != NULL,
                             "Got new TCP SXE for server");
    is(sxe_listen(server), SXE_RETURN_OK, "SXE listening on server");

    // Client - Reader
    ok((client = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_read_client, NULL)) != NULL,
                             "Got new TCP SXE for client");
    is(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(server)), SXE_RETURN_OK, "SXE connecting from client");

    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_event_connected_server", "Server connected");

    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending events at this point");

    for(x = 0; x < MTUS_TO_BLOCK; x++) {
        result = sxe_write(server_side_connected_sxe, buf, sizeof(buf));
        if (result != SXE_RETURN_OK) {
            is(result, SXE_RETURN_WARN_WOULD_BLOCK, "Write '%u' failed because the connection backed up", x);
            break;
        }
    }

    // Bomb out if we never made the write fail...
    if (result == SXE_RETURN_OK) { fail("Write never failed..."); exit(-1); }

    test_process_all_libev_events();
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events after the first write fails");

    sxe_notify_writable(server_side_connected_sxe, test_cb_server_sxe_writable);
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events after we 'notify_writeable(server)'");

    // Shift off all the client read events
    while (1) {
        const char * last;
        sxe_buf_clear(client);
        last = test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev);
        if (strcmp(last, "test_event_read_client") != 0) {
            break;
        }
    }

    // Server becomes writable again
    test_process_all_libev_events();
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_cb_server_sxe_writable",
          "Got a read event cause by the client closing");


    // Server becomes backed up again...
    for(x = 0; x < MTUS_TO_BLOCK; x++) {
        result = sxe_write(server_side_connected_sxe, buf, sizeof(buf));
        if (result != SXE_RETURN_OK) {
            is(result, SXE_RETURN_WARN_WOULD_BLOCK, "Write '%u' failed because the connection backed up", x);
            break;
        }
    }

    // Bomb out if we never made the write fail...
    if (result == SXE_RETURN_OK) { fail("Write never failed..."); exit(-1); }

    test_process_all_libev_events();
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events after the first write fails");

    sxe_notify_writable(server_side_connected_sxe, test_cb_server_sxe_writable);
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events after we 'notify_writeable(server)'");

    sxe_close(client);

    test_process_all_libev_events();
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_cb_server_sxe_writable",
          "Got a read event cause by the client closing");

    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_event_close_server",
          "Got a read event cause by the client closing");

    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events at this point");

    ///////////////////////
    // Setting up the writable callback inside the writable callback

    // Server - writer still running from before...

    // Client - Reader
    ok((client = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_read_client, NULL)) != NULL,
                             "Got new TCP SXE for client");
    is(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(server)), SXE_RETURN_OK, "SXE connecting from client");

    test_process_all_libev_events();
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_event_connected_server", "Server connected");

    // Might be some read events left over in the queue...
    if (tap_ev_queue_length(tap_q_client) != 0) {
        while (1) {
            const char * last;
            sxe_buf_clear(client);
            last = test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev);
            if (strcmp(last, "test_event_read_client") != 0) {
                break;
            }
        }
    }

    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending client events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events at this point");

    for(x = 0; x < MTUS_TO_BLOCK; x++) {
        result = sxe_write(server_side_connected_sxe, buf, sizeof(buf));
        if (result != SXE_RETURN_OK) {
            is(result, SXE_RETURN_WARN_WOULD_BLOCK, "Write '%u' failed because the connection backed up", x);
            break;
        }
    }

    // Bomb out if we never made the write fail...
    if (result == SXE_RETURN_OK) { fail("Write never failed..."); exit(-1); }

    test_process_all_libev_events();
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events after the first write fails");

    do_writes_inside_writable_callback = 1;
    sxe_notify_writable(server_side_connected_sxe, test_cb_server_sxe_writable);
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events after we 'notify_writeable(server)'");

    // Shift off all the client read events
    while (1) {
        const char * last;
        sxe_buf_clear(client);
        last = test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev);
        if (strcmp(last, "test_event_read_client") != 0) {
            break;
        }
    }

    test_process_all_libev_events();
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_cb_server_sxe_writable",
          "The first writable call back was called");

    // Shift off all the client read events
    while (1) {
        const char * last;
        sxe_buf_clear(client);
        last = test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev);
        if (strcmp(last, "test_event_read_client") != 0) {
            break;
        }
    }

    test_process_all_libev_events();
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_cb_server_sxe_writable",
          "The second writable callback was called");

    return exit_status();
}
