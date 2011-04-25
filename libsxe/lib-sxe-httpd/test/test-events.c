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

#define TEST_WAIT 5.0

static tap_ev_queue q_client;
static tap_ev_queue q_httpd;

static void
h_connect(struct SXE_HTTPD_REQUEST *request)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE61I("%s()", __func__);
    tap_ev_queue_push(q_httpd, __func__, 1, "request", request);
    SXER60I("return");
}

static void
h_request(struct SXE_HTTPD_REQUEST *request, const char *method, unsigned mlen, const char *url, unsigned ulen, const char *version, unsigned vlen)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE67I("%s(method=[%.*s],url=[%.*s],version=[%.*s])", __func__, mlen, method, ulen, url, vlen, version);
    tap_ev_queue_push(q_httpd, __func__, 4,
                      "request", request,
                      "url", tap_dup(url, ulen),
                      "method", tap_dup(method, mlen),
                      "version", tap_dup(version, vlen));
    SXER60I("return");
}

static void
h_header(struct SXE_HTTPD_REQUEST *request, const char *key, unsigned klen, const char *val, unsigned vlen)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE65I("%s(key=[%.*s],value=[%.*s])", __func__, klen, key, vlen, val);
    tap_ev_queue_push(q_httpd, __func__, 3,
                      "request", request,
                      "key", tap_dup(key, klen),
                      "value", tap_dup(val, vlen));
    SXER60I("return");
}

static void
h_eoh(struct SXE_HTTPD_REQUEST *request)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE61I("%s()", __func__);
    tap_ev_queue_push(q_httpd, __func__, 1, "request", request);
    SXER60I("return");
}

static void
h_body(struct SXE_HTTPD_REQUEST *request, const char *buf, unsigned used)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE63I("%s(buf=%p,used=%u)", __func__, buf, used);
    tap_ev_queue_push(q_httpd, __func__, 4,
                      "request", request,
                      "buf", tap_dup(buf, used),
                      "used", used);
    SXER60I("return");
}

static void
h_respond(struct SXE_HTTPD_REQUEST *request)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE61I("%s()", __func__);
    tap_ev_queue_push(q_httpd, __func__, 1, "request", request);
    SXER60I("return");
}

static void
h_close(struct SXE_HTTPD_REQUEST *request)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE61I("%s()", __func__);
    tap_ev_queue_push(q_httpd, __func__, 1, "request", request);
    SXER60I("return");
}

static void
h_sent(SXE_HTTPD_REQUEST *request, SXE_RETURN result, void *user_data)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(user_data);
    SXEE61I("%s()", __func__);
    tap_ev_queue_push(q_httpd, __func__, 2, "request", request, "result", result);
    SXER60I("return");
}

static void
client_connect(SXE * this)
{
    SXEE61I("%s()", __func__);
    tap_ev_queue_push(q_client, __func__, 1, "this", this);
    SXER60I("return");
}

static void
client_read(SXE * this, int length)
{
    SXE_UNUSED_PARAMETER(length);
    SXEE62I("%s(length=%u)", __func__, length);
    tap_ev_queue_push(q_client, __func__, 3, "this", this, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)), "used", SXE_BUF_USED(this));
    sxe_buf_clear(this);
    SXER60I("return");
}

int
main(void)
{
    SXE_HTTPD                httpd;
    SXE_HTTPD_REQUEST      * request;
    sxe_httpd_header_handler old_header_handler;
    tap_ev                   ev;
    SXE                    * listener;
    SXE                    * c;
    char                     buffer[1024];

    tap_plan(27, TAP_FLAG_ON_FAILURE_EXIT, NULL);
    sxe_register(4, 0);        /* http listener and connections */
    sxe_register(8, 0);        /* http clients */
    sxe_init();

    q_client = tap_ev_queue_new();
    q_httpd = tap_ev_queue_new();

    sxe_httpd_construct(&httpd, 3, 10, 512, 0);

    SXE_HTTPD_SET_HANDLER(&httpd, connect, h_connect);
    SXE_HTTPD_SET_HANDLER(&httpd, request, h_request);
    old_header_handler = SXE_HTTPD_SET_HANDLER(&httpd, header,  h_header);
    SXE_HTTPD_SET_HANDLER(&httpd, eoh,     h_eoh);
    SXE_HTTPD_SET_HANDLER(&httpd, body,    h_body);
    SXE_HTTPD_SET_HANDLER(&httpd, respond, h_respond);
    SXE_HTTPD_SET_HANDLER(&httpd, close,   h_close);

    listener = sxe_httpd_listen(&httpd, "0.0.0.0", 0);

    c = sxe_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, NULL);
    sxe_connect(c, "127.0.0.1", SXE_LOCAL_PORT(listener));

    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "client_connect",      "Client connected to HTTPD");
    SXE_WRITE_LITERAL(c, "GET /this/is/a/URL HTTP/1.1\r\nConnection: whatever\r\nHost: interesting\r\nContent-Length: 10\r\n\r\n12345678\r\n");

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_connect",            "HTTPD: connected");

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
    SXE_WRITE_LITERAL(c, "GET ");    /* Send the beginning of the next message           */
    test_ev_loop_wait(TEST_WAIT);    /* Try to make sure it's received before responding */
    sxe_httpd_response_simple(request, h_sent, NULL, 200, "OK", "abcd", NULL);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_sent",               "HTTPD: finished responding");

#define EXPECTED_RESPONSE "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nabcd"

    test_ev_queue_wait_read(q_client, TEST_WAIT, &ev, c, "client_read", buffer, SXE_LITERAL_LENGTH(EXPECTED_RESPONSE), "client");
    is_strncmp(buffer, EXPECTED_RESPONSE, SXE_LITERAL_LENGTH(EXPECTED_RESPONSE),              "Client received correct response");

    /* Now try it again, this time with only the respond handler hooked. */
    SXE_HTTPD_SET_HANDLER(&httpd, request, NULL);               /* try resetting to a NULL handler */
    SXE_HTTPD_SET_HANDLER(&httpd, header, old_header_handler);  /* try explicitly re-setting the old handler */
    SXE_HTTPD_SET_HANDLER(&httpd, eoh, NULL);
    SXE_HTTPD_SET_HANDLER(&httpd, body, NULL);

    /* Finish writing the next message
     */
    SXE_WRITE_LITERAL(c, "/this/is/a/URL HTTP/1.1\r\nConnection: whatever\r\nHost: interesting\r\nContent-Length: 10\r\n\r\n12345678\r\n");
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_respond",            "HTTPD: respond event");
    request = SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(ev, "request"));
    sxe_httpd_response_raw(request, EXPECTED_RESPONSE, SXE_LITERAL_LENGTH(EXPECTED_RESPONSE));
    sxe_httpd_response_end(request, h_sent, NULL);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_sent",               "HTTPD: finished responding");
    test_ev_queue_wait_read(q_client, TEST_WAIT, &ev, c, "client_read", buffer, SXE_LITERAL_LENGTH(EXPECTED_RESPONSE), "client");
    is_strncmp(buffer, EXPECTED_RESPONSE, SXE_LITERAL_LENGTH(EXPECTED_RESPONSE),              "Client received correct response");

    sxe_close(c);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "h_close",              "HTTPD: close event");

    return exit_status();
}
