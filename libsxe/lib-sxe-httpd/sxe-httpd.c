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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>    /* For alloca() under Windows */

#include "sxe-httpd.h"
#include "sxe-log.h"
#include "sxe-pool.h"

#define HTTPD_CONTENT_LENGTH "Content-Length"

static void
sxe_httpd_default_connect_handler(SXE_HTTPD_REQUEST *request) /* Coverage Exclusion - todo: win32 coverage */
{
    SXE * this = request->sxe; /* Coverage Exclusion - todo: win32 coverage */
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXEE92I("%s(request=%p)", __func__, request);
    SXER90I("return");
} /* Coverage Exclusion - todo: win32 coverage */

static void
sxe_httpd_default_request_handler(SXE_HTTPD_REQUEST *request, const char * method, unsigned method_length,
                                  const char * url, unsigned url_length, const char * version, unsigned version_length)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXE_UNUSED_PARAMETER(method);
    SXE_UNUSED_PARAMETER(method_length);
    SXE_UNUSED_PARAMETER(url);
    SXE_UNUSED_PARAMETER(url_length);
    SXE_UNUSED_PARAMETER(version);
    SXE_UNUSED_PARAMETER(version_length);
    SXEE98I("%s(request=%p,method='%.*s',url='%.*s',version='%.*s')", __func__, request,
            method_length, method, url_length, url, version_length, version);
    SXER90I("return");
}

static void
sxe_httpd_default_header_handler(SXE_HTTPD_REQUEST * request, const char * key, unsigned key_length, const char * value,
                                 unsigned value_length)
{
    SXE * this = request->sxe;

    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXE_UNUSED_PARAMETER(key);
    SXE_UNUSED_PARAMETER(key_length);
    SXE_UNUSED_PARAMETER(value);
    SXE_UNUSED_PARAMETER(value_length);
    SXEE96I("%s(request=%p,key='%.*s',value='%.*s')", __func__, request, key_length, key, value_length, value);
    SXER90I("return");
}

static void
sxe_httpd_default_eoh_handler(SXE_HTTPD_REQUEST *request)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXEE92I("%s(request=%p)", __func__, request);
    SXER90I("return");
}

static void
sxe_httpd_default_body_handler(SXE_HTTPD_REQUEST *request, const char *chunk, unsigned chunk_length)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXE_UNUSED_PARAMETER(chunk);
    SXE_UNUSED_PARAMETER(chunk_length);
    SXEE92I("%s(request=%p)", __func__, request);
    SXER90I("return");
}

static void
sxe_httpd_default_respond_handler(SXE_HTTPD_REQUEST *request) /* Coverage Exclusion - todo: win32 coverage */
{
    SXE * this = request->sxe; /* Coverage Exclusion - todo: win32 coverage */
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXEE92I("%s(request=%p)", __func__, request);
    SXEL30I("warning: default respond handler: generating 404 Not Found"); /* Coverage Exclusion - todo: win32 coverage */
    sxe_httpd_response_error(request, 404, "Not Found");
    SXER90I("return");
} /* Coverage Exclusion - todo: win32 coverage */

static void
sxe_httpd_default_close_handler(SXE_HTTPD_REQUEST *request) /* Coverage Exclusion - todo: win32 coverage */
{                                                    /* Coverage Exclusion: TBD */
    SXE * this = request->sxe;                       /* Coverage Exclusion: TBD */
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXEE92I("%s(request=%p)", __func__, request);
    SXER90I("return");
}                                                    /* Coverage Exclusion: TBD */

sxe_httpd_connect_handler
sxe_httpd_set_connect_handler(SXE_HTTPD *self, sxe_httpd_connect_handler new_handler)
{
    sxe_httpd_connect_handler old_handler = self->on_connect ;
    self->on_connect = new_handler ? new_handler : sxe_httpd_default_connect_handler;
    return old_handler;
}

sxe_httpd_request_handler
sxe_httpd_set_request_handler(SXE_HTTPD *self, sxe_httpd_request_handler new_handler)
{
    sxe_httpd_request_handler old_handler = self->on_request ;
    self->on_request = new_handler ? new_handler : sxe_httpd_default_request_handler;
    return old_handler;
}

sxe_httpd_header_handler
sxe_httpd_set_header_handler(SXE_HTTPD *self, sxe_httpd_header_handler new_handler)
{
    sxe_httpd_header_handler old_handler = self->on_header ;
    self->on_header = new_handler ? new_handler : sxe_httpd_default_header_handler;
    return old_handler;
}

sxe_httpd_eoh_handler
sxe_httpd_set_eoh_handler(SXE_HTTPD *self, sxe_httpd_eoh_handler new_handler)
{
    sxe_httpd_eoh_handler old_handler = self->on_eoh ;
    self->on_eoh = new_handler ? new_handler : sxe_httpd_default_eoh_handler;
    return old_handler;
}

sxe_httpd_body_handler
sxe_httpd_set_body_handler(SXE_HTTPD *self, sxe_httpd_body_handler new_handler)
{
    sxe_httpd_body_handler old_handler = self->on_body ;
    self->on_body = new_handler ? new_handler : sxe_httpd_default_body_handler;
    return old_handler;
}

sxe_httpd_respond_handler
sxe_httpd_set_respond_handler(SXE_HTTPD *self, sxe_httpd_respond_handler new_handler)
{
    sxe_httpd_respond_handler old_handler = self->on_respond ;
    self->on_respond = new_handler ? new_handler : sxe_httpd_default_respond_handler;
    return old_handler;
}

sxe_httpd_close_handler
sxe_httpd_set_close_handler(SXE_HTTPD *self, sxe_httpd_close_handler new_handler)
{
    sxe_httpd_close_handler old_handler = self->on_close ;
    self->on_close = new_handler ? new_handler : sxe_httpd_default_close_handler;
    return old_handler;
}

const char *
sxe_httpd_state_to_string(unsigned state)
{
    switch (state) {
    case SXE_HTTPD_CONN_FREE:             return "FREE";        /* not connected */
    case SXE_HTTPD_CONN_IDLE:             return "IDLE";        /* client connected */
    case SXE_HTTPD_CONN_REQ_LINE:         return "LINE";        /* VERB <SP> URL [<SP> VERSION] */
    case SXE_HTTPD_CONN_REQ_HEADERS:      return "HEADERS";     /* reading headers */
    case SXE_HTTPD_CONN_REQ_BODY:         return "BODY";        /* at BODY we run the handler for each chunk. */
    case SXE_HTTPD_CONN_REQ_RESPONSE:     return "RESPONSE";    /* awaiting the response (read disabled) */
    case SXE_HTTPD_CONN_ERROR_SENT:       return "ERROR";       /* sent an error, waiting for close            */
    }

    return NULL;                                                /* not a state - just keeps the compiler happy */
}

static void
sxe_httpd_clear_request(SXE_HTTPD_REQUEST *request)
{
    SXE *this = request->sxe;

    SXE_UNUSED_PARAMETER(this);
    SXEE81I("sxe_httpd_clear_request(request=%p)", request);

    request->in_content_length = 0;
    request->in_content_seen   = 0;
    request->out_eoh           = false;

    SXER80I("return");
}

/**
 * Close an HTTPD session and free the request object
 *
 * @param request Pointer to an HTTP request object
 */
void
sxe_httpd_close(SXE_HTTPD_REQUEST * request)
{
    SXE *              this     = request->sxe;
    SXE_HTTPD_REQUEST * requests = request->server->requests;
    unsigned           state    = sxe_pool_index_to_state(requests, request - requests);

    SXE_UNUSED_PARAMETER(this);

    SXEE81I("sxe_httpd_close(request=%u)", request - requests);
    sxe_httpd_clear_request(request);

    if (request->sxe != NULL) {
        sxe_close(request->sxe);
    }

    sxe_pool_set_indexed_element_state(requests, request - requests, state, SXE_HTTPD_CONN_FREE);
    SXER80I("return");
}

void
sxe_httpd_response_simple(SXE_HTTPD_REQUEST * request, int code, const char * status, const char * body, ...)
{
    SXE        * this = request->sxe;
    va_list      headers;
    const char * name;
    const char * value;
    unsigned     length;

    SXE_UNUSED_PARAMETER(this);
    SXEE85I("%s(request=%p,code=%d,status=%s,body='%s',...)", __func__, request, code, status, body);
    va_start(headers, body);
    sxe_httpd_response_start(request, code, status);

    while ((name = va_arg(headers, const char *)) != NULL) {
        SXEA12((value = va_arg(headers, const char *)) != NULL, "%s: header %s has no value", __func__, name);
        sxe_httpd_response_header(request, name, value, 0);
    }

    if (body != NULL) {
        length = strlen(body);
        sxe_httpd_response_content_length(request, length);
        sxe_httpd_response_chunk(request, body, length);
    }

    sxe_httpd_response_end(request);
    va_end(headers);
    SXER80I("return");
}

/**
 * Send an error response
 *
 * @param request Request to respond to
 * @param code    Status code (e.g. 404)
 * @param status  Status string (e.g. "Not Found")
 *
 * @note The connection is intentionally left open (see RFC 2616 10.4: an immediate close can cause data loss on the client).
 *       If you want to force it closed, it should be safe immediately after this call to call sxe_http_close, but otherwise,
 *       you should let the client close the connection or let HTTPD reap it if it runs out of free connections.  You will not
 *       get a free event for a client close on a connection to which you have sent an error response.
 */
void
sxe_httpd_response_error(SXE_HTTPD_REQUEST * request, unsigned code, const char * status)
{
    SXE               * this = request->sxe;
    SXE_HTTPD_REQUEST * pool = request->server->requests;
    unsigned            id   = request - pool;

    SXE_UNUSED_PARAMETER(this);
    SXEE84I("%s(request=%u,code=%u,status=%s)", __func__, id, code, status);
    sxe_httpd_response_simple(request, code, status, NULL, "Connection", "close", NULL);
    sxe_pool_set_indexed_element_state(pool, id, sxe_pool_index_to_state(pool, id), SXE_HTTPD_CONN_ERROR_SENT);
    SXER80I("return");
}

static void
sxe_httpd_event_connect(SXE * this)
{
    SXE_HTTPD         * self = SXE_USER_DATA(this);
    SXE_HTTPD_REQUEST * request;
    unsigned            id;

    SXE_UNUSED_PARAMETER(this);
    SXEE81I("sxe_httpd_event_connect(this=%p)", this);

    id = sxe_pool_set_oldest_element_state(self->requests, SXE_HTTPD_CONN_FREE, SXE_HTTPD_CONN_IDLE);

    if (id == SXE_POOL_NO_INDEX) {
        SXEL20I("sxe_httpd_event_connect: no free connections: checking for errored connections");
        id = sxe_pool_get_oldest_element_index(self->requests, SXE_HTTPD_CONN_ERROR_SENT);

        if (id == SXE_POOL_NO_INDEX) {
            SXEL20I("sxe_httpd_event_connect: no errored connections either: returning 503 error"); /* Coverage Exclusion - todo: win32 coverage */
            SXE_WRITE_LITERAL(this, "HTTP/1.1 503 Service unavailable\r\n" "Connection: close\r\n\r\n");
            sxe_close(this);
            goto SXE_EARLY_OUT;
        }

        sxe_httpd_close(&self->requests[id]);
        SXEA11I(sxe_pool_set_oldest_element_state(self->requests, SXE_HTTPD_CONN_FREE, SXE_HTTPD_CONN_IDLE) == id,
               "sxe_httpd_event_connect: Expected oldest free request to be the reaped error request %u", id);
        SXEL51I("sxe_pool_set_oldest_element_state: Reaped oldest request to which error response was sent (%u)", id);
    }

    request = &self->requests[id];
    memset(request, '\0', sizeof(*request));
    SXE_USER_DATA(this) = request;
    request->sxe = this;
    request->server = self;

    (*self->on_connect)(request);

SXE_EARLY_OUT:
    SXER80I("return");
}

static void
sxe_httpd_event_close(SXE * this)
{
    SXE_HTTPD_REQUEST * request = SXE_USER_DATA(this);
    SXE_HTTPD_REQUEST * pool    = request->server->requests;
    unsigned           id      = request - pool;

    SXEE81I("sxe_httpd_event_close(request=%u)", id);

    request->sxe = NULL;    /* Suppress further activity on this connection */

    if (sxe_pool_index_to_state(pool, id) != SXE_HTTPD_CONN_ERROR_SENT) {
        (*request->server->on_close)(request);
    }

    sxe_httpd_close(request);
    SXER80I("return");
}

static const struct {
    const char *name;
    unsigned length;
    SXE_HTTP_METHOD method;
} methods[] = {
    { "GET",    3, SXE_HTTP_METHOD_GET     },
    { "PUT",    3, SXE_HTTP_METHOD_PUT     },
    { "HEAD",   4, SXE_HTTP_METHOD_HEAD    },
    { "POST",   4, SXE_HTTP_METHOD_POST    },
    { "DELETE", 6, SXE_HTTP_METHOD_DELETE  },
    { NULL,     0, SXE_HTTP_METHOD_INVALID }
};

SXE_HTTP_METHOD
sxe_httpd_parse_method(const char *method, unsigned method_length)
{
    SXE_HTTP_METHOD meth = SXE_HTTP_METHOD_INVALID;
    unsigned i;

    SXEE83("%s(method=[%.*s])", __func__, method_length, method);

    for (i = 0; methods[i].name; i++) {
        if (method_length == methods[i].length && 0 == strncmp(method, methods[i].name, methods[i].length)) {
            meth = methods[i].method;
            break;
        }
    }

    SXER81("return %u", meth);
    return meth;
}

static const struct {
    const char *value;
    unsigned length;
    SXE_HTTP_VERSION version;
} versions[] = {
    { "HTTP/1.0", 8, SXE_HTTP_VERSION_1_0     },
    { "HTTP/1.1", 8, SXE_HTTP_VERSION_1_1     },
    { NULL,       0, SXE_HTTP_VERSION_UNKNOWN }
};

SXE_HTTP_VERSION
sxe_httpd_parse_version(const char *version, unsigned version_length)
{
    SXE_HTTP_VERSION v = SXE_HTTP_VERSION_UNKNOWN;
    unsigned i;

    SXEE83("%s(version=[%.*s])", __func__, version_length, version);

    for (i = 0; versions[i].value; i++) {
        if (version_length == versions[i].length && 0 == strncmp(version, versions[i].value, versions[i].length)) {
            v = versions[i].version;
            break;
        }
    }

    SXER81("return %u", v);
    return v;
}

static bool
sxe_httpd_parse_request_line(SXE_HTTPD_REQUEST * request, const char * start, unsigned length)
{
    SXE_HTTPD * server           = request->server;
    SXE       * this             = request->sxe;
    bool        request_is_valid = false;
    unsigned    i;
    unsigned    method_length;
    unsigned    url_off, url_length;
    unsigned    version_off, version_length;

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

    method_length = i;
    SXEL93I("method: %u[%.*s]", i, i, start);

    /* Whitespace */
    for (; i < length && strchr(" \t", start[i]); ++i) {
    }

    if (i == length) {
        goto SXE_EARLY_OUT;                            /* COVERAGE EXCLUSION: TODO: Test me! */
    }

    /* Look for the URL */
    url_off = i;

    for (; i < length && !strchr(" \t", start[i]); ++i) {
    }

    if (i == length) {
        goto SXE_EARLY_OUT;
    }

    url_length = i - url_off;
    SXEL93I("url: %u[%.*s]", url_length, url_length, start + url_off);

    /* Skip ahead */
    for (; i < length && strchr(" \t", start[i]); ++i) {
    }

    if (i == length) {
        goto SXE_EARLY_OUT;                            /* COVERAGE EXCLUSION: TODO: Test me! */
    }

    /* Look for the HTTP version */
    version_off = i;

    for (; i < length && !strchr(" \t", start[i]); ++i) {
    }

    if (i != length) {
        goto SXE_EARLY_OUT;                            /* COVERAGE EXCLUSION: TODO: Test me! */
    }

    version_length = i - version_off;
    SXEL93I("httpver: %u[%.*s]", version_length, version_length, start + version_off);

    request->method  = sxe_httpd_parse_method(start, method_length);

    if (request->method == SXE_HTTP_METHOD_INVALID) {
        goto SXE_EARLY_OUT;
    }

    request->version = sxe_httpd_parse_version(start + version_off, version_length);

    (*server->on_request)(request,
                          start, method_length,
                          start + url_off, url_length,
                          start + version_off, version_length);

    request_is_valid = true;

SXE_EARLY_OUT:
    SXER81I("return request_is_valid=%s", SXE_BOOL_TO_STR(request_is_valid));
    return request_is_valid;
}

static void
sxe_httpd_event_read(SXE * this, int additional_length)
{
    SXE_HTTPD_REQUEST * request = SXE_USER_DATA(this);
    SXE_HTTP_MESSAGE  * message;
    SXE_HTTPD         * server;
    SXE_HTTPD_REQUEST * request_pool;
    unsigned            state;
    char              * start;
    char              * end;
    unsigned            length;
    unsigned            consumed             = 0;
    int                 response_status_code = 400;
    const char        * response_reason      = "Bad request";
    SXE_RETURN          result;
    const char        * key;
    unsigned            key_length;
    const char        * value;
    unsigned            value_length;
    unsigned            i;

    SXEE82I("sxe_httpd_event_read(request=%p,additional_length=%d)", request, additional_length);
    SXE_UNUSED_PARAMETER(additional_length);
    message      = &request->message;
    server       = request->server;
    request_pool = server->requests;
    state        = sxe_pool_index_to_state(request_pool, request - request_pool);

    switch (state) {
    case SXE_HTTPD_CONN_IDLE:
        SXEL90I("state IDLE -> LINE");
        sxe_pool_set_indexed_element_state(request_pool, request - request_pool, state, SXE_HTTPD_CONN_REQ_LINE);
        state = SXE_HTTPD_CONN_REQ_LINE;
        /* fall through */

    case SXE_HTTPD_CONN_REQ_LINE:
        start  = SXE_BUF(this);
        length = SXE_BUF_USED(this);
        end    = sxe_strncspn(start, "\r\n", length);    /* TODO: Be smarter about not rescanning */

        if (end == NULL) {
            if (length == SXE_BUF_SIZE) {
                response_status_code = 414;
                response_reason = "Request-URI too large";
                goto SXE_ERROR_OUT;
            }

            goto SXE_EARLY_OUT;    /* Buffer is not full. Try again when there is more data */
        }

        if (!sxe_httpd_parse_request_line(request, start, end - start)) {
            goto SXE_ERROR_OUT;
        }

        SXEL90I("state LINE -> REQ_HEADERS");
        sxe_pool_set_indexed_element_state(request_pool, request - request_pool, state, SXE_HTTPD_CONN_REQ_HEADERS);
        state = SXE_HTTPD_CONN_REQ_HEADERS;
        sxe_http_message_construct(message, SXE_BUF(this), SXE_BUF_USED(this));
        SXEV80(sxe_http_message_parse_next_line_element(message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), == SXE_RETURN_OK,
               "Failed to reparse request line");

        /* fall through */

    case SXE_HTTPD_CONN_REQ_HEADERS:
        sxe_http_message_increase_buffer_length(message, SXE_BUF_USED(this));

        /* While the end-of-headers has not yet been reached
         */
        while ((result = sxe_http_message_parse_next_header(message)) != SXE_RETURN_END_OF_FILE) {
            if (result == SXE_RETURN_ERROR_BAD_MESSAGE) {
                goto SXE_ERROR_OUT;
            }

            if (result == SXE_RETURN_WARN_WOULD_BLOCK) {
                /* If the buffer is full
                 */
                if (SXE_BUF_USED(this) == SXE_BUF_SIZE) {
                    consumed = sxe_http_message_consume_parsed_headers(message); /* COVERAGE EXCLUSION - TODO: WIN32 COVERAGE */

                    if (consumed == 0) {
                        SXEL31I("%s: Bad request: Haven't found the end of the current header, but the buffer is full", __func__);
                        goto SXE_ERROR_OUT; /* COVERAGE EXCLUSION - TODO: WIN32 COVERAGE */
                    }

                    SXEL90I("About to consume the headers processed so far");
                    sxe_buf_consume(this, consumed);
                    sxe_buf_resume( this, SXE_BUF_RESUME_IMMEDIATE);
                    goto SXE_EARLY_OUT; /* COVERAGE EXCLUSION - TODO: WIN32 COVERAGE */
                }

                SXEL90I("Haven't found the end of the headers yet; waiting for more data");
                goto SXE_EARLY_OUT; /* COVERAGE EXCLUSION - TODO: WIN32 COVERAGE */
            }

            SXEA11I(result == SXE_RETURN_OK, "Unexpected return from sxe_http_message_parse_next_header: %s",
                    sxe_return_to_string(result));

            key          = sxe_http_message_get_header_name(message);
            key_length   = sxe_http_message_get_header_name_length(message);
            value        = sxe_http_message_get_header_value(message);
            value_length = sxe_http_message_get_header_value_length(message);

            /* Is this the content-length header?
             */
            if (key_length == SXE_LITERAL_LENGTH(HTTPD_CONTENT_LENGTH) && strncasecmp(key, HTTPD_CONTENT_LENGTH, key_length)
                == 0)
            {
                if (value_length == 0) {
                    SXEL31I("%s: Bad request: Content-Length header field value is empty", __func__);
                    goto SXE_ERROR_OUT;
                }

                request->in_content_length = 0;

                for (i = 0; i < value_length; i++) {
                    if (!isdigit(value[i])) {
                        SXEL32I("%s: Bad request: Content-Length header field value contains a non-digit '%c'", __func__,
                                value[i]);
                        goto SXE_ERROR_OUT;
                    }

                    request->in_content_length = request->in_content_length * 10 + value[i] - '0';
                }

                SXEL91I("content-length: %u", request->in_content_length);
            }

            SXEL94I("About to call on_header handler with key '%.*s' and value '%.*s'", key_length, key, value_length, value);
            (*request->server->on_header)(request, key, key_length, value, value_length);
        }

        SXEL90I("state REQ_HEADER -> REQ_EOH");
        consumed = sxe_http_message_consume_parsed_headers(message);
        sxe_buf_consume(this, consumed);    /* eat the terminator - om nom nom  */
        (*server->on_eoh)(request);
        request->in_content_seen = 0;
        SXEL90I("state REQ_EOH -> REQ_BODY");
        sxe_pool_set_indexed_element_state(request_pool, request - request_pool, state, SXE_HTTPD_CONN_REQ_BODY);
        state = SXE_HTTPD_CONN_REQ_BODY;

        /* fall through */

    case SXE_HTTPD_CONN_REQ_BODY:
        if (request->in_content_length && SXE_BUF_USED(this)) {
            unsigned data_len = SXE_BUF_USED(this);

            if (data_len > request->in_content_length - request->in_content_seen) {
                data_len = request->in_content_length - request->in_content_seen; /* Coverage Exclusion - todo: win32 coverage */
            }

            SXEL94I("c/l %u - body chunk %d:%.*s", request->in_content_length, data_len, data_len, SXE_BUF(this));
            (*server->on_body)(request, SXE_BUF(this), data_len);
            consumed += data_len;
            sxe_buf_consume(this, data_len);
            request->in_content_seen += data_len;
        }

        if (request->in_content_length <= request->in_content_seen) {
            SXEL90I("state REQ_BODY -> REQ_RESPONSE");
            sxe_pool_set_indexed_element_state(request_pool, request - request_pool, state, SXE_HTTPD_CONN_REQ_RESPONSE);
            state = SXE_HTTPD_CONN_REQ_RESPONSE;
            (*server->on_respond)(request);
        }

        goto SXE_EARLY_OUT;

    case SXE_HTTPD_CONN_ERROR_SENT:
        SXEL61I("Discarding %u bytes of data from a client in ERROR_SENT state", SXE_BUF_USED(this));
        goto SXE_EARLY_OUT;

    case SXE_HTTPD_CONN_REQ_RESPONSE:
        goto SXE_EARLY_OUT;

    default:
        SXEA11I(0, "Internal error: read called during unexpected state %s", sxe_httpd_state_to_string(state));    /* COVERAGE EXCLUSION: Can't happen */
    }

SXE_ERROR_OUT:
    sxe_buf_clear(this);
    sxe_httpd_response_error(request, response_status_code, response_reason);
    (*server->on_close)(request);
    consumed = 0;    /* Data is not actually consumed - just let it pile up in the receive buffer */

SXE_EARLY_OUT:
    if (consumed > 0) {
        sxe_buf_resume(this, SXE_BUF_RESUME_IMMEDIATE);    /* If there's more data, deliver it */
    }

    SXER80I("return");
}

void
sxe_httpd_response_start(SXE_HTTPD_REQUEST * request, int code, const char *status)
{
    SXE * this = request->sxe;
    char  rbuf[4096];

    SXEE83I("sxe_httpd_response_start(request=%p,code=%d,status=%s)", request, code, status);
    snprintf(rbuf, sizeof rbuf, "HTTP/1.1 %d %s\r\n", code, status);
    sxe_write(this, rbuf, strlen(rbuf));
    SXER80I("return");
}

void
sxe_httpd_response_header(SXE_HTTPD_REQUEST * request, const char *header, const char *value, unsigned length)
{
    SXE    * this      = request->sxe;
    unsigned vlen      = length ? length : strlen(value);
    unsigned hlen      = strlen(header) + vlen + SXE_LITERAL_LENGTH(": \r\n");
    char   * headerfmt = alloca(hlen + 1);    /* leave room for NUL */

    SXEE84I("sxe_httpd_response_header(request=%p,header=%s,value=%.*s)", request, header, vlen, value);
    SXEA11I(headerfmt != NULL, "failed to allocate %u bytes", hlen);
    snprintf(headerfmt, hlen + 1, "%s: %.*s\r\n", header, vlen, value);
    sxe_write(this, headerfmt, hlen);
    SXER80I("return");
}

void
sxe_httpd_response_content_length(SXE_HTTPD_REQUEST * request, int length)
{
    SXE *this = request->sxe;
    char ibuf[32];

    SXE_UNUSED_PARAMETER(this);
    SXEE92I("sxe_httpd_response_content_length(request=%p,length=%d)", request, length);
    snprintf(ibuf, sizeof ibuf, "%d", length);
    sxe_httpd_response_header(request, "Content-Length", ibuf, 0);
    SXER90I("return");
}

void
sxe_httpd_response_raw(SXE_HTTPD_REQUEST *request, const char *chunk, unsigned length)
{
    SXE *this = request->sxe;
    SXE_UNUSED_PARAMETER(this);

    SXEE84I("%s(request=%p, chunk=%p, length=%u)", __func__, request, chunk, length);
    request->out_eoh = true; /* NOTE: you're on your own! */
    sxe_write(request->sxe, chunk, length);
    SXER80I("return");
}

void
sxe_httpd_response_chunk(SXE_HTTPD_REQUEST *request, const char *chunk, unsigned length)
{
    SXE *this = request->sxe;
    SXE_UNUSED_PARAMETER(this);

    SXEE84I("%s(request=%p, chunk=%p, length=%u)", __func__, request, chunk, length);

    if (!request->out_eoh) {
        sxe_write(this, "\r\n", 2);
        request->out_eoh = true;
    }

    sxe_write(request->sxe, chunk, length);
    SXER80I("return");
}

#ifndef WINDOWS_NT
static void
sxe_httpd_event_sendfile_done(SXE * this, SXE_RETURN final_result)
{
    SXE_HTTPD_REQUEST * request = SXE_USER_DATA(this);

    SXEE83I("%s(request=%p, final_result=%s)", __func__, request, sxe_return_to_string(final_result));
    request->sendfile_handler(request, final_result, request->sendfile_userdata);
    SXER80I("return");
}

void
sxe_httpd_response_sendfile(SXE_HTTPD_REQUEST *request, int fd, unsigned length, sxe_httpd_sendfile_handler handler, void *user_data)
{
    request->sendfile_handler  = handler;
    request->sendfile_userdata = user_data;

    if (!request->out_eoh) {
        sxe_write(request->sxe, "\r\n", 2);
        request->out_eoh = true;
    }

    sxe_sendfile(request->sxe, fd, length, sxe_httpd_event_sendfile_done);
}
#endif

void
sxe_httpd_response_end(SXE_HTTPD_REQUEST *request)
{
    SXE               * this = request->sxe;
    SXE_HTTPD_REQUEST * request_pool = request->server->requests;
    unsigned            state        = sxe_pool_index_to_state(request_pool, request - request_pool);

    SXE_UNUSED_PARAMETER(this);
    SXEE81I("sxe_httpd_response_end(request=%p)", request);

    if (!request->out_eoh) {
        sxe_write(this, "\r\n", 2);
    }

    sxe_httpd_clear_request(request);

    if (state != SXE_HTTPD_CONN_ERROR_SENT) {
    sxe_pool_set_indexed_element_state(request_pool, request - request_pool, state, SXE_HTTPD_CONN_IDLE);
    sxe_buf_resume(this, SXE_BUF_RESUME_IMMEDIATE);
    }

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

#ifdef WINDOWS_NT
#else
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
#endif

void
sxe_httpd_construct(SXE_HTTPD * self, int connections, unsigned options)
{
    SXEE84("%s(self=%p, connections=%u, options=%x)", __func__, self, connections, options);

    SXE_UNUSED_PARAMETER(options);

    self->requests   = sxe_pool_new("httpd", connections, sizeof(SXE_HTTPD_REQUEST), SXE_HTTPD_CONN_NUMBER_OF_STATES, SXE_POOL_OPTION_UNLOCKED);
    sxe_pool_set_state_to_string(self->requests, sxe_httpd_state_to_string);
    self->on_connect = sxe_httpd_default_connect_handler;
    self->on_request = sxe_httpd_default_request_handler;
    self->on_header  = sxe_httpd_default_header_handler;
    self->on_eoh     = sxe_httpd_default_eoh_handler;
    self->on_body    = sxe_httpd_default_body_handler;
    self->on_respond = sxe_httpd_default_respond_handler;
    self->on_close   = sxe_httpd_default_close_handler;

    SXER80("return");
}

#define VALID_HEADER_CHARACTER(c) ( ((c) >= '0' && (c) <= '9') \
                                 || ((c) >= 'A' && (c) <= 'Z') \
                                 || ((c) >= 'a' && (c) <= 'z') \
                                 || ((c) == '-') )

bool
sxe_http_header_parse(const char *ptr, unsigned length, unsigned *pconsumed, bool *found_eoh, sxe_http_header_handler func, void *user_data)    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
{    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
    const char * key;
    const char * val;
    unsigned     key_len;
    unsigned     val_len;
    bool         headers_are_valid = true;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
    bool         call_func = true;            /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
    unsigned     i = 0;                       /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */

    SXEE82("sxe_http_header_parse(buf=%p,length=%u)", ptr, length);

    *pconsumed = 0;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */

    while (i < length) {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
        SXEL92("sxe_http_header_parse: i=%d length=%d", i, length);

        SXEL92("sxe_http_header_parse:      header block: [%.*s]", length - i, &ptr[i]);

        if (length - i >= 4 && 0 == strncmp(&ptr[i], "\r\n\r\n", 4)) {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            /* it is valid to have a header block containing only \r\n\r\n. This
             * means we have a valid header block containing zero headers. */
            *found_eoh = true;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            break;
        }

        SXEL92("sxe_http_header_parse:     skipping CRLF: offset=%d (%c)", i, ptr[i]);
        for (; i < length && strchr("\r\n", ptr[i]); i++);    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
        if (i == length) {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            goto SXE_EARLY_OUT;     /* COVERAGE EXCLUSION: TODO: Test defragmentation */
        }
        SXEL92("sxe_http_header_parse:                  : offset=%d (%c)", i, ptr[i]);

        key = &ptr[i];    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */

        SXEL92("sxe_http_header_parse: looking for colon: offset=%d (%c)", i, ptr[i]);

        for (; i < length && VALID_HEADER_CHARACTER(ptr[i]); i++) {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
        }
        if (i == length) {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            SXEL90("sxe_http_header_parse: hit end of buffer without finding colon; not done yet");
            goto SXE_EARLY_OUT; /* Coverage Exclusion - todo: win32 coverage */
        }
        SXEL92("sxe_http_header_parse:                  : offset=%d (%c)", i, ptr[i]);
        if (ptr[i] != ':') {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            headers_are_valid = false;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            goto SXE_EARLY_OUT;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
        }

        SXEL92("sxe_http_header_parse:                  : offset=%d (%c)", i, ptr[i]);
        key_len = &ptr[i] - key;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */

        if (key_len == 0) {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            headers_are_valid = false;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            goto SXE_EARLY_OUT;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
        }

        SXEL92("sxe_http_header_parse:        key length: offset=%d (%d)", i, key_len);
        i++;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */

        SXEL92("sxe_http_header_parse:   skipping SP-TAB: offset=%d (%c)", i, ptr[i]);

        for (; i < length && strchr(" \t", ptr[i]); i++) {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
        }

        if (i == length) {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            goto SXE_EARLY_OUT;     /* COVERAGE EXCLUSION: TODO: Test defragmentation */
        }

        SXEL92("sxe_http_header_parse:                  : offset=%d (%c)", i, ptr[i]);

        val = &ptr[i];    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
        SXEL92("sxe_http_header_parse:  looking for CRLF: offset=%d (%c)", i, ptr[i]);

        for (; i < length && !strchr("\r\n", ptr[i]); i++) {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
        }

        if (i == length) {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            goto SXE_EARLY_OUT;     /* COVERAGE EXCLUSION: TODO: Test defragmentation */
        }

        val_len = &ptr[i] - val;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */

        if (val_len == 0) {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            headers_are_valid = false;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            goto SXE_EARLY_OUT;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
        }

        SXEL91("sxe_http_header_parse:              end : offset=%d", i);
        SXEL92("sxe_http_header_parse:      value length: offset=%d (%d)", i, val_len);
        SXEL92("sxe_http_header_parse:            header: %.*s", val + val_len - key, key);
        *pconsumed = i;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */

        if (func && call_func) {    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
            call_func = func(key, key_len, val, val_len, user_data);    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
        }
    }

    SXEL90("sxe_http_header_parse: headers are done");

SXE_EARLY_OUT:                   /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
    SXER82("return headers_are_valid=%s // consumed=%u", SXE_BOOL_TO_STR(headers_are_valid), *pconsumed);
    return headers_are_valid;    /* COVERAGE EXCLUSION: To be removed once cnxd no longer uses */
}

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
