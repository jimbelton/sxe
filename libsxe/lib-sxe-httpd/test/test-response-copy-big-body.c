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
#include <errno.h>

#include "tap.h"
#include "sxe-httpd.h"
#include "sxe-test.h"
#include "sxe-util.h"

#include "common.h"

#define TEST_WAIT           5.0
#define TEST_SEND_BYTES   640 * 1024
#define TEST_BUFFER_SIZE  512
#define TEST_BUFFER_COUNT 1 + (TEST_SEND_BYTES / TEST_BUFFER_SIZE)

static SXE_HTTPD httpd;

int
main(void)
{
    SXE_HTTPD_REQUEST      * request;
    tap_ev                   ev;
    SXE                    * listener;
    SXE                    * this;
    char                   * send_buffer;
    char                   * recv_buffer;
    char                   * test_buffer;
    int                      i, j;

    SXEA1(send_buffer = malloc(TEST_SEND_BYTES), "Failed to allocate %u KB", TEST_SEND_BYTES);
    SXEA1(recv_buffer = malloc(TEST_SEND_BYTES + 1024), "Failed to allocate %u KB", TEST_SEND_BYTES + 1024);
    SXEA1(test_buffer = malloc(TEST_SEND_BYTES + 1024), "Failed to allocate %u KB", TEST_SEND_BYTES + 1024);

    j = snprintf(test_buffer, TEST_SEND_BYTES + 1024, "HTTP/1.1 200 OK\r\n\r\n");
    for (i = 0; i < TEST_SEND_BYTES; i++, j++) {
        send_buffer[i] = 'A';
        test_buffer[j] = 'A';
    }

    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_POOL=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LIST=5");
    tap_plan(6, TAP_FLAG_ON_FAILURE_EXIT, NULL);
    test_sxe_register_and_init(12);

    sxe_httpd_construct(&httpd, 3, TEST_BUFFER_COUNT, TEST_BUFFER_SIZE, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, respond, h_respond);

    SXEA1((listener = test_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL,                         "sxe_httpd_listen failed");
    SXEA1((this = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, NULL)) != NULL, "sxe_new_tcp failed");
    sxe_connect(this, "127.0.0.1", SXE_LOCAL_PORT(listener));

#define TEST_GET "GET / HTTP/1.1\r\n\r\n"

    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "client_connect",      "Client connected to HTTPD");
    TEST_SXE_SEND_LITERAL(this, TEST_GET, client_sent, q_client, TEST_WAIT, &ev);

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev),            "h_respond", "HTTPD: respond event");
    request = SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(ev, "request"));

    sxe_httpd_response_start(request, 200, "OK");
    sxe_httpd_response_copy_body_data(request, send_buffer, TEST_SEND_BYTES);
    sxe_httpd_response_end(request, h_sent, NULL);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev),            "h_sent",    "HTTPD: finished responding");

    test_ev_queue_wait_read(q_client, TEST_WAIT, &ev, this, "client_read", recv_buffer, j, "client");
    is_strncmp(recv_buffer, test_buffer, j,              "Client received correct response");

    return exit_status();
}
