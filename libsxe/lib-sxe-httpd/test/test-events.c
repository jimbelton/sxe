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
    sxe_httpd_header_handler old_header_handler;
    tap_ev                   ev;
    SXE                    * listener;
    SXE                    * this;
    char                     buffer[1024];

    tap_plan(40, TAP_FLAG_ON_FAILURE_EXIT, NULL);
    test_sxe_register_and_init(12);

    sxe_httpd_construct(&httpd, 3, TEST_BUFFER_COUNT, 512, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, connect, h_connect);
    SXE_HTTPD_SET_HANDLER(&httpd, request, h_request);
    old_header_handler = SXE_HTTPD_SET_HANDLER(&httpd, header,  h_header);
    SXE_HTTPD_SET_HANDLER(&httpd, eoh,     h_eoh);
    SXE_HTTPD_SET_HANDLER(&httpd, body,    h_body);
    SXE_HTTPD_SET_HANDLER(&httpd, respond, h_respond);
    SXE_HTTPD_SET_HANDLER(&httpd, close,   h_close);

    SXEA1((listener = test_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL,                         "sxe_httpd_listen failed");
    SXEA1((this = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, NULL)) != NULL, "sxe_new_tcp failed");
    sxe_connect(this, "127.0.0.1", SXE_LOCAL_PORT(listener));

#define TEST_GET "GET /this/is/a/URL HTTP/1.1\r\nConnection: whatever\r\nHost: interesting\r\n" \
                 "Content-Length: 10\r\n\r\n12345678\r\n"

    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "client_connect",      "Client connected to HTTPD");
    TEST_SXE_SEND_LITERAL(this, TEST_GET, client_sent, q_client, TEST_WAIT, &ev);

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_connect",            "HTTPD: connected");

//  is(sxe_httpd_diag_get_active_requests (&httpd), 1                , "# active requests" ); /* note: deferred events can cause these */
//  is(sxe_httpd_diag_get_idle_connections(&httpd), 0                , "# idle connections"); /* to be zero on Windows release sometimes */
    is(sxe_httpd_diag_get_free_connections(&httpd), 2                , "# free connections");
    is(sxe_httpd_diag_get_free_buffers    (&httpd), TEST_BUFFER_COUNT, "# unused buffers"  );

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_request",            "HTTPD: new request");
    is_strncmp(tap_ev_arg(ev, "url"), "/this/is/a/URL", SXE_LITERAL_LENGTH("/this/is/a/URL"), "HTTPD: URL is correct");
    is_strncmp(tap_ev_arg(ev, "method"), "GET", SXE_LITERAL_LENGTH("GET"),                    "HTTPD: method is 'GET'");
    is_strncmp(tap_ev_arg(ev, "version"), "HTTP/1.1", SXE_LITERAL_LENGTH("HTTP/1.1"),         "HTTPD: version is 'HTTP/1.1'");

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_header",             "HTTPD: header event");
    is_strncmp(tap_ev_arg(ev, "key"), "Connection", SXE_LITERAL_LENGTH("Connection"),         "HTTPD: header was 'Connect'");
    is_strncmp(tap_ev_arg(ev, "value"), "whatever", SXE_LITERAL_LENGTH("whatever"),           "HTTPD: header value was 'whatever'");

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_header",             "HTTPD: header event");
    is_strncmp(tap_ev_arg(ev, "key"), "Host", SXE_LITERAL_LENGTH("Host"),                     "HTTPD: header was 'Connect'");
    is_strncmp(tap_ev_arg(ev, "value"), "interesting", SXE_LITERAL_LENGTH("interesting"),     "HTTPD: header value was 'whatever'");

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_header",             "HTTPD: header event");
    is_strncmp(tap_ev_arg(ev, "key"), "Content-Length", SXE_LITERAL_LENGTH("Content-Length"), "HTTPD: header was 'Connect'");
    is_strncmp(tap_ev_arg(ev, "value"), "10", SXE_LITERAL_LENGTH("10"),                       "HTTPD: header value was 'whatever'");

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_eoh",                "HTTPD: eoh (end of headers) event");

    test_ev_queue_wait_read(q_httpd, TEST_WAIT, &ev, NULL, "h_body", buffer, 10, "HTTPD body handler");
    is_strncmp(buffer, "12345678\r\n", SXE_LITERAL_LENGTH("12345678\r\n"),                    "HTTPD: read correct body");

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_respond",            "HTTPD: respond event");
    request = SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(ev, "request"));
    TEST_SXE_SEND_LITERAL(this, "GET ", client_sent, q_client, TEST_WAIT, &ev);
    test_ev_loop_wait(TEST_WAIT);    /* Try to make sure it's received before responding */
    sxe_httpd_response_simple(request, h_sent, NULL, 200, "OK", "abcd", NULL);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_sent",               "HTTPD: finished responding");

#define EXPECTED_RESPONSE "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nabcd"

    test_ev_queue_wait_read(q_client, TEST_WAIT, &ev, this, "client_read", buffer, SXE_LITERAL_LENGTH(EXPECTED_RESPONSE), "client");
    is_strncmp(buffer, EXPECTED_RESPONSE, SXE_LITERAL_LENGTH(EXPECTED_RESPONSE),              "Client received correct response");

    /* Now try it again, this time with only the respond handler hooked. */
    SXE_HTTPD_SET_HANDLER(&httpd, request, NULL);               /* try resetting to a NULL handler */
    SXE_HTTPD_SET_HANDLER(&httpd, header, old_header_handler);  /* try explicitly re-setting the old handler */
    SXE_HTTPD_SET_HANDLER(&httpd, eoh, NULL);
    SXE_HTTPD_SET_HANDLER(&httpd, body, NULL);

    /* Finish writing the next message
     */
    TEST_SXE_SEND_LITERAL(this, "/this/is/a/URL HTTP/1.1\r\nConnection: whatever\r\nHost: interesting\r\nContent-Length: 10\r\n\r\n12345678\r\n", client_sent, q_client, TEST_WAIT, &ev);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_respond",            "HTTPD: respond event");
    request = SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(ev, "request"));
    sxe_httpd_response_copy_raw_data(request, EXPECTED_RESPONSE, SXE_LITERAL_LENGTH(EXPECTED_RESPONSE));
    sxe_httpd_response_end(request, h_sent, NULL);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_sent",               "HTTPD: finished responding");
    test_ev_queue_wait_read(q_client, TEST_WAIT, &ev, this, "client_read", buffer, SXE_LITERAL_LENGTH(EXPECTED_RESPONSE), "client");
    is_strncmp(buffer, EXPECTED_RESPONSE, SXE_LITERAL_LENGTH(EXPECTED_RESPONSE),              "Client received correct response");

    sxe_close(this);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_close",              "HTTPD: close event");

    /* For coverage on window */
    this = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, client_close);
    sxe_connect(this, "127.0.0.1", SXE_LOCAL_PORT(listener));

    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "client_connect",      "Client connected to HTTPD");
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_connect",            "HTTPD: connected");

    TEST_SXE_SEND_LITERAL(this, "GET /simple HTTP/1.1\r\n\r\n", client_sent, q_client, TEST_WAIT, &ev);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_respond",            "HTTPD: respond event");
    request = (SXE_HTTPD_REQUEST *)(long)tap_ev_arg(ev, "request");
    sxe_httpd_response_simple(request, h_sent, NULL, 200, "OK", NULL, "Connection", "close", NULL);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_sent",               "HTTPD: finished responding");
    sxe_close(request->sxe);

#define TEST_200_CLOSE_RESPONSE "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"
    test_ev_queue_wait_read(q_client, TEST_WAIT, &ev, this, "client_read", buffer, SXE_LITERAL_LENGTH(TEST_200_CLOSE_RESPONSE), "client");
    is_strncmp(buffer, TEST_200_CLOSE_RESPONSE, SXE_LITERAL_LENGTH(TEST_200_CLOSE_RESPONSE),  "GET response is a 200 OK with close");
    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "client_close",        "Got a client close event");

    return exit_status();
}
