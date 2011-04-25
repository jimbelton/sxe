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

/* This test proves that the HTTP server can listen on a pipe, and receive remote HTTP clients (over TCP) via the pipe.
 * This facility is needed for the WDX server on the web appliance. */

#include <string.h>

#include "sxe-httpd.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_WAIT             5
#define TEST_URL              "/"
#define TEST_HTTP_REQUEST     "GET " TEST_URL " HTTP/1.1\r\n\r\n"
#define TEST_HTTP_RESPONSE    "HTTP/1.1 200 OK\r\nContent-Length: 7\r\n\r\nPong!\r\n"
#define TEST_PIPE_NAME_PREFIX "/tmp/pipe."
#define TEST_PID_MAX_DIGITS   10

#ifdef WINDOWS_NT

int
main(void)
{
    plan_skip_all("Pipes are not yet supported on windows");
    return exit_status();
}

#else

static char  pipe_name[] = "/tmp/pipe.##########";    /* Must be in /tmp due to use of Samba on SXL pairing stations */
static SXE * pipe_listener;                           /* HTTP server, listening on a pipe                            */
static SXE * pipe_client;                             /* Apache -- the pipe client                                   */
static SXE * tcp_listener;                            /* Apache -- listening on TCP                                  */
static SXE * tcp_client;                              /* The remote HTTP client                                      */

static void
evhttp_connect(SXE_HTTPD_REQUEST *request)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXEE92I("%s(request=%p)", __func__, request);
    tap_ev_push(__func__, 1, "request", request);
    SXER90I("return");
}

static void
evhttp_request(SXE_HTTPD_REQUEST *request,
               const char *method, unsigned method_len,
               const char *url, unsigned url_len,
               const char *version, unsigned version_len)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(method);
    SXE_UNUSED_PARAMETER(method_len);
    SXE_UNUSED_PARAMETER(version);
    SXE_UNUSED_PARAMETER(version_len);
    SXEE98I("%s(request=%p,method=[%.*s],url=[%.*s],version=[%.*s])", __func__, request,
            method_len, method, url_len, url, version_len, version);
    tap_ev_push(__func__, 3,
                "request", request,
                "url", tap_dup(url, url_len),
                "url_len", url_len);
    SXER90I("return");
}

static void
evhttp_respond(SXE_HTTPD_REQUEST *request)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXEE92I("%s(request=%p)", __func__, request);
    tap_ev_push(__func__, 1, "request", request);
    SXER90I("return");
}

static void
evhttp_close(SXE_HTTPD_REQUEST *request)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXEE92I("%s(request=%p)", __func__, request);
    tap_ev_push(__func__, 1, "request", request);
    SXER90I("return");
}

static void
evhttp_sent(SXE_HTTPD_REQUEST *request, SXE_RETURN result, void *user_data)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(user_data);
    SXEE93I("%s(request=%p,result=%s)", __func__, request, sxe_return_to_string(result));
    tap_ev_push(__func__, 2, "request", request, "result", result);
    SXER90I("return");
}

static void
test_event_client_connected(SXE * this)
{
    SXEE60I("test_event_client_connected()");
    SXE_WRITE_LITERAL(this, TEST_HTTP_REQUEST);
    SXER60I("return");
}

static void
test_event_proxy_pipe_connected(SXE * this)
{
    SXE * that = SXE_USER_DATA(this);

    SXEE60I("test_event_proxy_pipe_connected()");
    sxe_write_pipe(this, SXE_BUF(that), SXE_BUF_USED(that), that->socket);
    SXER60I("return");
}

static void
test_event_proxy_pipe_closed(SXE * this)
{
    SXE * that = SXE_USER_DATA(this);
    SXEE60I("test_event_proxy_pipe_closed()");
    sxe_close(that);                              /* close the accepted TCP socket once it's been received by the server */
    SXER60I("return");
}

static void
test_event_proxy_tcp_read(SXE * this, int length)
{
    SXEE61I("test_event_proxy_tcp_read(length=%d)", length);
    SXE_UNUSED_PARAMETER(length);

    if (SXE_BUF_USED(this) < SXE_LITERAL_LENGTH(TEST_HTTP_REQUEST)) {
        SXEL23I("test_event_proxy_tcp_read: Received %u bytes of %u, need %u; keep waiting...", length, SXE_BUF_USED(this),
                SXE_LITERAL_LENGTH(TEST_HTTP_REQUEST));
        goto SXE_EARLY_OUT;
    }

    pipe_client = sxe_new_pipe(NULL, pipe_name, test_event_proxy_pipe_connected, NULL, test_event_proxy_pipe_closed);
    SXEA10I(pipe_client != NULL, "Proxy failed to allocate a pipe client");
    SXE_USER_DATA(pipe_client) = this;
    SXEA10I(sxe_connect_pipe(pipe_client) == SXE_RETURN_OK, "Proxy failed to connect pipe client to HTTPD");

SXE_EARLY_OUT:
    SXER60I("return");
}

static void
test_event_client_read(SXE * this, int length)
{
    SXEE61I("test_event_client_read(length=%d)", length);
    SXE_UNUSED_PARAMETER(length);

    if (SXE_BUF_USED(this) < SXE_LITERAL_LENGTH(TEST_HTTP_RESPONSE)) {
        SXEL23I("test_event_client_read: Received %u bytes of %u, need %u; keep waiting...", length, SXE_BUF_USED(this),
                SXE_LITERAL_LENGTH(TEST_HTTP_RESPONSE));
        goto SXE_EARLY_OUT;
    }

    tap_ev_push("test_event_client_read", 2, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)), "used", SXE_BUF_USED(this));

SXE_EARLY_OUT:
    SXER60I("return");
}

int
main(void)
{
    SXE_HTTPD          httpd;
    SXE_HTTPD_REQUEST * request;
    tap_ev             ev;

    plan_tests(9);
    signal(SIGPIPE, SIG_IGN);

    sxe_register(4, 0);        /* http listener and connections */
    sxe_register(8, 0);        /* http clients */
    SXEA10(sxe_init() == SXE_RETURN_OK, "Couldn't initialized SXE");

    /* Set up the HTTP daemon on a pipe
     */
    snprintf(&pipe_name[sizeof(TEST_PIPE_NAME_PREFIX) - 1], TEST_PID_MAX_DIGITS + 1, "%d", getpid());
    (void)unlink(pipe_name);

    sxe_httpd_construct(&httpd, 2, 10, 512, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, connect, evhttp_connect);
    SXE_HTTPD_SET_HANDLER(&httpd, request, evhttp_request);
    SXE_HTTPD_SET_HANDLER(&httpd, respond, evhttp_respond);
    SXE_HTTPD_SET_HANDLER(&httpd, close,   evhttp_close);

    ok((pipe_listener = sxe_httpd_listen_pipe(&httpd, pipe_name)) != NULL,                 "Listened on pipe %s", pipe_name);
    httpd.user_data   = pipe_listener;

    /* Set up the TCP listener for the TCP proxy (e.g. Apache on the SWA)
     */
    SXEA10((tcp_listener = sxe_new_tcp(NULL, "0.0.0.0", 0, NULL, test_event_proxy_tcp_read, NULL)) != NULL,
           "Couldn't allocate a SXE for the proxy");
    SXEA10(sxe_listen(tcp_listener) == SXE_RETURN_OK, "Couldn't listen on TCP SXE");

    /* Set up the TCP client (e.g. remote endpoint computer)
     */
    SXEA10((tcp_client = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_client_connected, test_event_client_read, NULL)) != NULL,
           "Couldn't allocate a SXE for the client");
    SXEA10(sxe_connect(tcp_client, "127.0.0.1", SXE_LOCAL_PORT(tcp_listener)) == SXE_RETURN_OK, "Failed to connect from client");

    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "evhttp_connect",                   "HTTPD got connection");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "evhttp_request",                   "HTTPD got request event");
    is_strncmp(TEST_URL, (const char *)tap_ev_arg(ev, "url"), tap_ev_arg(ev, "url_len"),   "Got URL '" TEST_URL "'");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "evhttp_respond",                   "HTTPD ready for response");
    request = SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(ev, "request"));
    sxe_httpd_response_simple(request, evhttp_sent, NULL, 200, "OK", "Pong!\r\n", NULL);
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "evhttp_sent",                      "HTTPD finished responding");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_event_client_read",           "Client got read event");
    is_strncmp(tap_ev_arg(ev, "buf"), TEST_HTTP_RESPONSE, tap_ev_arg(ev, "used"),          "Got response '" TEST_HTTP_RESPONSE "'");
    sxe_close(tcp_client);
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "evhttp_close",                     "HTTPD got close event");

    sxe_close(pipe_listener);
    (void)unlink(pipe_name);
    return exit_status();
}

#endif
