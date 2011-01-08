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

#include <stdlib.h>
#include <string.h>

#include "sxe-httpd.h"
#include "sxe-log.h"
#include "sxe-pool.h"

#define SXE_HTTPD_SERVERNAME "sxe-httpd/1.0"

static void
sxe_httpd_clear_request(SXE_HTTP_REQUEST *request)
{
    SXE *this = request->sxe;

    SXE_UNUSED_PARAMETER(this);

    SXEE81I("sxe_httpd_clear_request(request=%p)", request);
    request->in_content_length = 0;
    request->in_content_seen = 0;
    request->in_len = 0;
    request->method_len = 0;
    request->url_off = 0;
    request->url_len = 0;
    request->version_off = 0;
    request->version_len = 0;
    request->headers_off = 0;
    request->headers_len = 0;
    request->out_len = 0;

    while (!SXE_LIST_IS_EMPTY(&request->in_headers)) {
        SXE_HTTP_HEADER *h = sxe_list_pop(&request->in_headers);
        sxe_list_unshift(&request->server->h_freelist, h);
    }

    while (!SXE_LIST_IS_EMPTY(&request->out_headers)) {
        SXE_HTTP_HEADER *h = sxe_list_pop(&request->out_headers);
        sxe_list_unshift(&request->server->h_freelist, h);
    }

    SXER80I("return");
}

void
sxe_httpd_response_close(SXE_HTTP_REQUEST * request)
{
    SXE *              this     = request->sxe;
    SXE_HTTP_REQUEST * requests = request->server->requests;
    unsigned           state    = sxe_pool_index_to_state(requests, request - requests);

    SXE_UNUSED_PARAMETER(this);

    SXEE81I("sxe_httpd_response_close(request=%p)", request);
    sxe_httpd_clear_request(request);

    if (request->sxe) {
        sxe_close(request->sxe);
    }

    sxe_pool_set_indexed_element_state(requests, request - requests, state, SXE_HTTPD_CONN_FREE);
    SXER80I("return");
}

static void
sxe_httpd_error_close(SXE_HTTP_REQUEST * request, int code, const char *status)
{
    SXE * this = request->sxe;
    char  body[1024];

    SXE_UNUSED_PARAMETER(this);
    SXEE83I("sxe_httpd_error_close(request=%p,code=%d,status=%s)", request, code, status);

    snprintf(body, sizeof body,
            "<html>%d %s</html>\r\n", code, status);

    sxe_httpd_set_header_out(request, "Server", SXE_HTTPD_SERVERNAME, sizeof(SXE_HTTPD_SERVERNAME)-1);
    sxe_httpd_set_header_out(request, "Connection", "close", 0);
    sxe_httpd_set_header_out(request, "Content-Type", "text/html; charset=\"UTF-8\"", 0);
    sxe_httpd_response_simple(request, code, status, body);
    sxe_httpd_response_close(request);

    SXER80I("return");
}

static void
sxe_httpd_event_timeout(void * array, unsigned i, void * user_info)    /* COVERAGE EXCLUSION : TODO - test timeouts */
{                                                                      /* COVERAGE EXCLUSION : TODO - test timeouts */
    SXE_HTTP_REQUEST * request = ((SXE_HTTP_REQUEST *)array) + i;      /* COVERAGE EXCLUSION : TODO - test timeouts */
    SXE *this = request->sxe;                                          /* COVERAGE EXCLUSION : TODO - test timeouts */
    unsigned state = sxe_pool_index_to_state(array, i);                /* COVERAGE EXCLUSION : TODO - test timeouts */

    SXE_UNUSED_PARAMETER(this);                                        /* COVERAGE EXCLUSION : TODO - test timeouts */
    SXE_UNUSED_PARAMETER(user_info);                                   /* COVERAGE EXCLUSION : TODO - test timeouts */

    SXEE83I("sxe_httpd_event_timeout(array=%p,i=%u,user_info=%p)", array, i, user_info)
    SXEL51I("timed out in state %d", state);                           /* COVERAGE EXCLUSION : TODO - test timeouts */

    if (request->server->handler) {                                    /* COVERAGE EXCLUSION : TODO - test timeouts */
        request->server->handler(request, SXE_HTTPD_CONN_FREE, NULL, 0, request->server->user_data);    /* COVERAGE EXCLUSION : TODO - test timeouts */
    }

    sxe_httpd_response_close(request);                                 /* COVERAGE EXCLUSION : TODO - test timeouts */

    SXER80I("return");
}                                                                      /* COVERAGE EXCLUSION : TODO - test timeouts */

static void
sxe_httpd_event_connect(SXE * this)
{
    SXE_HTTPD * self = SXE_USER_DATA(this);
    SXE_HTTP_REQUEST * request;
    unsigned id;

    SXE_UNUSED_PARAMETER(this);
    SXEE81I("sxe_httpd_event_connect(this=%p)", this);

    id = sxe_pool_set_oldest_element_state(self->requests, SXE_HTTPD_CONN_FREE, SXE_HTTPD_CONN_IDLE);

    if (id == SXE_POOL_NO_INDEX) {
        SXEL20I("httpd_connect: no free connections, returning 503 error");
        SXE_WRITE_LITERAL(this, "HTTP/1.1 503 Service unavailable\r\n" "Connection: close\r\n\r\n");
        sxe_close(this);
        goto SXE_EARLY_OUT;
    }

    request = self->requests + id;
    memset(request, '\0', sizeof *request);
    SXE_USER_DATA(this) = request;
    request->sxe = this;
    request->server = self;

    if (self->handler) {
        self->handler(request, SXE_HTTPD_CONN_IDLE, NULL, 0, self->user_data);
    }

SXE_EARLY_OUT:
    SXER80I("return");
}

static void
sxe_httpd_event_close(SXE * this)
{
    SXE_HTTP_REQUEST * request = SXE_USER_DATA(this);
    SXEE81I("sxe_httpd_event_close(this=%p)", this);
    request->sxe = NULL;
    sxe_httpd_response_close(request);
    SXER80I("return");
}

static const struct {
    const char *name;
    unsigned length;
    SXE_HTTP_METHOD method;
} methods[] = {
    { "GET",    3, SXE_HTTP_METHOD_GET    },
    { "PUT",    3, SXE_HTTP_METHOD_PUT    },
    { "HEAD",   4, SXE_HTTP_METHOD_HEAD   },
    { "POST",   4, SXE_HTTP_METHOD_POST   },
    { "DELETE", 6, SXE_HTTP_METHOD_DELETE },
    { NULL,     0, SXE_HTTP_METHOD_GET    }
};

static bool
sxe_httpd_parse_request_line(SXE_HTTP_REQUEST * request, const char * start, int length)
{
    SXE * this             = request->sxe;
    bool  request_is_valid = false;
    int   i;

    SXE_UNUSED_PARAMETER(this);
    SXEE83I("sxe_httpd_parse_request_line(request=%p,start=%p,length=%d)", request, start, length);

    if (length == 0) {
        goto SXE_EARLY_OUT;                            /* COVERAGE EXCLUSION: TODO: Test me! */
    }

    /* Look for the request */
    for (i = 0; i < length && !strchr(" \t", start[i]); ++i) {
    }

    if (i == length) {
        goto SXE_EARLY_OUT;
    }

    request->method_len = i;
    SXEL93I("method: %u[%.*s]", i, i, start);

    /* Whitespace */
    for (; i < length && strchr(" \t", start[i]); ++i) {
    }

    if (i == length) {
        goto SXE_EARLY_OUT;                            /* COVERAGE EXCLUSION: TODO: Test me! */
    }

    /* Look for the URL */
    request->url_off = i;

    for (; i < length && !strchr(" \t", start[i]); ++i) {
    }

    if (i == length) {
        goto SXE_EARLY_OUT;
    }

    request->url_len = i - request->url_off;
    SXEL93I("url: %u[%.*s]", request->url_len, request->url_len, start + request->url_off);

    /* Skip ahead */
    for (; i < length && strchr(" \t", start[i]); ++i) {
    }

    if (i == length) {
        goto SXE_EARLY_OUT;                            /* COVERAGE EXCLUSION: TODO: Test me! */
    }

    /* Look for the HTTP version */
    request->version_off = i;

    for (; i < length && !strchr(" \t", start[i]); ++i) {
    }

    if (i != length) {
        goto SXE_EARLY_OUT;                            /* COVERAGE EXCLUSION: TODO: Test me! */
    }

    request->version_len = i - request->version_off;
    SXEL93I("httpver: %u[%.*s]", request->version_len, request->version_len, start + request->version_off);

    /* Validate the method name.
     */
    for (i = 0; methods[i].name; i++) {
        if (request->method_len == methods[i].length && 0 == strncmp(start, methods[i].name, methods[i].length)) {
            request->method = methods[i].method;
            request_is_valid = true;
            SXEL91I("valid verb: %u\n", request->method);
            goto SXE_EARLY_OUT;
        }
    }

SXE_EARLY_OUT:
    SXER81I("return request_is_valid=%s", SXE_BOOL_TO_STR(request_is_valid));
    return request_is_valid;
}

#define VALID_HEADER_CHARACTER(c) ( ((c) >= '0' && (c) <= '9') \
                                 || ((c) >= 'A' && (c) <= 'Z') \
                                 || ((c) >= 'a' && (c) <= 'z') \
                                 || ((c) == '-') )

static bool
sxe_httpd_parse_headers(SXE_HTTP_REQUEST *request)
{
    SXE *this = request->sxe;
    bool              headers_are_good = false;
    SXE_HTTP_HEADER * h;
    const char *ptr = request->in_buf + request->headers_off;
    unsigned length = request->headers_len;
    unsigned i = 0;
    const char *key, *val;
    unsigned key_len, val_len;

    SXE_UNUSED_PARAMETER(this);
    SXEE81I("sxe_httpd_parse_headers(request=%p)", request);

    while (i < length) {
        SXEL92I("sxe_httpd_parse_headers: i=%d length=%d", i, length);

        SXEL92I("sxe_httpd_parse_headers:      header block: [%.*s]", length - i, &ptr[i]);
        SXEL92I("sxe_httpd_parse_headers:     skipping CRLF: offset=%d (%c)", i, ptr[i]);
        for (; i < length && strchr("\r\n", ptr[i]); i++);
        if (i == length) break;
        SXEL92I("sxe_httpd_parse_headers:                  : offset=%d (%c)", i, ptr[i]);

        key = &ptr[i];

        if (!VALID_HEADER_CHARACTER(ptr[i])) {
            goto SXE_EARLY_OUT;
        }

        SXEL92I("sxe_httpd_parse_headers: looking for colon: offset=%d (%c)", i, ptr[i]);

        for (; i < length && ptr[i] != ':'; i++) {
        }

        if (i == length) {
            goto SXE_EARLY_OUT;
        }

        SXEL92I("sxe_httpd_parse_headers:                  : offset=%d (%c)", i, ptr[i]);
        key_len = &ptr[i] - key;

        if (key_len == 0) {
            goto SXE_EARLY_OUT;    /* COVERAGE EXCLUSION: TODO: Need a test case for this failure condition */
        }

        SXEL92I("sxe_httpd_parse_headers:        key length: offset=%d (%d)", i, key_len);
        i++;

        SXEL92I("sxe_httpd_parse_headers:   skipping SP-TAB: offset=%d (%c)", i, ptr[i]);

        for (; i < length && strchr(" \t", ptr[i]); i++) {
        }

        if (i == length) {
            goto SXE_EARLY_OUT;
        }

        SXEL92I("sxe_httpd_parse_headers:                  : offset=%d (%c)", i, ptr[i]);

        val = &ptr[i];
        SXEL92I("sxe_httpd_parse_headers:  looking for CRLF: offset=%d (%c)", i, ptr[i]);

        for (; i < length && !strchr("\r\n", ptr[i]); i++) {
        }

        val_len = &ptr[i] - val;
        SXEL91I("sxe_httpd_parse_headers:              end : offset=%d", i);
        SXEL92I("sxe_httpd_parse_headers:      value length: offset=%d (%d)", i, key_len);
        SXEL92I("sxe_httpd_parse_headers:            header: %.*s", val + val_len - key, key);

        h = sxe_list_shift(&request->server->h_freelist);
        h->is_response = 0;
        h->key_off = key - request->in_buf;
        h->key_len = key_len;
        h->val_off = val - request->in_buf;
        h->val_len = val_len;
        SXEL98I("pushing new header: k=[%d@%d:%.*s] v=[%d@%d:%.*s]",
                h->key_len, h->key_off, h->key_len, SXE_HTTP_HEADER_KEY(request, h),
                h->val_len, h->val_off, h->val_len, SXE_HTTP_HEADER_VAL(request, h));
        sxe_list_push(&request->in_headers, h);

        /* Is this the content-length header?
         */
        if (SXE_HTTP_HEADER_EQUALS(request, h, "Content-Length")) {
            const char *p = val;
            const char *e = val + val_len;

            while (p < e) {
                request->in_content_length = request->in_content_length * 10 + *p++ - '0';
            }

            SXEL91I("content-length: %u", request->in_content_length);
        }
    }

    SXEL90I("sxe_httpd_parse_headers: done");
    headers_are_good = true;

SXE_EARLY_OUT:
    SXER81I("return headers_are_good=%s", SXE_BOOL_TO_STR(headers_are_good));
    return headers_are_good;
}

static void
sxe_httpd_event_read(SXE * this, int additional_length)
{
    SXE_HTTP_REQUEST * request = SXE_USER_DATA(this);
    SXE_HTTPD        * server  = request->server;
    SXE_HTTP_REQUEST * reqpool = server->requests;
    unsigned           state;
    char             * start;
    char             * end;
    unsigned           length;
    unsigned           consumed;
    int                response_status_code = 0;
    const char       * response_reason      = "";

    SXEE82I("sxe_httpd_event_read(this=%p,additional_length=%d)", this, additional_length);
    SXE_UNUSED_PARAMETER(additional_length);

    state    = sxe_pool_index_to_state(reqpool, request - reqpool);
    consumed = SXE_BUF_USED(this);    /* by default, we consume the whole buffer */

    if (state != SXE_HTTPD_CONN_REQ_BODY) {
        size_t bytes = SXE_BUF_USED(this);

        if (request->in_len + bytes > sizeof(request->in_buf)) {
            bytes = sizeof(request->in_buf) - request->in_len;
        }

        memcpy(request->in_buf + request->in_len, SXE_BUF(this), bytes);
        request->in_len += bytes;
    }

    switch (state) {
    case SXE_HTTPD_CONN_IDLE:
        SXEL90I("state IDLE -> LINE");
        sxe_pool_set_indexed_element_state(reqpool, request - reqpool, state, SXE_HTTPD_CONN_REQ_LINE);
        state = SXE_HTTPD_CONN_REQ_LINE;
        /* fall through */

    case SXE_HTTPD_CONN_REQ_LINE:
        start  = request->in_buf;
        length = request->in_len;
        end    = sxe_strncspn(start, "\r\n", length);

        if (end == NULL) {
            /* TODO: If buffer is full, send a 400 (request line can't be 1500+ bytes long) */
            goto SXE_EARLY_OUT;    /* COVERAGE EXCLUSION: TODO: Test this case */
        }

        if (!sxe_httpd_parse_request_line(request, start, end - start)) {
            response_status_code = 400;
            response_reason = "Bad request";
            goto SXE_EARLY_OUT;
        }

        request->headers_off = end - start; /* the \r\n */
        SXEL90I("state LINE -> REQ_HEADERS");
        sxe_pool_set_indexed_element_state(reqpool, request - reqpool, state, SXE_HTTPD_CONN_REQ_HEADERS);
        state = SXE_HTTPD_CONN_REQ_HEADERS;
        /* fall through */

    case SXE_HTTPD_CONN_REQ_HEADERS:
        start  = request->in_buf + request->headers_off;
        length = request->in_len - request->headers_off;
        end    = sxe_strnstr(start, "\r\n\r\n", length);

        if (end == NULL) {
            if (request->in_len >= sizeof(request->in_buf)) {
                response_status_code = 413;
                response_reason = "Request entity too large";
            }

            SXEL90I("Haven't found the end of the headers yet");
            goto SXE_EARLY_OUT;
        }

        SXEL90I("Found the end of the headers");
        request->in_content_length = 0;
        request->headers_len       = end - start;

        if (!sxe_httpd_parse_headers(request)) {
            response_status_code = 400;
            response_reason = "Bad request";
            goto SXE_EARLY_OUT;
        }

        request->in_content_seen = 0;
        SXEL90I("state REQ_HEADERS -> REQ_BODY");
        sxe_pool_set_indexed_element_state(reqpool, request - reqpool, state, SXE_HTTPD_CONN_REQ_BODY);
        state = SXE_HTTPD_CONN_REQ_BODY;

        /* calculate how much of what we copied was part of the headers, and
         * consume that. */
        end += strlen("\r\n\r\n");
        consumed = SXE_BUF_USED(this) - (request->in_buf + request->in_len - end);
        SXEA93I(consumed != 0, "consumed is zero??? used=%u in_buf+in_len=%p end=%p", SXE_BUF_USED(this), request->in_buf + request->in_len, end);
        sxe_buf_consume(this, consumed);
        /* fall through */

    case SXE_HTTPD_CONN_REQ_BODY:
        consumed = 0;

        if (request->in_content_length && SXE_BUF_USED(this)) {
            unsigned data_len = SXE_BUF_USED(this);

            if (data_len > request->in_content_length - request->in_content_seen) {
                data_len = request->in_content_length - request->in_content_seen;
            }

            if (server->handler) {
                SXEL94I("c/l %u - body chunk %d:%.*s", request->in_content_length, data_len, data_len, SXE_BUF(this));
                server->handler(request, state, SXE_BUF(this), data_len, server->user_data);
            }

            sxe_buf_consume(this, data_len);
            request->in_content_seen += data_len;
        }

        if (request->in_content_length > request->in_content_seen) {
            goto SXE_EARLY_OUT;
        }

        if (server->handler) {    /* TODO: Why is handler ever NULL? */
            SXEL90I("state REQ_BODY -> REQ_RESPONSE");
            sxe_pool_set_indexed_element_state(reqpool, request - reqpool, state, SXE_HTTPD_CONN_REQ_RESPONSE);
            state = SXE_HTTPD_CONN_REQ_RESPONSE;
            server->handler(request, state, NULL, 0, server->user_data);
            goto SXE_SUCCESS_OUT;
        }

        /* fall through */

    case SXE_HTTPD_CONN_REQ_RESPONSE:
        SXEA10(0, "Internal error: state RESPONSE called during a read???");    /* COVERAGE EXCLUSION: Can't happen */
    }

SXE_EARLY_OUT:
    sxe_buf_clear(this);
    sxe_buf_resume(this);

    if (response_status_code != 0) {
        sxe_httpd_error_close(request, response_status_code, response_reason);
    }

SXE_SUCCESS_OUT:
    SXER80I("return");
}

const SXE_HTTP_HEADER *
sxe_httpd_get_header_in(SXE_HTTP_REQUEST *request, const char *header)
{
    SXE *this = request->sxe;
    SXE_LIST_WALKER walker;
    SXE_HTTP_HEADER *h = NULL;

    SXE_UNUSED_PARAMETER(this);
    SXEE82I("sxe_httpd_get_header_in(request=%p,header=%s)", request, header);
    sxe_list_walker_construct(&walker, &request->in_headers);

    while ((h = sxe_list_walker_step(&walker)) != NULL) {
        if (SXE_HTTP_HEADER_EQUALS(request, h, header)) {
            goto SXE_EARLY_OUT;
        }
    }

SXE_EARLY_OUT:
    SXER81I("return %p", h);
    return h;
}

const SXE_HTTP_HEADER *
sxe_httpd_get_header_out(SXE_HTTP_REQUEST *request, const char *header)
{
    SXE *this = request->sxe;
    SXE_LIST_WALKER walker;
    SXE_HTTP_HEADER *h = NULL;

    SXE_UNUSED_PARAMETER(this);
    SXEE82I("sxe_httpd_get_header_out(request=%p,header=%s)", request, header);
    sxe_list_walker_construct(&walker, &request->out_headers);

    while ((h = sxe_list_walker_step(&walker)) != NULL) {
        SXEL94I("header: %.*s (%.*s)",
                SXE_HTTP_HEADER_KEY_LEN(h),
                SXE_HTTP_HEADER_KEY(request, h),
                SXE_HTTP_HEADER_VAL_LEN(h),
                SXE_HTTP_HEADER_VAL(request, h));
        if (SXE_HTTP_HEADER_EQUALS(request, h, header)) {
            goto SXE_EARLY_OUT;
        }
    }

SXE_EARLY_OUT:
    SXER81I("return %p", h);
    return h;
}

void
sxe_httpd_set_header_out(SXE_HTTP_REQUEST *request, const char *header, const char *value, unsigned length)
{
    SXE *this = request->sxe;
    SXE_HTTP_HEADER *h;

    SXE_UNUSED_PARAMETER(this);
    SXEE84I("sxe_httpd_set_header_out(request=%p,header=%s,value=%.*s)", request, header, length, value);

    h = sxe_list_shift(&request->server->h_freelist);
    SXEA10I(h != NULL, "Got invalid header");
    h->is_response = 1;
    h->key_off = request->out_len;
    h->key_len = strlen(header);
    memcpy(request->out_buf + request->out_len, header, h->key_len);
    request->out_len += h->key_len;

    memcpy(request->out_buf + request->out_len, ": ", 2);
    request->out_len += 2;

    h->val_off = request->out_len;
    h->val_len = length ? length : strlen(value);
    memcpy(request->out_buf + request->out_len, value, h->val_len);
    request->out_len += h->val_len;

    memcpy(request->out_buf + request->out_len, "\r\n", 2);
    request->out_len += 2;

    sxe_list_push(&request->out_headers, h);

    SXER80I("return");
}

void
sxe_httpd_set_content_length(SXE_HTTP_REQUEST *request, int length)
{
    SXE *this = request->sxe;
    char ibuf[32];

    SXE_UNUSED_PARAMETER(this);
    SXEE92I("sxe_httpd_set_content_length(request=%p,length=%d)", request, length);
    snprintf(ibuf, sizeof ibuf, "%d", length);
    sxe_httpd_set_header_out(request, "Content-Length", ibuf, 0);
    SXER90I("return");
}

void
sxe_httpd_response_start(SXE_HTTP_REQUEST *request, int code, const char *status)
{
    SXE *this = request->sxe;
    char rbuf[4096];

    SXE_UNUSED_PARAMETER(this);
    SXEE83I("sxe_httpd_response_start(request=%p,code=%d,status=%s)", request, code, status);
    snprintf(rbuf, sizeof rbuf, "HTTP/1.1 %d %s\r\n", code, status);
    sxe_write(this, rbuf, strlen(rbuf));
    sxe_write(this, request->out_buf, request->out_len);
    sxe_write(this, "\r\n", 2); /* extra \r\n */
    SXER80I("return");
}

void
sxe_httpd_response_chunk(SXE_HTTP_REQUEST *request, const char *chunk, unsigned length)
{
    SXEE83("sxe_httpd_response_chunk(request=%p, chunk=%p, length=%u)", request, chunk, length);
    sxe_write(request->sxe, chunk, length);
    SXER80("return");
}

static void
sxe_httpd_event_sendfile_done(SXE * this, SXE_RETURN final_result)
{
    SXE_HTTP_REQUEST * request = SXE_USER_DATA(this);
    request->sendfile_handler(request, final_result, request->sendfile_userdata);
}

void
sxe_httpd_response_sendfile(SXE_HTTP_REQUEST *request, int fd, unsigned length, sxe_httpd_sendfile_handler handler, void *user_data)
{
    request->sendfile_handler  = handler;
    request->sendfile_userdata = user_data;
#ifndef WINDOWS_NT
    sxe_sendfile(request->sxe, fd, length, sxe_httpd_event_sendfile_done);
#endif
}

void
sxe_httpd_response_end(SXE_HTTP_REQUEST *request)
{
    SXE *this = request->sxe;
    SXE_HTTP_REQUEST * reqpool = request->server->requests;
    unsigned state = sxe_pool_index_to_state(reqpool, request - reqpool);

    SXE_UNUSED_PARAMETER(this);
    SXEE81I("sxe_httpd_response_end(request=%p)", request);
    sxe_httpd_clear_request(request);
    sxe_pool_set_indexed_element_state(reqpool, request - reqpool, state, SXE_HTTPD_CONN_IDLE);
    sxe_buf_resume(this);
    SXER80I("return");
}

void
sxe_httpd_response_simple(SXE_HTTP_REQUEST * request, int code, const char * status, const char * body)
{
    SXE * this = request->sxe;
    int   len  = strlen(body);

    SXE_UNUSED_PARAMETER(this);
    SXEE84I("sxe_httpd_response_simple(request=%p,code=%d,status=%s,body=%s)", request, code, status, body);
    sxe_httpd_set_content_length(request, len);
    sxe_httpd_response_start(request, code, status);
    sxe_httpd_response_chunk(request, body, len);
    sxe_httpd_response_end(request);
    SXER80I("return");
}

SXE *
sxe_httpd_listen(SXE_HTTPD * self, const char *address, unsigned short port)
{
    SXE * this;

    SXEE82("sxe_httpd_listen(address=%s, port=%hu)", address, port);

    if ((this = sxe_new_tcp(NULL, address, port, sxe_httpd_event_connect, sxe_httpd_event_read, sxe_httpd_event_close)) == NULL) {
        SXEL20("sxe_httpd_listen: Failed to allocate a SXE for TCP");           /* Coverage Exclusion: TODO: Test me! */
        goto SXE_ERROR_OUT;                                                     /* Coverage Exclusion: TODO: Test me! */
    }

    SXE_USER_DATA(this) = self;

    if (sxe_listen(this) != SXE_RETURN_OK) {
        SXEL22("sxe_httpd_listen: Failed to listen on address %s, port %hu)", address, port);    /* Coverage Exclusion: TODO: Test me! */
        sxe_close(this);                                                        /* Coverage Exclusion: TODO: Test me! */
        this = NULL;                                                            /* Coverage Exclusion: TODO: Test me! */
    }

SXE_ERROR_OUT:
    SXER81("return %p", this);
    return this;
}

SXE *
sxe_httpd_listen_pipe(SXE_HTTPD * self, const char * path)
{
    SXE * this;

    SXEE81("sxe_httpd_listen_pipe(path=%s)", path);

    if ((this = sxe_new_pipe(NULL, path, sxe_httpd_event_connect, sxe_httpd_event_read, sxe_httpd_event_close)) == NULL) {
        SXEL20("sxe_httpd_listen_pipe: Failed to allocate a SXE for pipes");    /* Coverage Exclusion: TODO: Test me! */
        goto SXE_ERROR_OUT;                                                     /* Coverage Exclusion: TODO: Test me! */
    }

    SXE_USER_DATA(this) = self;

    if (sxe_listen(this) != SXE_RETURN_OK) {
        SXEL21("sxe_httpd_listen_pipe: Failed to listen on path %s)", path);    /* Coverage Exclusion: TODO: Test me! */
        sxe_close(this);                                                        /* Coverage Exclusion: TODO: Test me! */
        this = NULL;                                                            /* Coverage Exclusion: TODO: Test me! */
    }

SXE_ERROR_OUT:
    SXER81("return %p", this);
    return this;
}

void
sxe_httpd_construct(SXE_HTTPD * self, int connections, sxe_httpd_handler handler, void * user_data)
{
    int           i;
    double        timeout[SXE_HTTPD_CONN_NUMBER_OF_STATES];
    int           total_headers = SXE_HTTPD_HEADERS_PER_REQUEST * connections;
    SXE_LOG_LEVEL log_level_saved;

    SXEE84("sxe_httpd_construct(self=%p, connections=%u, event_handler=%p, user_data=%p)", self, connections, handler, user_data);

    if (total_headers < SXE_HTTPD_HEADERS_MIN) {
        total_headers = SXE_HTTPD_HEADERS_MIN;
    }

    timeout[SXE_HTTPD_CONN_FREE]         = 0;
    timeout[SXE_HTTPD_CONN_IDLE]         = 60;   /* TODO: make this configurable */
    timeout[SXE_HTTPD_CONN_REQ_LINE]     = 2;
    timeout[SXE_HTTPD_CONN_REQ_HEADERS]  = 30;
    timeout[SXE_HTTPD_CONN_REQ_BODY]     = 0;
    timeout[SXE_HTTPD_CONN_REQ_RESPONSE] = 0;

    SXEA10(self->h_array = calloc(total_headers, sizeof(SXE_HTTP_HEADER)), "allocated header nodes");
    self->requests = sxe_pool_new_with_timeouts("httpd", connections, sizeof(SXE_HTTP_REQUEST), SXE_HTTPD_CONN_NUMBER_OF_STATES,
                                                timeout, sxe_httpd_event_timeout, 0);

    /* TODO: Use a pool instead of reinventing the wheel ;') */

    log_level_saved = sxe_log_decrease_level(SXE_LOG_LEVEL_DEBUG);
    SXE_LIST_CONSTRUCT(&self->h_freelist, 0, SXE_HTTP_HEADER, node);

    for (i = 0; i < total_headers; i++) {
        sxe_list_unshift(&self->h_freelist, &self->h_array[i]);
    }

    for (i = 0; i < connections; i++) {
        SXE_LIST_CONSTRUCT(&self->requests[i].in_headers, 1, SXE_HTTP_HEADER, node);
        SXE_LIST_CONSTRUCT(&self->requests[i].out_headers, 1, SXE_HTTP_HEADER, node);
    }

    sxe_log_set_level(log_level_saved);
    self->user_data = user_data;
    self->handler   = handler;
    SXER80("return");
}

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
