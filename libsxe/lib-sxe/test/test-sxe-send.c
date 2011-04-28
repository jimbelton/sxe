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

#define TEST_WAIT                                2
#define SXE_MTU_SIZE                             1500
#define MTU_COUNTS_FOR_LARGER_THEN_SOCKET_BUFFER 200
#define TEST_DATA                                "TEST_DATA1234"
#define TEST_DATA_LENGTH                         (sizeof(TEST_DATA) - 1)
#define TEST_DATA_3                              "1234_TEST_DATA_3"
#define TEST_DATA_3_LENGTH                       (sizeof(TEST_DATA_3) - 1)

static unsigned char test_data[SXE_MTU_SIZE * MTU_COUNTS_FOR_LARGER_THEN_SOCKET_BUFFER]; // Large amount of data
static unsigned char test_buf[ SXE_MTU_SIZE * MTU_COUNTS_FOR_LARGER_THEN_SOCKET_BUFFER];
static unsigned      test_buf_length = 0;
static SXE *         server_side_connected_sxe;
static tap_ev_queue  tap_q_client;
static tap_ev_queue  tap_q_server;

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
    SXEE61I("test_event_read(length=%d)", length);

    memcpy(test_buf + test_buf_length, SXE_BUF(this), length);
    test_buf_length += length;
    tap_ev_queue_push(tap_q_client, __func__, 1, "length", length);

    SXE_BUF_CLEAR(this);
    SXER60I("return");
}

static void
test_event_read_server(SXE * this, int length)
{
    SXEE61I("test_event_read(length=%d)", length);

    memcpy(test_buf + test_buf_length, SXE_BUF(this), length);
    test_buf_length += length;
    tap_ev_queue_push(tap_q_server, __func__, 1, "length", length);

    SXE_BUF_CLEAR(this);
    SXER60I("return");
}

static void
test_event_send_complete(SXE * this, SXE_RETURN sxe_return)
{
    SXEE62I("test_event_send_complete(sxe_return=%s/%u)", sxe_return_to_string(sxe_return), sxe_return);
    tap_ev_queue_push(tap_q_client, __func__, 2, "this", this, "sxe_return", sxe_return);
    SXER60I("return");
}

static void
test_event_sxe_is_writable(SXE * this, SXE_RETURN sxe_return)
{
    SXEE61I("test_event_sxe_is_writable(sxe_return=%s)", sxe_return_to_string(sxe_return));
    tap_ev_queue_push(tap_q_client, __func__, 2, "this", this, "sxe_return", sxe_return);
    SXER60I("return");
}

int
main(void)
{
    SXE    * client;
    SXE    * server;
    tap_ev   ev;
    unsigned counter;
    SXE_RETURN  send_result;

    sxe_log_set_level(SXE_LOG_LEVEL_LIBRARY_TRACE);
    plan_tests(31);
    sxe_register(3, 0);
    tap_q_client = tap_ev_queue_new();
    tap_q_server = tap_ev_queue_new();

    is(sxe_init(), SXE_RETURN_OK, "SXE initialized");
    ok((server = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_connected_server, test_event_read_server, NULL)) != NULL,
                             "Got new TCP SXE for server");
    is(sxe_listen(server), SXE_RETURN_OK, "SXE listening on server");
    ok((client = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_read_client, NULL)) != NULL,
                             "Got new TCP SXE for client");
    is(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(server)), SXE_RETURN_OK, "SXE connecting from client");

    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_event_connected_server", "Server connected");

    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending events at this point");

    /* basic case (less then 1 buffer) */

    is(sxe_send(client, TEST_DATA, TEST_DATA_LENGTH, test_event_send_complete), SXE_RETURN_OK,
       "First - Able to send the whole buffer in one sxe_send() call. No callback needed");

    is_eq(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_event_read_server",
          "First - SXE server got read event");
    is(tap_ev_arg(ev, "length"), TEST_DATA_LENGTH, "Client got %u bytes", (unsigned)TEST_DATA_LENGTH);

    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending events at this point");

    /* send more then can fit in the sockets buffer */

    /* Prepopulate the data with known values
     */
    for (counter = 0; counter < sizeof(test_data); counter++) {
        test_data[counter] = counter;
    }

    test_buf_length = 0;
    send_result = sxe_send(client, test_data, sizeof(test_data), test_event_send_complete);
    if (send_result != SXE_RETURN_OK && send_result != SXE_RETURN_IN_PROGRESS) {
        fail("send_result neither 'OK' nor 'IN_PROGRESS': %s", sxe_return_to_string(send_result));
    }
    else {
        pass("send_result either OK or IN_PROGRESS");
    }

    for (counter = 0; counter < sizeof(test_data); counter += SXE_CAST(unsigned, tap_ev_arg(ev, "length"))) {
        if (strcmp(test_tap_ev_queue_identifier_wait(tap_q_server, 1, &ev), "test_event_read_server") != 0) {
            fail("Unexpected server event %s", (const char *)tap_ev_identifier(ev));
            break;
        }
    }

    is(counter,                            sizeof(test_data), "Second - Received all the data");
    is(tap_ev_queue_length(tap_q_server),  0,                 "Second - There are no pending server events at this point");

    if (send_result == SXE_RETURN_OK) {
        skip(2, "No call back needed: sxe_send() returned immediately");
    }
    else {
        is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_send_complete",
              "Second - Called back after sxe_send() completed");
        is((SXE_RETURN)tap_ev_arg(ev, "sxe_return"), SXE_RETURN_OK, "Second - sxe_send() was successful");
    }

    is(tap_ev_queue_length(tap_q_client),  0, "Second - There are no pending client events at this point");

    /* Verify the data that was read
     */
    for (counter = 0; counter < sizeof(test_data); counter++) {
        if (test_buf[counter] != (unsigned char)counter) {
            printf("sxe_send() data is corrupted: Got 0x%02x, expected 0x%02x\n", test_buf[counter], (unsigned char)counter);
            is(test_buf[counter], (unsigned char)counter, "sxe_send() data is corrupted!");
            exit(1);
        }
    }

    /* the sxe_send(er) can now still recieve data */

    test_buf_length = 0;
    sxe_write(server_side_connected_sxe, TEST_DATA_3, TEST_DATA_3_LENGTH);
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_read_client", "Third - client got a read event");
    is(tap_ev_arg(ev, "length"), TEST_DATA_3_LENGTH, "client read %u bytes", (unsigned)TEST_DATA_3_LENGTH);
    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending events at this point");

    /* simple test for sxe_notify_writable() */

    // Note: this is really just testing that EV works...
    sxe_notify_writable(client, test_event_sxe_is_writable);
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_sxe_is_writable", "Fourth - Client SXE is writable");
    // The SXE can now read data once again...
    test_buf_length = 0;
    sxe_write(server_side_connected_sxe, TEST_DATA_3, TEST_DATA_3_LENGTH);
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_read_client", "Fourth - client got a read event");
    is(tap_ev_arg(ev, "length"), TEST_DATA_3_LENGTH, "client read %u bytes", (unsigned)TEST_DATA_3_LENGTH);
    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending events at this point");

    /* sxe_send() failure */

    sxe_close(client);
    is(sxe_send(client, TEST_DATA, TEST_DATA_LENGTH, test_event_send_complete), SXE_RETURN_ERROR_NO_CONNECTION,
       "sxe_send() fails and returns an error on a bad sxe/socket");

    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending events at this point");
    return exit_status();
}

