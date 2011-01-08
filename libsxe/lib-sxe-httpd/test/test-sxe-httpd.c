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

#include "tap.h"
#include "sxe-util.h"
#include "sxe-httpd.h"

#ifdef WINDOWS_NT
int
main(void)
{
    plan_skip_all("TODO: implement sxe_sendfile on windows");
    return exit_status();
}
#else

static unsigned request_sizes[6] = {54, 0, 49, 54, 54, 36};

static void
test_event_sendfile_done(SXE_HTTP_REQUEST * request, SXE_RETURN final_result, void * user)
{
    SXE_UNUSED_PARAMETER(final_result);

    SXEE60(__func__);
    sxe_httpd_response_end(request);
    close((int)user);
    SXER60("return");
}

static void
test_event_httpd(SXE_HTTP_REQUEST * request, SXE_HTTPD_CONN_STATE state, char *chunk, size_t chunk_len, void * user)
{
    SXE * this = request->sxe;

    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(state);
    SXE_UNUSED_PARAMETER(chunk);
    SXE_UNUSED_PARAMETER(chunk_len);
    SXE_UNUSED_PARAMETER(user);

    SXEE65I("test_event_httpd(request=%p,state=%u,chunk=%p,chunk_len=%u,user=%p)", request, state, chunk, chunk_len, user);

    if (chunk_len != 0) {
        SXEL61I("ignoring callback; chunk_len is %u", chunk_len);
        SXEL63I("Data: %d:[%.*s]", chunk_len, chunk_len, chunk);
        goto SXE_EARLY_OUT;
    }

    if (state != SXE_HTTPD_CONN_REQ_RESPONSE) {
        SXEL61I("ignoring callback; state is %u", state);
        goto SXE_EARLY_OUT;
    }

    SXEL62I("URL: [%.*s]", SXE_HTTP_REQUEST_URL_LEN(request), SXE_HTTP_REQUEST_URL(request));

    if (0 == strncasecmp(SXE_HTTP_REQUEST_URL(request), "/r1", 3)) {
        const SXE_HTTP_HEADER *h = sxe_httpd_get_header_in(request, "host");

        ok(h != NULL, "/r1: 'Host' header is present");
        is(SXE_HTTP_HEADER_VAL_LEN(h), 3, "/r1: 'Host' header is 3 bytes long");
        is(strncasecmp(SXE_HTTP_HEADER_VAL(request, h), "foo", 3), 0, "/r1: 'Host' header is 'foo'");

        sxe_httpd_set_header_out(request, "Content-Type", "text/html", 0);
        sxe_httpd_response_start(request, 200, "OK");
        sxe_httpd_response_chunk(request, "r1:hello\r\n", 7);
        sxe_httpd_response_end(request);
        sxe_httpd_response_close(request);
        goto SXE_EARLY_OUT;
    }

    if (0 == strncasecmp(SXE_HTTP_REQUEST_URL(request), "/r3", 3)) {
        const SXE_HTTP_HEADER *header;
        int fd = open("sxe-httpd-proto.h", O_RDONLY);
        struct stat st;
        fstat(fd, &st);

        sxe_httpd_set_content_length(request, st.st_size);
        sxe_httpd_set_header_out(request, "Charlie", "Horse", 5);

        header = sxe_httpd_get_header_out(request, "CHarLIE");
        is_strncmp(SXE_HTTP_HEADER_VAL(request, header), "Horse", 5, "'Charlie' header is correct (Horse)");

        request_sizes[1] = st.st_size + 57;

        sxe_httpd_response_start(request, 200, "OK");
        sxe_httpd_response_sendfile(request, fd, st.st_size, test_event_sendfile_done, (void *)fd);
        goto SXE_EARLY_OUT;
    }

    if (0 == strncasecmp(SXE_HTTP_REQUEST_URL(request), "/r4", 3)) {
        SXEL61I("got content-length ", request->in_content_length);
        sxe_httpd_response_simple(request, 200, "OK", "c4:gotit\r\n");
        goto SXE_EARLY_OUT;
    }

    if (0 == strncasecmp(SXE_HTTP_REQUEST_URL(request), "/stop", 5)) {
        sxe_httpd_response_start(request, 400, "Bad request");
        sxe_httpd_response_chunk(request, "<gone/>\r\n",8);
        sxe_httpd_response_end(request);
        sxe_httpd_response_close(request);
        sxe_close(user); /* the listener */
        goto SXE_EARLY_OUT;
    }

    sxe_httpd_response_simple(request, 404, "Not found", "<nope/>\r\n");

SXE_EARLY_OUT:
    SXER60I("return");
}

static void
test_event_c3_connect(SXE * this)
{
    SXEE60I(__func__);
    sxe_close(this);
    SXER60I("return");
}

static void
test_event_c1_response(SXE * this, int length)
{
    SXEE60I(__func__);
    SXE_UNUSED_PARAMETER(length);
    is(SXE_BUF_USED(this), 51, "c1: sxe buf used is %d (sb 51)", SXE_BUF_USED(this));
    is_strncmp(SXE_BUF(this), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nr1:hello\r\n", 51, "c1: response is 'r1:hello'");
    sxe_buf_clear(this);
    SXER60I("return");
}

static void
test_event_c2_response(SXE * this, int length)
{
    SXEE60I(__func__);
    SXE_UNUSED_PARAMETER(length);

    if (SXE_BUF_USED(this) < request_sizes[SXE_USER_DATA_AS_INT(this)]) {
        return; /* wait until our entire response has arrived */
    }

    switch (SXE_USER_DATA_AS_INT(this)++) {
    case 0:
        is(SXE_BUF_USED(this), 54, "c2: sxe buf used is %d (sb 54)", SXE_BUF_USED(this));
        is_strncmp(SXE_BUF(this), "HTTP/1.1 404 Not found\r\nContent-Length: 9\r\n\r\n<nope/>\r\n", 54, "c2: 404 not found");
        sxe_buf_clear(this);
        SXE_WRITE_LITERAL(this, "GET /r3 HTTP/1.1\r\n\r\n");
        break;
    case 1:
        is(SXE_BUF_USED(this), request_sizes[1], "c3: sxe buf used is %d", SXE_BUF_USED(this));
        sxe_buf_clear(this);
        SXE_WRITE_LITERAL(this, "POST /r4 HTTP/1.1\r\nContent-Length: 10\r\n\r\n1234567890");
        break;
    case 2:
        is(SXE_BUF_USED(this), 49, "c4: sxe buf used is %d (sb 49)", SXE_BUF_USED(this));
        is_strncmp(SXE_BUF(this), "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nc4:gotit\r\n", 49, "c4: response is 'c4:gotit'");
        sxe_buf_clear(this);
        SXE_WRITE_LITERAL(this, "GET /r5 HTTP/1.1\r\n\r\nGET /r6 HTTP/1.1\r\n\r\n");
        break;
    case 3:
        ok(SXE_BUF_USED(this) >= 54, "c5: sxe buf used %d (>= 54)", SXE_BUF_USED(this));
        is_strncmp(SXE_BUF(this), "HTTP/1.1 404 Not found\r\nContent-Length: 9\r\n\r\n<nope/>\r\n", 54, "c5: 404 not found");
        sxe_buf_consume(this, 54);
        sxe_buf_resume(this);
        break;
    case 4:
        is(SXE_BUF_USED(this), 54, "c6: sxe buf used is %d (sb 54)", SXE_BUF_USED(this));
        is_strncmp(SXE_BUF(this), "HTTP/1.1 404 Not found\r\nContent-Length: 9\r\n\r\n<nope/>\r\n", 54, "c6: 404 not found");
        sxe_buf_consume(this, 54);
        sxe_buf_resume(this);
        SXE_WRITE_LITERAL(this, "GET /stop HTTP/1.1\r\n\r\n");
        break;
    case 5:
        is(SXE_BUF_USED(this), 36, "stop: sxe buf used is %d (sb 36)", SXE_BUF_USED(this));
        is_strncmp(SXE_BUF(this), "HTTP/1.1 400 Bad request\r\n\r\n<gone/>\r\n", 36, "stop: 400 bad request");
        sxe_close(this);
        break;
    }

    SXER60I("return");
}

int
main(void)
{
    SXE_HTTPD httpd;
    SXE *listener;
    SXE *c1, *c2, *c3;

    plan_tests(18);

    sxe_register(4, 0);        /* http listener and connections */
    sxe_register(8, 0);        /* http clients */
    SXEA10(sxe_init() == SXE_RETURN_OK, "Couldn't initialized SXE");

    sxe_httpd_construct(&httpd, 3, test_event_httpd, 0);
    ok((listener = sxe_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL, "HTTPD listening");
    httpd.user_data = listener;

    SXEA10((c1 = sxe_new_tcp(NULL, "0.0.0.0", 0, NULL, test_event_c1_response, NULL)) != NULL, "Failed to allocate client SXE c1");
    sxe_connect(c1, "127.0.0.1", SXE_LOCAL_PORT(listener));
    SXEA10(SXE_WRITE_LITERAL(c1, "GET /r1 HTTP/1.1\r\nHost: foo\r\n\r\n") == SXE_RETURN_OK,    "Failed to write request to c1");

    SXEA10((c2 = sxe_new_tcp(NULL, "0.0.0.0", 0, NULL, test_event_c2_response, NULL)) != NULL, "Failed to allocate client SXE c2");
    sxe_connect(c2, "127.0.0.1", SXE_LOCAL_PORT(listener));
    SXEA10(SXE_WRITE_LITERAL(c2, "GET /r2 HTTP/1.1\r\n\r\n")              == SXE_RETURN_OK,    "Failed to write request to c2");

    SXEA10((c3 = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_c3_connect, NULL, NULL)) != NULL,  "Failed to allocate client SXE c3");
    sxe_connect(c3, "127.0.0.1", SXE_LOCAL_PORT(listener));

    signal(SIGPIPE, SIG_IGN);
    ev_loop(ev_default_loop(EVFLAG_AUTO), 0);
    return exit_status();
}
#endif

