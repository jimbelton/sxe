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
#include <sys/socket.h>

#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-socket.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_WAIT          2
#define TEST_HUGE_BUFFER   30000
#define TEST_HUGE_COUNT    10
#define TEST_DATA          "TEST_DATA1234"
#define TEST_DATA_LENGTH   (sizeof(TEST_DATA) - 1)
#define TEST_DATA_3        "1234_TEST_DATA_3"
#define TEST_DATA_3_LENGTH (sizeof(TEST_DATA_3) - 1)
#define TEST_BUF_SIZE      1024       /* This is the minimum buffer size that can be set with setsockopt(SO_RCVBUF/SO_SNDBUF) */

#ifndef __APPLE__
static SXE_BUFFER    test_buffers[TEST_HUGE_COUNT];
static unsigned char test_data[   TEST_HUGE_COUNT][TEST_HUGE_BUFFER];     /* Large amount of data */
#endif

static unsigned char test_buf[    TEST_HUGE_COUNT * TEST_HUGE_BUFFER];
static unsigned      test_buf_length = 0;
static SXE         * test_server_connection;
static tap_ev_queue  tap_q_client;

static tap_ev_queue  tap_q_server;

static void
test_event_connected_server(SXE * this)
{
    SXEE6I("()");
    tap_ev_queue_push(tap_q_server, __func__, 1, "this", this);
    test_server_connection = this;
    SXER6I("return");
}

static void
test_event_read_client(SXE * this, int length)
{
    SXEE6I("(length=%d)", length);

    memcpy(test_buf + test_buf_length, SXE_BUF(this), length);
    test_buf_length += length;
    tap_ev_queue_push(tap_q_client, __func__, 2,  "this", this, "length", (intptr_t)length);

    sxe_buf_clear(this);
    SXER6I("return");
}

static void
test_event_read_server(SXE * this, int length)
{
    SXEE6I("(length=%d)", length);

    memcpy(test_buf + test_buf_length, SXE_BUF(this), length);
    test_buf_length += length;
    tap_ev_queue_push(tap_q_server, __func__, 4, "this", this, "length", (intptr_t)length,
                      "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)), "used", SXE_BUF_USED(this));
    sxe_buf_clear(this);
    SXER6I("return");
}

static void
test_event_send_complete(SXE * this, SXE_RETURN result)
{
    SXEE6I("(result=%s)", sxe_return_to_string(result));
    tap_ev_queue_push(tap_q_client, __func__, 2, "this", this, "result", result);
    SXER6I("return");
}

#define TEST_SEND_WITHIN_CB "send within callback"

#ifndef __APPLE__

static void
test_event_send_complete_so_resend(SXE * this, SXE_RETURN result)
{
    SXEE6I("(result=%s)", sxe_return_to_string(result));
    tap_ev_queue_push(tap_q_client, __func__, 2, "this", this, "result", result);
    SXEA1(sxe_send(this, TEST_SEND_WITHIN_CB, SXE_LITERAL_LENGTH(TEST_SEND_WITHIN_CB), test_event_send_complete) == result,
          "send within completion callback immediately gets same result");
    SXER6I("return");
}

static void
test_event_buffer_consumed(void * user_data, SXE_BUFFER * buffer)
{
    SXE * this = (SXE *)user_data;

    SXEE6I("(buffer=%p)", buffer);
    tap_ev_queue_push(tap_q_client, __func__, 2, "this", this, "buffer", buffer);
    SXER6I("return");
}

static void
test_event_sxe_is_writable(SXE * this, SXE_RETURN result)
{
    SXEE6I("(result=%s)", sxe_return_to_string(result));
    tap_ev_queue_push(tap_q_client, __func__, 2, "this", this, "result", result);
    SXER6I("return");
}
#endif

int
main(void)
{
    SXE      * client;
    SXE      * server;
    tap_ev     ev;
    unsigned   counter;
    SXE_RETURN send_result;
    int        window_size      = TEST_BUF_SIZE;
    SXE_BUFFER buffer;

    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LIST=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_POOL=5");
    sxe_log_set_level(SXE_LOG_LEVEL_TRACE);

    (void)buffer;
    (void)window_size;
    (void)counter;
    (void)send_result;

#if defined(WIN32) || defined(__APPLE__)
    tap_plan(25, TAP_FLAG_ON_FAILURE_EXIT, NULL);
#else
    tap_plan(51, TAP_FLAG_ON_FAILURE_EXIT, NULL);
#endif

    sxe_register(3, 0);
    tap_q_client = tap_ev_queue_new();
    tap_q_server = tap_ev_queue_new();

    is(sxe_init(), SXE_RETURN_OK, "SXE initialized");
    ok((server = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_connected_server, test_event_read_server, NULL)) != NULL,
       "Got new TCP SXE for server");
    is(sxe_listen_plus(server, 0, TEST_BUF_SIZE, TEST_BUF_SIZE), SXE_RETURN_OK, "SXE listening on server");
    ok((client = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_read_client, NULL)) != NULL,
       "Got new TCP SXE for client");
    is(sxe_connect_plus(client, "127.0.0.1", SXE_LOCAL_PORT(server), TEST_BUF_SIZE, TEST_BUF_SIZE), SXE_RETURN_OK,
       "SXE connecting from client");

    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_event_connected_server", "Server connected");
    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending client events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events at this point");

    tap_set_test_case_name("Send less than 1 buffer");
    is(sxe_send(client, TEST_DATA, TEST_DATA_LENGTH, test_event_send_complete), SXE_RETURN_OK,
       "Able to send the whole buffer in one sxe_send() call. No callback needed");
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_event_read_server", "SXE server got read event");
    is(tap_ev_arg(ev, "length"), TEST_DATA_LENGTH, "Server got %u bytes", (unsigned)TEST_DATA_LENGTH);

    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending client events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events at this point");

#ifdef __APPLE__
    skip(6, "Just about impossible to send more than fits in the socket buffer on Mac OS X");
#else
    tap_set_test_case_name("Send more than can fit in the socket buffer");

    /* Prepopulate the data with known values
     */
    for (counter = 0; counter < sizeof(test_data); counter++) {
        ((unsigned char *)test_data)[counter] = counter;
    }

    test_buf_length = 0;

    for (counter = 0; counter < TEST_HUGE_COUNT - 1; counter++) {
        sxe_buffer_construct(&test_buffers[counter], SXE_CAST(char *, &test_data[counter]),sizeof(test_data[counter]),
                             sizeof(test_data[counter]));
        sxe_send_buffer(client, &test_buffers[counter]);
    }

    sxe_buffer_construct_plus(&test_buffers[counter], SXE_CAST(char *, &test_data[counter]), sizeof(test_data[counter]),
                              sizeof(test_data[counter]), client, test_event_buffer_consumed);
    send_result = sxe_send_buffer(client, &test_buffers[counter]);
    ok(send_result == SXE_RETURN_OK || send_result == SXE_RETURN_IN_PROGRESS, "send_result either OK or IN_PROGRESS");

    for (counter = 0; counter < sizeof(test_data); counter += SXE_CAST(unsigned, tap_ev_arg(ev, "length"))) {
        if (strcmp(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_event_read_server") != 0) {
            fail("Unexpected server event %s", (const char *)tap_ev_identifier(ev));
            break;
        }
    }

    is(counter,                            sizeof(test_data), "Received all the data");
    is(tap_ev_queue_length(tap_q_server),  0,                 "There are no pending server events at this point");

    if (send_result == SXE_RETURN_OK) {
        skip(2, "No call back needed: sxe_send() returned immediately");
    }
    else {
        is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_buffer_consumed",
              "Called back after SXE buffer was consumed");
        is(tap_ev_arg(ev, "buffer"), &test_buffers[TEST_HUGE_COUNT - 1], "Called back on the expected buffer");
    }

    is(tap_ev_queue_length(tap_q_client), 0, "There are no pending client events at this point");

    /* Verify the data that was read
     */
    for (counter = 0; counter < sizeof(test_data); counter++) {
        if (test_buf[counter] != (unsigned char)counter) {
            printf("sxe_send() data is corrupted: Got 0x%02x, expected 0x%02x\n", test_buf[counter], (unsigned char)counter);
            is(test_buf[counter], (unsigned char)counter, "sxe_send() data is corrupted!");
            exit(1);
        }
    }

#endif

    tap_set_test_case_name("sxe_send(er) can still recieve data");

    test_buf_length = 0;
    sxe_write(test_server_connection, TEST_DATA_3, TEST_DATA_3_LENGTH);
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_read_client", "Third - client got a read event");
    is(tap_ev_arg(ev, "length"), TEST_DATA_3_LENGTH, "client read %u bytes", (unsigned)TEST_DATA_3_LENGTH);
    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending client events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events at this point");

#if !defined(WIN32) && !defined(__APPLE__)
    tap_set_test_case_name("sxe_notify_writable");

    // Note: this is really just testing that EV works...
    sxe_notify_writable(client, test_event_sxe_is_writable);
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_sxe_is_writable", "Client SXE is writable");

    // The SXE can now read data once again...
    test_buf_length = 0;
    sxe_write(test_server_connection, TEST_DATA_3, TEST_DATA_3_LENGTH);
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_read_client",     "Client got a read event");
    is(tap_ev_arg(ev, "length"), TEST_DATA_3_LENGTH, "Client read %u bytes", (unsigned)TEST_DATA_3_LENGTH);
    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending client events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events at this point");

    tap_set_test_case_name("SXE send buffer flow control");
    SXEA1(sizeof(test_data[0]) > 4 * (unsigned)window_size, "Only have %lu bytes to send, need %lu", SXE_CAST(unsigned long, sizeof(test_data[0])), SXE_CAST(unsigned long, 4 * window_size));
    SXEA1(setsockopt(client->socket,                 SOL_SOCKET, SO_SNDBUF, SXE_CAST(char *, &window_size), sizeof(window_size)) == 0,
          "setsockopt failed: %s", strerror(errno));
    SXEA1(setsockopt(test_server_connection->socket, SOL_SOCKET, SO_RCVBUF, SXE_CAST(char *, &window_size), sizeof(window_size)) == 0,
          "setsockopt failed: %s", strerror(errno));
    sxe_pause(test_server_connection);
    sxe_buffer_construct_plus(&buffer, (char *)test_data[0], 4 * window_size, 4 * window_size, client, test_event_buffer_consumed);
    is(sxe_send_buffer(client, &buffer), SXE_RETURN_IN_PROGRESS, "sxe_send() returns in progress when window is small");
    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending client events at this point");
    sxe_buf_resume(test_server_connection, SXE_BUF_RESUME_IMMEDIATE);
    test_ev_queue_wait_read(tap_q_server, TEST_WAIT, &ev, test_server_connection, "test_event_read_server", (char *)test_buf,
                            4 * (unsigned)window_size, "server after resume");
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_buffer_consumed", "Client buffer was cleared on close");

    tap_set_test_case_name("SXE send buffer freed on close");
    sxe_pause(test_server_connection);
    sxe_buffer_construct_plus(&buffer, (char *)test_data[0], 4 * window_size, 4 * window_size, client, test_event_buffer_consumed);
    is(sxe_send_buffer(client, &buffer), SXE_RETURN_IN_PROGRESS, "sxe_send() returns in progress when window is small");
    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending client events at this point");
    sxe_close(client);
    sxe_close(test_server_connection);
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_buffer_consumed", "Client buffer was cleared on close");

    tap_set_test_case_name("SXE send completes with error on late send failure");
    ok((client = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_read_client, NULL)) != NULL,
       "Got another new TCP SXE for client");
    is(sxe_connect_plus(client, "127.0.0.1", SXE_LOCAL_PORT(server), TEST_BUF_SIZE, TEST_BUF_SIZE), SXE_RETURN_OK,
       "SXE reconnecting from client");
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_event_connected_server", "Server connected");
    sxe_pause(test_server_connection);
    is(sxe_send(client, (char *)test_data[0], 4 * TEST_BUF_SIZE, test_event_send_complete), SXE_RETURN_IN_PROGRESS,
       "sxe_send() once more returns in progress when window is small");
    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending client events at this point");
    sxe_close(test_server_connection);
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_send_complete", "Send complete called on close");

    tap_set_test_case_name("sxe_send failure on closed SXE");
    is(sxe_send(client, TEST_DATA, TEST_DATA_LENGTH, test_event_send_complete), SXE_RETURN_ERROR_NO_CONNECTION,
       "sxe_send() fails and returns an error on a bad sxe/socket");

    tap_set_test_case_name("SXE send is re-entrant (called from cb)");
    ok((client = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_read_client, NULL)) != NULL, "New TCP SXE for client");
    is(sxe_connect_plus(client, "127.0.0.1", SXE_LOCAL_PORT(server), TEST_BUF_SIZE, TEST_BUF_SIZE), SXE_RETURN_OK, "reconnect from client");
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_event_connected_server", "Got reconnect on server side");
    sxe_pause(test_server_connection);
    is(sxe_send(client, (char *)test_data[0], 4 * TEST_BUF_SIZE, test_event_send_complete_so_resend), SXE_RETURN_IN_PROGRESS,
       "sxe_send() returns in progress when window is small and server side is paused");
    is(tap_ev_queue_length(tap_q_client),  0, "No pending client events at this point");
    sxe_buf_resume(test_server_connection, SXE_BUF_RESUME_IMMEDIATE);
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_send_complete_so_resend",
          "Resume completes the 1st send and the send from within the completion callback");
    test_ev_queue_wait_read(tap_q_server, TEST_WAIT, &ev, test_server_connection, "test_event_read_server", (char *)test_buf,
                            4 * (unsigned)window_size + SXE_LITERAL_LENGTH(TEST_SEND_WITHIN_CB),
                            "Server gets all the data expected - from both sxe_send() calls");

#endif

    tap_set_test_case_name(NULL);
    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending client events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events at this point");
    return exit_status();
}

