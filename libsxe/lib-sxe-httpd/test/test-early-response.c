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

#define TEST_WAIT          5.0
#define TEST_BUFFER_COUNT 10

static SXE_HTTPD httpd;

int
main(void)
{
    SXE_HTTPD_REQUEST      * request;
    SXE_HTTPD_BUFFER         rawbuf;
    tap_ev                   ev;
    SXE                    * listener;
    SXE                    * this;
    char                     buffer[1024];

    tap_plan(6, TAP_FLAG_ON_FAILURE_EXIT, NULL);
    test_sxe_register_and_init(12);

    sxe_httpd_construct(&httpd, 3, TEST_BUFFER_COUNT, 512, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, eoh, h_eoh);

    sxe_httpd_buffer_construct_const(&rawbuf, "Fiddlesticks\r\n", SXE_LITERAL_LENGTH("Fiddlesticks\r\n"), NULL, NULL);

    SXEA1((listener = test_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL,                         "sxe_httpd_listen failed");
    SXEA1((this = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, NULL)) != NULL, "sxe_new_tcp failed");
    sxe_connect(this, "127.0.0.1", SXE_LOCAL_PORT(listener));

#define TEST_PARTIAL_POST "POST / HTTP/1.1\r\n" \
                          "Content-Length: 42\r\n" \
                          "\r\n"

    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "client_connect",      "Client connected to HTTPD");
    TEST_SXE_SEND_LITERAL(this, TEST_PARTIAL_POST, client_sent, q_client, TEST_WAIT, &ev);

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev),            "h_eoh", "HTTPD: end of headers event");
    request = SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(ev, "request"));

    sxe_httpd_set_state_error(request);
    sxe_httpd_response_simple(request, h_sent, NULL, 408, "Request timeout", "Blah blah", NULL);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev),            "h_sent",    "HTTPD: finished responding");

#define EXPECTED_RESPONSE "HTTP/1.1 408 Request timeout\r\nContent-Length: 9\r\n\r\nBlah blah"

    test_ev_queue_wait_read(q_client, TEST_WAIT, &ev, this, "client_read", buffer, SXE_LITERAL_LENGTH(EXPECTED_RESPONSE), "client");
    is_strncmp(buffer, EXPECTED_RESPONSE, SXE_LITERAL_LENGTH(EXPECTED_RESPONSE),              "Client received correct response");

    return exit_status();
}
