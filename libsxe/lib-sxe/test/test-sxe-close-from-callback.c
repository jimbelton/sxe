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

#ifdef _WIN32

int
main(void)
{
    plan_skip_all("- skip this test program - I don't care if this works on windows or not");
    return exit_status();
}

#else /* _WIN32 */

static unsigned char test_data[   TEST_HUGE_COUNT][TEST_HUGE_BUFFER];     /* Large amount of data */
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
test_event_buffer_consumed(void * user_data, SXE_BUFFER * buffer)
{
    SXE * this = (SXE *)user_data;

    SXEE6I("(buffer=%p)", buffer);
    tap_ev_queue_push(tap_q_client, __func__, 2, "this", this, "buffer", buffer);
    sxe_close(this);
    SXER6I("return");
}

int
main(void)
{
    SXE      * client;
    SXE      * server;
    tap_ev     ev;
    int        window_size      = TEST_BUF_SIZE;
    SXE_BUFFER buffer;

    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LIST=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_POOL=5");
    sxe_log_set_level(SXE_LOG_LEVEL_TRACE);

    tap_plan(13, TAP_FLAG_ON_FAILURE_EXIT, NULL);

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

    tap_set_test_case_name("SXE send buffer freed on close");
    sxe_pause(test_server_connection);
    sxe_buffer_construct_plus(&buffer, (char *)test_data[0], 4 * window_size, 4 * window_size, client, test_event_buffer_consumed);
    is(sxe_send_buffer(client, &buffer), SXE_RETURN_IN_PROGRESS, "sxe_send() returns in progress when window is small");
    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending client events at this point");
    sxe_close(client);
    sxe_close(test_server_connection);
    is_eq(test_tap_ev_queue_identifier_wait(tap_q_client, 1, &ev), "test_event_buffer_consumed", "Client buffer was cleared on close");

    tap_set_test_case_name(NULL);
    is(tap_ev_queue_length(tap_q_client),  0, "There are no pending client events at this point");
    is(tap_ev_queue_length(tap_q_server),  0, "There are no pending server events at this point");
    return exit_status();
}

#endif

