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

#include "sxe-httpd.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_WAIT               5.0
#define TEST_200_RESPONSE       "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
#define TEST_200_CLOSE_RESPONSE "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"

tap_ev_queue q_httpd;

#ifndef WINDOWS_NT

static void
test_event_httpd_request(SXE_HTTPD_REQUEST *request, const char * method, unsigned method_length, const char * url,
                         unsigned url_length, const char * version, unsigned version_length)
{
    SXEE68("%s(request=%p, method='%.*s', url='%.*s', version='%*.s'", __func__, request, method_length, method,
           url_length, url, version_length, version);
    tap_ev_queue_push(q_httpd, __func__, 7, "request", request, "method_length", method_length, "method", method,
                     "url_length", url_length, "url", url, "version_length", version_length, "version", version);
    SXER60("return");
}

static void
test_event_httpd_respond(SXE_HTTPD_REQUEST *request)
{
    SXEE62("%s(request=%p)", __func__, request);
    tap_ev_queue_push(q_httpd, __func__, 1, "request", request);
    SXER60("return");
}

static void
test_event_httpd_sent(SXE_HTTPD_REQUEST *request, SXE_RETURN result, void *user_data)
{
    SXE_UNUSED_PARAMETER(user_data);
    SXEE62("%s(request=%p)", __func__, request);
    tap_ev_queue_push(q_httpd, __func__, 2, "request", request, "result", result);
    SXER60("return");
}

static void
test_event_client_file_sent(SXE * this, SXE_RETURN final)
{
    SXEE63I("%s(this=%p, final=%s)", __func__, this, sxe_return_to_string(final));
    tap_ev_push(__func__, 2, "this", this, "final", final);
    SXER60("return");
}

static void
test_event_client_connected(SXE * this)
{
    SXEE62I("%s(this=%p)", __func__, this);
    tap_ev_push(__func__, 1, "this", this);
    SXER60("return");
}

static void
test_event_client_read(SXE * this, int length)
{
    SXE_UNUSED_PARAMETER(length);
    SXEE64I("%s(this=%p, buf='%.*s')", __func__, this, SXE_BUF_USED(this), SXE_BUF(this));
    tap_ev_push(__func__, 3, "this", this, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)), "used", SXE_BUF_USED(this));
    sxe_buf_clear(this);
    SXER60("return");
}

static void
test_event_client_close(SXE * this)
{
    SXEE62I("%s(this=%p)", __func__, this);
    tap_ev_push(__func__, 1, "this", this);
    SXER60("return");
}

#endif

int
main(void)
{
#ifdef WINDOWS_NT
    plan_skip_all("No sendfile on windows");
#else
    SXE_HTTPD           httpd;
    SXE               * listener;
    SXE               * client;
    tap_ev              ev;
    char                buffer[1024];
    size_t              length;
    SXE_RETURN          result;
    int                 fd;
    struct stat         file_status;
    SXE_HTTPD_REQUEST * request;
    off_t               offset;

    q_httpd = tap_ev_queue_new();
    tap_plan(20, TAP_FLAG_ON_FAILURE_EXIT | TAP_FLAG_DEBUG, NULL);

    sxe_register(4, 0);        /* http listener and connections */
    sxe_register(8, 0);        /* http clients */
    sxe_init();

    sxe_httpd_construct(  &httpd, 3, 10, 512, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, request, test_event_httpd_request);
    SXE_HTTPD_SET_HANDLER(&httpd, respond, test_event_httpd_respond);

    listener        = sxe_httpd_listen(&httpd, "0.0.0.0", 0);
    httpd.user_data = listener;
    client          = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_client_connected, test_event_client_read, test_event_client_close);
    sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener));

    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_event_client_connected", "Got a connected event");
    is(tap_ev_arg(ev, "this"), client,                                                "On the client");
    SXEA11((fd = open("../sxe-httpd.c", O_RDONLY)) >= 0,                              "Failed to open '../sxe-httpd.c': %s", strerror(errno));
    SXEA11(fstat(fd, &file_status)                          >= 0,                              "fstat failed: %s", strerror(errno));
    length = snprintf(buffer, sizeof(buffer), "POST /sxe-httpd.c HTTP/1.1\r\nContent-Type: text/plain\r\n"
                                              "Content-Length: %lu\r\n\r\n", (unsigned long)file_status.st_size);
    SXEA11((result = sxe_write(client, buffer, length)) == SXE_RETURN_OK,             "sxe_write failed: %s", sxe_return_to_string(result));
    offset = 0;
    sxe_sendfile(client, fd, &offset, (unsigned)file_status.st_size, test_event_client_file_sent);

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "test_event_httpd_request", "Got a request event");
    request = SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(ev, "request"));
    is(SXE_CAST(int, tap_ev_arg(ev, "url_length")), (int)strlen("/sxe-httpd.c"),     "Request URL has the expected length");
    is_strncmp(tap_ev_arg(ev, "url"), "/sxe-httpd.c", strlen("/sxe-httpd.c"),         "Request is for URL '/sxe-httpd.c'");

    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_event_client_file_sent", "Got a file-sent event");

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "test_event_httpd_respond", "Got a respond event");
    request = SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(ev, "request"));
    sxe_httpd_response_simple(request, test_event_httpd_sent, NULL, 200, "OK", NULL, NULL);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "test_event_httpd_sent", "Server finished sending");

    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_event_client_read",      "Got a read event");
    is(SXE_CAST(int, tap_ev_arg(ev, "used")), (int)strlen(TEST_200_RESPONSE),        "POST response has expected length");
    is_strncmp(tap_ev_arg(ev, "buf"), TEST_200_RESPONSE, strlen(TEST_200_RESPONSE),   "POST response is a 200 OK");
    SXE_WRITE_LITERAL(client, "GET / HTTP/1.1\r\nHost: foobar\r\nConnection: close\r\n\r\n");

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "test_event_httpd_request", "Got a request event");
    request = SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(ev, "request"));
    is(SXE_CAST(int, tap_ev_arg(ev, "url_length")), (int)strlen("/"),                "Request URL has the expected length");
    is_strncmp(tap_ev_arg(ev, "url"), "/", strlen("/"),                               "Request is for URL '/'");

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "test_event_httpd_respond", "Got a respond event");
    request = SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(ev, "request"));
    sxe_httpd_response_simple(request, test_event_httpd_sent, NULL, 200, "OK", NULL, "Connection", "close", NULL);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev), "test_event_httpd_sent", "Server finished sending");
    sxe_close(request->sxe);

    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_event_client_read",      "Got a read event");
    is(SXE_CAST(int, tap_ev_arg(ev, "used")), (int)strlen(TEST_200_CLOSE_RESPONSE),  "GET response has expected length");
    is_strncmp(tap_ev_arg(ev, "buf"), TEST_200_CLOSE_RESPONSE, strlen(TEST_200_CLOSE_RESPONSE),
                                                                                      "GET response is a 200 OK with close");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_event_client_close",     "Got a close event");
#endif

    return exit_status();
}
