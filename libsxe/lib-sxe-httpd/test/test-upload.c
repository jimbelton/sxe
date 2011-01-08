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

static void
http_handler(SXE_HTTP_REQUEST *request, SXE_HTTPD_CONN_STATE state, char *chunk, size_t chunk_len, void *user)
{
    SXE *this = request->sxe;
    int done = 0;

    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(state);
    SXE_UNUSED_PARAMETER(chunk);
    SXE_UNUSED_PARAMETER(chunk_len);
    SXE_UNUSED_PARAMETER(user);

    SXEE65I("http_handler(request=%p,state=%u,chunk=%p,chunk_len=%u,user=%p)", request, state, chunk, chunk_len, user);

    if (chunk_len != 0) {
        SXEL61I("ignoring callback; chunk_len is %u", chunk_len);
        SXEL63I("Data: %d:[%.*s]", chunk_len, chunk_len, chunk);
        goto SXE_EARLY_OUT;
    }

    if (state != SXE_HTTPD_CONN_REQ_RESPONSE) {
        SXEL61I("ignoring callback; state is %u", state);
        goto SXE_EARLY_OUT;
    }

    SXEL63I("URL: %d:[%.*s]", SXE_HTTP_REQUEST_URL_LEN(request), SXE_HTTP_REQUEST_URL_LEN(request), SXE_HTTP_REQUEST_URL(request));
    if (1 == SXE_HTTP_REQUEST_URL_LEN(request))
        done = 1;

    sxe_httpd_response_simple(request, 200, "OK", "");

    if (done) {
        sxe_httpd_response_close(request);
        SXEL11I("sxe_close()ing listen socket %p", request->server->user_data);
        sxe_close(request->server->user_data);
    }

SXE_EARLY_OUT:
    SXER60I("return");
}

static void
handle_sendfile(SXE * this, SXE_RETURN final)
{
    SXE_UNUSED_PARAMETER(final);
    SXEL41I("closing fd %d", this->sendfile_in_fd);
    close(this->sendfile_in_fd);
    SXE_WRITE_LITERAL(this,
                      "GET / HTTP/1.1\r\n"
                      "Host: foobar\r\n"
                      "Connection: close\r\n\r\n");
}

static void
handle_connect(SXE * this)
{
    int fd = open("../sxe-httpd.c", O_RDONLY);
    struct stat st;
    char buf[1024];
    size_t len;

    fstat(fd, &st);
    len = snprintf(buf, sizeof buf,
            "POST /sxe-httpd.c HTTP/1.1\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %lu\r\n\r\n", st.st_size);

    sxe_write(this, buf, len);
#ifndef WINDOWS_NT
    sxe_sendfile(this, fd, (unsigned)st.st_size, handle_sendfile);
#endif
}

static void
handle_read(SXE * this, int length)
{
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(length);
    ok(SXE_BUF_USED(this) >= 38, "post response >= 38: %d", SXE_BUF_USED(this));
    is_strncmp(SXE_BUF(this), "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n", 38, "post response 200 OK");
    sxe_buf_consume(this, 38);
    ok(SXE_BUF_USED(this) >= 38, "post response >= 38: %d", SXE_BUF_USED(this));
    is_strncmp(SXE_BUF(this), "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n", 38, "post response 200 OK");
    sxe_buf_consume(this, 38);
    sxe_close(this);
}

static void
handle_close(SXE * this)
{
    SXE_UNUSED_PARAMETER(this);
}

int
main(void)
{
#ifdef WINDOWS_NT
    plan_skip_all("No sendfile on windows");
#else
    SXE_HTTPD httpd;
    SXE *listener;
    SXE *c;

    plan_tests(4);

    sxe_register(4, 0);        /* http listener and connections */
    sxe_register(8, 0);        /* http clients */
    sxe_init();

    sxe_httpd_construct(&httpd, 3, http_handler, 0);
    listener = sxe_httpd_listen(&httpd, "0.0.0.0", 0);
    httpd.user_data = listener;

    c = sxe_new_tcp(NULL, "0.0.0.0", 0, handle_connect, handle_read, handle_close);
    sxe_connect(c, "127.0.0.1", SXE_LOCAL_PORT(listener));

    ev_loop(ev_default_loop(EVFLAG_AUTO), 0);
#endif

    return exit_status();
}
