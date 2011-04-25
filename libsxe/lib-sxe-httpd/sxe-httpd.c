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
#include <stddef.h>
#include <string.h>
#ifdef __APPLE__
#include <alloca.h>    /* For alloca() under Mac OS X */
#else
#include <malloc.h>    /* For alloca() under Windows */
#endif

#include "sxe-httpd.h"
#include "sxe-log.h"
#include "sxe-pool.h"

#define HTTPD_CONTENT_LENGTH "Content-Length"

/* Pointer substraction is signed but index is unsigned */
#define SXE_HTTPD_REQUEST_INDEX(base_ptr, object_ptr) ((SXE_CAST(uintptr_t, object_ptr) - SXE_CAST(uintptr_t, base_ptr))/sizeof(SXE_HTTPD_REQUEST))

#define SXE_HTTPD_BUFFER_SIZE(s)     (offsetof(SXE_BUFFER, space) + (s)->buffersize)
#define SXE_HTTPD_BUFFER(s, idx)     (SXE_BUFFER *)(((char *)(s)->buffers) + (idx * SXE_HTTPD_BUFFER_SIZE(s)))
#define SXE_HTTPD_BUFFER_INDEX(s, b) ((SXE_CAST(uintptr_t, (b)) - SXE_CAST(uintptr_t, (s)->buffers)) / SXE_HTTPD_BUFFER_SIZE((s)))

typedef enum {
    SXE_HTTPD_BUFFER_FREE,
    SXE_HTTPD_BUFFER_USED,
    SXE_HTTPD_BUFFER_NUMBER_OF_STATES
} SXE_HTTPD_BUFFER_STATE;

static inline SXE_BUFFER *
sxe_httpd_get_buffer(SXE_HTTPD *self)
{
    SXE_BUFFER * buffer = NULL;
    unsigned     id;

    SXEE80("sxe_httpd_get_buffer()");

    id = sxe_pool_set_oldest_element_state(self->buffers, SXE_HTTPD_BUFFER_FREE, SXE_HTTPD_BUFFER_USED);
    if (id == SXE_POOL_NO_INDEX) {
        SXEL30("Warning: sxe_httpd_get_buffer: no buffers available, increase buffers or lower concurrency");   /* Coverage exclusion: todo: test running out of send buffers */
        goto SXE_EARLY_OUT;                                                                                     /* Coverage exclusion: todo: test running out of send buffers */
    }

    buffer = SXE_HTTPD_BUFFER(self, id);
    buffer->ptr  = buffer->space;
    buffer->len  = 0;
    buffer->sent = 0;

SXE_EARLY_OUT:
    SXER81("return %p", buffer);
    return buffer;
}

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
    sxe_httpd_response_simple(request, NULL, NULL, 404, "Not Found", NULL, NULL);
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
    case SXE_HTTPD_CONN_BAD:              return "BAD";         /* bad connection */
    case SXE_HTTPD_CONN_REQ_LINE:         return "LINE";        /* VERB <SP> URL [<SP> VERSION] */
    case SXE_HTTPD_CONN_REQ_HEADERS:      return "HEADERS";     /* reading headers */
    case SXE_HTTPD_CONN_REQ_BODY:         return "BODY";        /* at BODY we run the handler for each chunk. */
    case SXE_HTTPD_CONN_REQ_RESPONSE:     return "RESPONSE";    /* awaiting the response (read disabled) */
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
    SXE *               this       = request->sxe;
    SXE_HTTPD_REQUEST * requests   = request->server->requests;
    unsigned            request_id = SXE_HTTPD_REQUEST_INDEX(requests, request);
    unsigned            state;

    SXE_UNUSED_PARAMETER(this);

    SXEE82I("sxe_httpd_close(request=%p, request_id=%u)", request, request_id);
    sxe_httpd_clear_request(request);

    if (request->sxe != NULL) {
        sxe_close(request->sxe);
    }

    state = sxe_pool_index_to_state(requests, request_id);
    sxe_pool_set_indexed_element_state(requests, request_id, state, SXE_HTTPD_CONN_FREE);
    SXER80I("return");
}

void
sxe_httpd_response_simple(SXE_HTTPD_REQUEST * request, sxe_httpd_on_sent_handler on_complete, void *user_data, int code, const char * status, const char * body, ...)
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

    sxe_httpd_response_end(request, on_complete, user_data);
    va_end(headers);
    SXER80I("return");
}

static void
sxe_httpd_reap_oldest_connection(SXE_HTTPD * self)
{
    unsigned             oldest_id;
    SXE_TIME             oldest_time = 0;
    SXE_TIME             element_time;
    SXE_HTTPD_CONN_STATE oldest_state;
    SXE_HTTPD_CONN_STATE element_state;

    SXEE81("sxe_httpd_reap_oldest_connection(self=%p)", self);

    /* reap connections in BAD state first */
    oldest_id = sxe_pool_get_oldest_element_index(self->requests, SXE_HTTPD_CONN_BAD);

    if (oldest_id == SXE_POOL_NO_INDEX) {
        SXEL20("sxe_httpd_reap_oldest_connection: no bad connections to reap");

        for (element_state = SXE_HTTPD_CONN_IDLE; element_state != SXE_HTTPD_CONN_NUMBER_OF_STATES; element_state++) {
            element_time = sxe_pool_get_oldest_element_time(self->requests, element_state);
            if (oldest_time == 0 || (element_time != 0 && element_time < oldest_time)) {
                oldest_time  = element_time;
                oldest_state = element_state;
            }
        }
        oldest_id = sxe_pool_get_oldest_element_index(self->requests, oldest_state);
    }

    SXEA10(oldest_id != SXE_POOL_NO_INDEX, "The oldest state has an index");
    SXEL91("sxe_httpd_reap_oldest_connection: Going to reap request index %u", oldest_id);
    (*self->on_close)(&self->requests[oldest_id]);
    sxe_httpd_close(&self->requests[oldest_id]);

    SXER80("return");
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
    SXEA10(id != SXE_POOL_NO_INDEX, "Ran out of connections, should not happen");

    request = &self->requests[id];
    memset(request, '\0', sizeof(*request));
    SXE_USER_DATA(this) = request;
    request->sxe = this;
    request->server = self;
    SXE_LIST_CONSTRUCT(&request->out_buffer_list, 0, SXE_BUFFER, node);

    (*self->on_connect)(request);

    /* reap the oldest connection if we have no free connections */
    if (sxe_pool_get_number_in_state(self->requests, SXE_HTTPD_CONN_FREE) == 0) {
        SXEL60("sxe_httpd_event_connect: connection pool full, reaping the oldest connection");
        sxe_httpd_reap_oldest_connection(self);
    }

    SXER80I("return");
}

static void
sxe_httpd_event_close(SXE * this)
{
    SXE_HTTPD_REQUEST * request = SXE_USER_DATA(this);
    SXE_HTTPD_REQUEST * pool    = request->server->requests;
    unsigned            id      = SXE_HTTPD_REQUEST_INDEX(pool, request);

    SXE_UNUSED_PARAMETER(pool);
    SXE_UNUSED_PARAMETER(id);

    SXEE81I("sxe_httpd_event_close(request=%u)", id);
    request->sxe = NULL;    /* Suppress further activity on this connection */
    (*request->server->on_close)(request);

    sxe_httpd_close(request);
    SXER80I("return");
}

static const struct {
    const char *    name;
    unsigned        length;
    SXE_HTTP_METHOD method;
} methods[] = {
    { "GET",    3, SXE_HTTP_METHOD_GET     },
    { "PUT",    3, SXE_HTTP_METHOD_PUT     },
    { "HEAD",   4, SXE_HTTP_METHOD_HEAD    },
    { "POST",   4, SXE_HTTP_METHOD_POST    },
    { "DELETE", 6, SXE_HTTP_METHOD_DELETE  },
    { NULL,     0, SXE_HTTP_METHOD_INVALID }
};

static SXE_HTTP_METHOD
sxe_httpd_parse_method(const char * method_string, unsigned method_length)
{
    SXE_HTTP_METHOD method = SXE_HTTP_METHOD_INVALID;
    unsigned        i;

    SXEE83("%s(method_string='%.*s')", __func__, method_length, method_string);

    for (i = 0; methods[i].name != NULL; i++) {
        if (method_length == methods[i].length && 0 == memcmp(method_string, methods[i].name, methods[i].length)) {
            method = methods[i].method;
            break;
        }
    }

    SXER81("return method=%u", method);
    return method;
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
sxe_httpd_parse_version(const char * version_string, unsigned version_length)
{
    SXE_HTTP_VERSION version = SXE_HTTP_VERSION_UNKNOWN;
    unsigned         i;

    SXEE83("%s(version_string='%.*s')", __func__, version_length, version_string);

    for (i = 0; versions[i].value != NULL; i++) {
        if (version_length == versions[i].length && 0 == memcmp(version_string, versions[i].value, versions[i].length)) {
            version = versions[i].version;
            break;
        }
    }

    SXER81("return version=%u", version);
    return version;
}

/* Note: This function should be called after the whole request line is received */
static SXE_RETURN
sxe_httpd_parse_request_line(SXE_HTTPD_REQUEST * request)
{
    SXE_HTTPD  * server           = request->server;
    SXE        * this             = request->sxe;
    SXE_RETURN   result;
    const char * method_string;
    unsigned     method_length;
    const char * url_string;
    unsigned     url_length;
    const char * version_string;
    unsigned     version_length;

    SXEE81I("sxe_httpd_parse_request_line(request=%p)", request);

    /* Look for the method */
    result = sxe_http_message_parse_next_line_element(&request->message, SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN);
    if (result != SXE_RETURN_OK) {
        goto SXE_ERROR_OUT;
    }
    method_string    = sxe_http_message_get_line_element(       &request->message);
    method_length    = sxe_http_message_get_line_element_length(&request->message);
    request->method  = sxe_httpd_parse_method(method_string, method_length);
    if (request->method == SXE_HTTP_METHOD_INVALID) {
        SXEL23I("%s: Invalid method in HTTP request line: '%.*s'", __func__, method_length, method_string);
        result = SXE_RETURN_ERROR_BAD_MESSAGE;
        goto SXE_ERROR_OUT;
    }
    SXEL93I("Method: '%.*s' (%u)", method_length, method_string, request->method);

    /* Look for the Url */
    result = sxe_http_message_parse_next_line_element(&request->message, SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN);
    if (result != SXE_RETURN_OK) {
        goto SXE_ERROR_OUT;
    }
    url_string    = sxe_http_message_get_line_element(       &request->message);
    url_length    = sxe_http_message_get_line_element_length(&request->message);
    SXEL92I("Url: '%.*s'", url_length, url_string);

    /* Look for the HTTP version */
    result = sxe_http_message_parse_next_line_element(&request->message, SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN);
    if (result != SXE_RETURN_OK) {
        goto SXE_ERROR_OUT;
    }
    version_string    = sxe_http_message_get_line_element(       &request->message);
    version_length    = sxe_http_message_get_line_element_length(&request->message);
    request->version  = sxe_httpd_parse_version(version_string, version_length);
    if (request->version == SXE_HTTP_VERSION_UNKNOWN) {
        SXEL23I("%s: Invalid version in HTTP request line: '%.*s'", __func__, version_length, version_string);
        result = SXE_RETURN_ERROR_BAD_MESSAGE;
        goto SXE_ERROR_OUT;
    }
    SXEL92I("Version: '%.*s'", version_length, version_string);

    /* Look for the end of line */
    result = sxe_http_message_parse_next_line_element(&request->message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE);
    if (result != SXE_RETURN_OK && result != SXE_RETURN_END_OF_FILE) {
        goto SXE_ERROR_OUT;
    }
    else {
        result = SXE_RETURN_OK; /* EOF is okay here */
    }

    (*server->on_request)(request,
                          method_string,  method_length,
                          url_string,     url_length,
                          version_string, version_length);

SXE_ERROR_OUT:
    SXER81I("return result=%s", sxe_return_to_string(result));
    return result;
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
    unsigned            buffer_left          = 0;
    int                 response_status_code = 400;
    const char        * response_reason      = "Bad request";
    SXE_RETURN          result;
    const char        * key;
    unsigned            key_length;
    const char        * value;
    unsigned            value_length;
    unsigned            i;
    unsigned            request_id;

    SXEE82I("sxe_httpd_event_read(request=%p,additional_length=%d)", request, additional_length);
    SXE_UNUSED_PARAMETER(additional_length);
    message      = &request->message;
    server       = request->server;
    request_pool = server->requests;
    request_id   = SXE_HTTPD_REQUEST_INDEX(request_pool, request);
    state        = sxe_pool_index_to_state(request_pool, request_id);

    switch (state) {
    case SXE_HTTPD_CONN_IDLE:
        SXEL90I("state IDLE -> LINE");
        sxe_http_message_construct(message, SXE_BUF(this), SXE_BUF_USED(this));
        sxe_pool_set_indexed_element_state(request_pool, request_id, state, SXE_HTTPD_CONN_REQ_LINE);
        state = SXE_HTTPD_CONN_REQ_LINE;
        /* fall through */

    case SXE_HTTPD_CONN_REQ_LINE:
        sxe_http_message_increase_buffer_length(message, SXE_BUF_USED(this));
        start  = SXE_BUF(this);
        length = SXE_BUF_USED(this);
        end    = sxe_strncspn(start, "\n", length);    /* TODO: Be smarter about not rescanning */

        if (end == NULL) {
            if (length == SXE_BUF_SIZE) {
                response_status_code = 414;
                response_reason = "Request-URI too large";
                goto SXE_ERROR_OUT;
            }
            goto SXE_EARLY_OUT;    /* Buffer is not full. Try again when there is more data */
        }

        if (*--end != '\r') {
            goto SXE_ERROR_OUT;
        }

        if ((result = sxe_httpd_parse_request_line(request)) != SXE_RETURN_OK) {
            goto SXE_ERROR_OUT;
        }

        SXEL90I("state LINE -> REQ_HEADERS");
        sxe_pool_set_indexed_element_state(request_pool, request_id, state, SXE_HTTPD_CONN_REQ_HEADERS);
        state = SXE_HTTPD_CONN_REQ_HEADERS;

        /* fall through */

    case SXE_HTTPD_CONN_REQ_HEADERS:
        sxe_http_message_increase_buffer_length(message, SXE_BUF_USED(this));

        /* While the end-of-headers has not yet been reached
         */
        while ((result = sxe_http_message_parse_next_header(message)) != SXE_RETURN_END_OF_FILE) {
            if (result == SXE_RETURN_ERROR_BAD_MESSAGE) {
                goto SXE_ERROR_OUT;
            }

            if ((consumed = sxe_http_message_get_ignore_length(message))) {
                sxe_buf_consume(this, consumed);
                if ((buffer_left = sxe_http_message_get_buffer_length(message))) {
                    SXEL92I("%u bytes ignored , move the remaining %u bytes to the beginning of buffer", consumed, buffer_left);
                    sxe_http_message_buffer_shift_ignore_length(message);
                }
                goto SXE_EARLY_OUT;
            }

            if (result == SXE_RETURN_WARN_WOULD_BLOCK) {
                /* If the buffer is full
                 */
                if (SXE_BUF_USED(this) == SXE_BUF_SIZE) {
                    consumed = sxe_http_message_consume_parsed_headers(message); /* COVERAGE EXCLUSION - TODO: WIN32 COVERAGE */

                    if (consumed == 0) {
                        SXEL32I("%s: Found a header which is too big(>=%u), ignore it", __func__, SXE_BUF_SIZE);
                        sxe_buf_clear(this);
                        sxe_http_message_set_ignore_line(message);
                        goto SXE_EARLY_OUT;
                    }

                    SXEL90I("About to consume the headers processed so far");
                    sxe_buf_consume(this, consumed);
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
        sxe_pool_set_indexed_element_state(request_pool, request_id, state, SXE_HTTPD_CONN_REQ_BODY);
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
            sxe_pool_set_indexed_element_state(request_pool, request_id, state, SXE_HTTPD_CONN_REQ_RESPONSE);
            state = SXE_HTTPD_CONN_REQ_RESPONSE;
            (*server->on_respond)(request);
        }

        goto SXE_EARLY_OUT;

    case SXE_HTTPD_CONN_REQ_RESPONSE:
        goto SXE_EARLY_OUT;

    case SXE_HTTPD_CONN_BAD:
        SXEL61I("Discarding %u bytes of data from a client in CONN_BAD state", SXE_BUF_USED(this));
        goto SXE_EARLY_OUT;

    default:
        SXEA11I(0, "Internal error: read called during unexpected state %s", sxe_httpd_state_to_string(state));    /* COVERAGE EXCLUSION: Can't happen */
    }

SXE_ERROR_OUT:
    sxe_buf_clear(this);
    sxe_httpd_response_simple(request, NULL, NULL, response_status_code, response_reason, NULL, NULL);
    /* Set the state to BAD so from now on it will just early out (sink mode) */
    sxe_pool_set_indexed_element_state(request_pool, request_id, SXE_HTTPD_CONN_IDLE, SXE_HTTPD_CONN_BAD);
    consumed = 0;

SXE_EARLY_OUT:
    if (consumed > 0) {
        sxe_buf_resume(this, SXE_BUF_RESUME_IMMEDIATE);    /* If there's more data, deliver it */
    }

    SXER80I("return");
}

SXE_RETURN
sxe_httpd_response_start(SXE_HTTPD_REQUEST * request, int code, const char *status)
{
    SXE_RETURN   result = SXE_RETURN_NO_UNUSED_ELEMENTS;
    SXE        * this = request->sxe;
    SXE_HTTPD  * self = request->server;
    SXE_BUFFER * buffer;
    int          len;

    SXEE83I("sxe_httpd_response_start(request=%p,code=%d,status=%s)", request, code, status);

    buffer = sxe_httpd_get_buffer(self);
    if (buffer) {
        len = snprintf(buffer->space, self->buffersize, "HTTP/1.1 %d %s\r\n", code, status);
        SXEA12I(len > 0 && (unsigned)len < self->buffersize, "Status message is longer (%u) than the buffer size (%u)", len, self->buffersize);
        buffer->len = len;
        sxe_list_push(&request->out_buffer_list, buffer);
        result = SXE_RETURN_OK;
    }

    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

SXE_RETURN
sxe_httpd_response_header(SXE_HTTPD_REQUEST * request, const char *header, const char *value, unsigned length)
{
    SXE_RETURN   result = SXE_RETURN_NO_UNUSED_ELEMENTS;
    SXE_HTTPD  * self   = request->server;
    SXE        * this   = request->sxe;
    unsigned     vlen   = length ? length : strlen(value);
    SXE_BUFFER * buffer;
    int          len;

    SXEE84I("sxe_httpd_response_header(request=%p,header=%s,value=%.*s)", request, header, vlen, value);
    SXEA80I(!SXE_LIST_IS_EMPTY(&request->out_buffer_list), "sxe_httpd_response_header() called before sxe_httpd_response_start()");

    /* Try to write the header into the existing buffer */
    buffer = sxe_list_peek_tail(&request->out_buffer_list);
    if (buffer->ptr == buffer->space) {
        len = snprintf(buffer->space + buffer->len, self->buffersize - buffer->len, "%s: %.*s\r\n", header, vlen, value);
        if (len > 0 && (unsigned)len < self->buffersize - buffer->len) {
            buffer->len += len;
            result = SXE_RETURN_OK;
            SXEL83I("sxe_httpd_response_header(): wrote %u bytes to buffer %u: now %u bytes", len, SXE_LIST_GET_LENGTH(&request->out_buffer_list), buffer->len);
            goto SXE_EARLY_OUT;
        }
    }

    /* Write the header into its own buffer */
    buffer = sxe_httpd_get_buffer(self);
    if (buffer) {
        len = snprintf(buffer->space, self->buffersize, "%s: %.*s\r\n", header, vlen, value);
        SXEA12I(len > 0 && (unsigned)len < self->buffersize, "Header is longer (%u) than the buffer size (%u)", len, self->buffersize);
        buffer->len = len;
        sxe_list_push(&request->out_buffer_list, buffer);
        result = SXE_RETURN_OK;
    }

SXE_EARLY_OUT:
    SXER81I("return %s", sxe_return_to_string(result));
    return result;
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

SXE_RETURN
sxe_httpd_response_raw(SXE_HTTPD_REQUEST *request, const char *chunk, unsigned length)
{
    SXE_RETURN   result = SXE_RETURN_NO_UNUSED_ELEMENTS;
    SXE        * this = request->sxe;
    SXE_HTTPD  * self = request->server;
    SXE_BUFFER * buffer;

    SXE_UNUSED_PARAMETER(this);

    SXEE84I("%s(request=%p, chunk=%p, length=%u)", __func__, request, chunk, length);

    buffer = sxe_httpd_get_buffer(self);
    if (buffer) {
        result = SXE_RETURN_OK;
        request->out_eoh = true; /* NOTE: you're on your own! */
        buffer->ptr = chunk;
        buffer->len = length;
        sxe_list_push(&request->out_buffer_list, buffer);
    }

    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

static SXE_RETURN
sxe_httpd_response_eoh(SXE_HTTPD_REQUEST *request)
{
    SXE_RETURN   result = SXE_RETURN_NO_UNUSED_ELEMENTS;
    SXE        * this = request->sxe;
    SXE_HTTPD  * self = request->server;
    SXE_BUFFER * buffer;

    SXE_UNUSED_PARAMETER(this);

    SXEE82I("%s(request=%p)", __func__, request);

    /* Try to write the header into the existing buffer */
    buffer = sxe_list_peek_tail(&request->out_buffer_list);
    if (buffer->ptr == buffer->space) {
        if (self->buffersize - buffer->len >= 2) {
            buffer->space[buffer->len + 0] = '\r';
            buffer->space[buffer->len + 1] = '\n';
            buffer->len += 2;
            SXEL82I("sxe_httpd_response_header(): wrote EOF to buffer %u: now u bytes", SXE_LIST_GET_LENGTH(&request->out_buffer_list), buffer->len);
            goto SXE_SUCCESS_OUT;
        }
    }

    buffer = sxe_httpd_get_buffer(self);
    if (buffer) {
        buffer->ptr = "\r\n";
        buffer->len = 2;
        sxe_list_push(&request->out_buffer_list, buffer);
SXE_SUCCESS_OUT:
        request->out_eoh = true;
        result = SXE_RETURN_OK;
    }

    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

SXE_RETURN
sxe_httpd_response_chunk(SXE_HTTPD_REQUEST *request, const char *chunk, unsigned length)
{
    SXE_RETURN   result = SXE_RETURN_NO_UNUSED_ELEMENTS;
    SXE        * this = request->sxe;
    SXE_HTTPD  * self = request->server;
    SXE_BUFFER * buffer;

    SXE_UNUSED_PARAMETER(this);

    SXEE84I("%s(request=%p, chunk=%p, length=%u)", __func__, request, chunk, length);
    SXEA80I(!SXE_LIST_IS_EMPTY(&request->out_buffer_list), "sxe_httpd_response_chunk() called before sxe_httpd_response_start()");

    if (!request->out_eoh) {
        if (sxe_httpd_response_eoh(request) != SXE_RETURN_OK) {
            goto SXE_EARLY_OUT;                                                 /* Coverage exclusion: todo: test running out of send buffers */
        }
    }

    buffer = sxe_httpd_get_buffer(self);
    if (buffer) {
        buffer->ptr = chunk;
        buffer->len = length;
        sxe_list_push(&request->out_buffer_list, buffer);
        result = SXE_RETURN_OK;
    }

SXE_EARLY_OUT:
    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

#ifndef WINDOWS_NT
static void
sxe_httpd_event_sendfile_done(SXE * this, SXE_RETURN final_result)
{
    SXE_HTTPD_REQUEST * request = SXE_USER_DATA(this);

    SXEE83I("%s(request=%p, final_result=%s)", __func__, request, sxe_return_to_string(final_result));
    if (request->on_sent_handler) {
        (*request->on_sent_handler)(request, final_result, request->on_sent_userdata);
    }
    SXER80I("return");
}

static void
sxe_httpd_event_sendfile_ready(SXE * this, SXE_RETURN final_result)
{
    SXE_HTTPD_REQUEST * request = SXE_USER_DATA(this);
    SXE_HTTPD         * self    = request->server;
    SXE_BUFFER        * buffer;

    SXE_UNUSED_PARAMETER(final_result);
    SXEE83I("%s(request=%p, final_result=%s)", __func__, request, sxe_return_to_string(final_result));

    for (buffer = sxe_list_shift(&request->out_buffer_list); buffer; buffer = sxe_list_shift(&request->out_buffer_list)) {
        unsigned id = SXE_HTTPD_BUFFER_INDEX(self, buffer);
        sxe_pool_set_indexed_element_state(self->buffers, id, SXE_HTTPD_BUFFER_USED, SXE_HTTPD_BUFFER_FREE);
    }

    sxe_sendfile(this, request->sendfile_fd, &request->sendfile_offset, request->sendfile_length, sxe_httpd_event_sendfile_done);
    SXER80I("return");
}

SXE_RETURN
sxe_httpd_response_sendfile(SXE_HTTPD_REQUEST *request, int fd, unsigned length, sxe_httpd_on_sent_handler handler, void *user_data)
{
    SXE_RETURN   result = SXE_RETURN_NO_UNUSED_ELEMENTS;
    SXE        * this   = request->sxe;

    SXEE86I("%s(request=%p,fd=%d,length=%u,handler=%p,user_data=%p)", __func__, request, fd, length, handler, user_data);
    SXEA80I(!SXE_LIST_IS_EMPTY(&request->out_buffer_list), "sxe_httpd_response_sendfile() called before sxe_httpd_response_start()");

    request->on_sent_handler  = handler;
    request->on_sent_userdata = user_data;
    request->sendfile_fd      = fd;
    request->sendfile_length  = length;
    request->sendfile_offset  = lseek(fd, 0, SEEK_CUR);

    if (!request->out_eoh) {
        if (sxe_httpd_response_eoh(request) != SXE_RETURN_OK) {
            goto SXE_EARLY_OUT;                                                 /* Coverage exclusion: todo: test running out of send buffers */
        }
    }

    result = sxe_send_buffers(this, &request->out_buffer_list, sxe_httpd_event_sendfile_ready);

    if (result != SXE_RETURN_IN_PROGRESS) {
        sxe_httpd_event_sendfile_ready(this, result);
    }

SXE_EARLY_OUT:
    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}
#endif

static void
sxe_httpd_event_response_done(SXE * this, SXE_RETURN final_result)
{
    SXE_HTTPD_REQUEST * request      = SXE_USER_DATA(this);
    SXE_HTTPD         * self         = request->server;
    SXE_HTTPD_REQUEST * request_pool = self->requests;
    unsigned            request_id   = SXE_HTTPD_REQUEST_INDEX(request_pool, request);
    unsigned            state        = sxe_pool_index_to_state(request_pool, request_id);
    SXE_BUFFER        * buffer;

    SXEE83I("%s(request=%p, final_result=%s)", __func__, request, sxe_return_to_string(final_result));

    for (buffer = sxe_list_shift(&request->out_buffer_list); buffer; buffer = sxe_list_shift(&request->out_buffer_list)) {
        unsigned id = SXE_HTTPD_BUFFER_INDEX(self, buffer);
        sxe_pool_set_indexed_element_state(self->buffers, id, SXE_HTTPD_BUFFER_USED, SXE_HTTPD_BUFFER_FREE);
    }

    if (request->on_sent_handler) {
        (*request->on_sent_handler)(request, final_result, request->on_sent_userdata);
    }

    sxe_httpd_clear_request(request);
    sxe_pool_set_indexed_element_state(request_pool, request_id, state, SXE_HTTPD_CONN_IDLE);
    sxe_buf_resume(this, SXE_BUF_RESUME_IMMEDIATE);

    SXER80I("return");
}

SXE_RETURN
sxe_httpd_response_end(SXE_HTTPD_REQUEST *request, sxe_httpd_on_sent_handler handler, void *user_data)
{
    SXE_RETURN          result = SXE_RETURN_NO_UNUSED_ELEMENTS;
    SXE               * this = request->sxe;

    SXE_UNUSED_PARAMETER(this);
    SXEE81I("sxe_httpd_response_end(request=%p)", request);

    request->on_sent_handler  = handler;
    request->on_sent_userdata = user_data;

    if (!request->out_eoh) {
        if (sxe_httpd_response_eoh(request) != SXE_RETURN_OK) {
            goto SXE_EARLY_OUT;                                                 /* Coverage exclusion: todo: test running out of send buffers */
        }
    }

    result = sxe_send_buffers(this, &request->out_buffer_list, sxe_httpd_event_response_done);

    if (result != SXE_RETURN_IN_PROGRESS) {
        sxe_httpd_event_response_done(this, result);
    }

SXE_EARLY_OUT:
    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

SXE *
sxe_httpd_listen(SXE_HTTPD * self, const char *address, unsigned short port)
{
    SXE * this;

    SXEE82("sxe_httpd_listen(address=%s, port=%hu)", address, port);

    if ((this = sxe_new_tcp(NULL, address, port, sxe_httpd_event_connect, sxe_httpd_event_read, sxe_httpd_event_close)) == NULL) {
        SXEL20("sxe_httpd_listen: Failed to allocate a SXE for TCP");           /* Coverage Exclusion: No need to test */
        goto SXE_ERROR_OUT;                                                     /* Coverage Exclusion: No need to test */
    }

    SXE_USER_DATA(this) = self;

    if (sxe_listen(this) != SXE_RETURN_OK) {
        SXEL22("sxe_httpd_listen: Failed to listen on address %s, port %hu)", address, port);    /* Coverage Exclusion: No need to test */
        sxe_close(this);                                                        /* Coverage Exclusion: No need to test */
        this = NULL;                                                            /* Coverage Exclusion: No need to test */
    }

SXE_ERROR_OUT:
    SXER81("return %p", this);
    return this;
}

#ifndef WINDOWS_NT
SXE *
sxe_httpd_listen_pipe(SXE_HTTPD * self, const char * path)
{
    SXE * this;

    SXEE81("sxe_httpd_listen_pipe(path=%s)", path);

    if ((this = sxe_new_pipe(NULL, path, sxe_httpd_event_connect, sxe_httpd_event_read, sxe_httpd_event_close)) == NULL) {
        SXEL20("sxe_httpd_listen_pipe: Failed to allocate a SXE for pipes");    /* Coverage Exclusion: No need to test */
        goto SXE_ERROR_OUT;                                                     /* Coverage Exclusion: No need to test */
    }

    SXE_USER_DATA(this) = self;

    if (sxe_listen(this) != SXE_RETURN_OK) {
        SXEL21("sxe_httpd_listen_pipe: Failed to listen on path %s)", path);    /* Coverage Exclusion: No need to test */
        sxe_close(this);                                                        /* Coverage Exclusion: No need to test */
        this = NULL;                                                            /* Coverage Exclusion: No need to test */
    }

SXE_ERROR_OUT:
    SXER81("return %p", this);
    return this;
}
#endif

void
sxe_httpd_construct(SXE_HTTPD * self, int connections, int chunks, unsigned chunksize, unsigned options)
{
    SXEE86("%s(self=%p, connections=%u, chunks=%u, chunksize=%u, options=%x)", __func__, self, connections, chunks, chunksize, options);

    SXE_UNUSED_PARAMETER(options);

    SXEA10(connections >    1, "Requires minimum TWO connections");
    SXEA10(chunksize   >= 512, "Requires minimum chunksize of 512");
    SXEA10(chunks      >    9, "Requires minimum 10 chunks");

    self->requests   = sxe_pool_new("httpd", connections, sizeof(SXE_HTTPD_REQUEST), SXE_HTTPD_CONN_NUMBER_OF_STATES, SXE_POOL_OPTION_UNLOCKED | SXE_POOL_OPTION_TIMED);
    sxe_pool_set_state_to_string(self->requests, sxe_httpd_state_to_string);

    self->buffersize = chunksize;
    self->buffers    = sxe_pool_new("httpd-buffers", chunks, SXE_HTTPD_BUFFER_SIZE(self), SXE_HTTPD_BUFFER_NUMBER_OF_STATES, SXE_POOL_OPTION_UNLOCKED);

    self->on_connect = sxe_httpd_default_connect_handler;
    self->on_request = sxe_httpd_default_request_handler;
    self->on_header  = sxe_httpd_default_header_handler;
    self->on_eoh     = sxe_httpd_default_eoh_handler;
    self->on_body    = sxe_httpd_default_body_handler;
    self->on_respond = sxe_httpd_default_respond_handler;
    self->on_close   = sxe_httpd_default_close_handler;

    SXER80("return");
}

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
