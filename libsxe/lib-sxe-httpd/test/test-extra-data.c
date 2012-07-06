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

#define TEST_WAIT 5.0

int
main(void)
{
    SXE_HTTPD                httpd;
    SXE_HTTPD_REQUEST      * request;
    tap_ev                   ev;
    SXE                    * listener;
    SXE                    * c;
    SXE                    * c2;
    char                     buffer[1024];

    tap_plan(24, TAP_FLAG_ON_FAILURE_EXIT, NULL);
    test_sxe_register_and_init(12);

    sxe_httpd_construct(&httpd, 3, 10, 512, 0);

    SXE_HTTPD_SET_HANDLER(&httpd, connect, h_connect);
    SXE_HTTPD_SET_HANDLER(&httpd, request, h_request);
    SXE_HTTPD_SET_HANDLER(&httpd, header,  h_header);
    SXE_HTTPD_SET_HANDLER(&httpd, eoh,     h_eoh);
    SXE_HTTPD_SET_HANDLER(&httpd, body,    h_body);
    SXE_HTTPD_SET_HANDLER(&httpd, respond, h_respond);
    SXE_HTTPD_SET_HANDLER(&httpd, close,   h_close);

    listener = test_httpd_listen(&httpd, "0.0.0.0", 0);

    c = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, NULL);
    sxe_connect(c, "127.0.0.1", SXE_LOCAL_PORT(listener));

    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "client_connect",      "1st Client connected to HTTPD");
    TEST_SXE_SEND_LITERAL(c, "POST /this/is/a/URL HTTP/1.1\r\nContent-Length: 10\r\n\r\n12345678\r\n", client_sent, q_client, TEST_WAIT, &ev);

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_connect",            "HTTPD: connected");
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_request",            "HTTPD: new request");

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_header",             "HTTPD: header event");
    is_strncmp(tap_ev_arg(ev, "key"), "Content-Length", SXE_LITERAL_LENGTH("Content-Length"), "HTTPD: header was 'Connect'");
    is_strncmp(tap_ev_arg(ev, "value"), "10", SXE_LITERAL_LENGTH("10"),                       "HTTPD: header value was 'whatever'");

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_eoh",                "HTTPD: eoh (end of headers) event");

    test_ev_queue_wait_read(q_httpd, TEST_WAIT, &ev, NULL, "h_body", buffer, 10, "HTTPD body handler");
    is_strncmp(buffer, "12345678\r\n", SXE_LITERAL_LENGTH("12345678\r\n"),                    "HTTPD: read correct body");

    /* httpd is still in "req_response" state */
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_respond",            "HTTPD: respond event");

    /* Extra data from client, e.g., "content-length" is wrong and shorter than real packet */
    TEST_SXE_SEND_LITERAL(c, "AB", client_sent, q_client, TEST_WAIT, &ev);

    /* Send another valid request again, the connection is in "sink" mode now, data will get ignored */
    TEST_SXE_SEND_LITERAL(c, "POST /this/is/a/URL HTTP/1.1\r\nContent-Length: 10\r\n\r\n12345678\r\n", client_sent, q_client, TEST_WAIT, &ev);
    test_process_all_libev_events();

    /* In sink mode, no more httpd events */
    is(tap_ev_queue_length(q_httpd), 0, "No lurking httpd events");

    /* New client connection should not be affected */
    c2 = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, client_close);
    sxe_connect(c2, "127.0.0.1", SXE_LOCAL_PORT(listener));

    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "client_connect",      "2nd Client connected to HTTPD");
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_connect",            "HTTPD: connected");

    TEST_SXE_SEND_LITERAL(c2, "GET /simple HTTP/1.1\r\n\r\n", client_sent, q_client, TEST_WAIT, &ev);

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_request",            "HTTPD: new request");
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_eoh",                "HTTPD: eoh (end of headers) event");
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_respond",            "HTTPD: respond event");
    request = (SXE_HTTPD_REQUEST *)(long)tap_ev_arg(ev, "request");
    sxe_httpd_response_simple(request, h_sent, NULL, 200, "OK", NULL, "Connection", "close", NULL);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_sent",               "HTTPD: finished responding");
    sxe_close(request->sxe);

#define TEST_200_CLOSE_RESPONSE "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"
    test_ev_queue_wait_read(q_client, TEST_WAIT, &ev, c2, "client_read", buffer, SXE_LITERAL_LENGTH(TEST_200_CLOSE_RESPONSE), "client");
    is_strncmp(buffer, TEST_200_CLOSE_RESPONSE, SXE_LITERAL_LENGTH(TEST_200_CLOSE_RESPONSE),  "GET response is a 200 OK with close");
    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "client_close",        "Got a client close event");

    return exit_status();
}
