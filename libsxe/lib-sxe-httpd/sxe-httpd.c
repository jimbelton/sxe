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

#include "sxe-httpd.h"
#include "sxe-log.h"
#include "sxe-pool.h"

/* Pointer substraction is signed but index is unsigned */
#define SXE_HTTPD_REQUEST_INDEX(base_ptr, object_ptr) ((SXE_CAST(uintptr_t, object_ptr) - SXE_CAST(uintptr_t, base_ptr)) / sizeof(SXE_HTTPD_REQUEST))

#define SXE_HTTPD_INTERNAL_BUFFER_SIZE(s)     (sizeof(SXE_BUFFER) + (s)->buffer_size)
#define SXE_HTTPD_INTERNAL_BUFFER(s, idx)     (SXE_BUFFER *)(((char *)(s)->buffers) + ((idx) * SXE_HTTPD_INTERNAL_BUFFER_SIZE(s)))
#define SXE_HTTPD_INTERNAL_BUFFER_INDEX(s, b) ((SXE_CAST(uintptr_t, (b)) - SXE_CAST(uintptr_t, (s)->buffers)) / SXE_HTTPD_INTERNAL_BUFFER_SIZE((s)))

typedef enum {
    SXE_HTTPD_BUFFER_FREE,
    SXE_HTTPD_BUFFER_USED,
    SXE_HTTPD_BUFFER_NUMBER_OF_STATES
} SXE_HTTPD_BUFFER_STATE;

extern struct ev_loop * sxe_private_main_loop;    /* Defer into the same ev loop as SXE */

static void
sxe_httpd_default_respond_handler(SXE_HTTPD_REQUEST * request)            /* Coverage Exclusion - todo: win32 coverage */
{
    SXE * this = request->sxe;                                            /* Coverage Exclusion - todo: win32 coverage */
    SXE_UNUSED_PARAMETER(this);
    SXEE7I("(request=%p)", request);
    SXEL3I("warning: default respond handler: generating 404 Not Found"); /* Coverage Exclusion - todo: win32 coverage */
    sxe_httpd_response_simple(request, NULL, NULL, 404, "Not Found", NULL, HTTPD_CONNECTION_CLOSE_HEADER, HTTPD_CONNECTION_CLOSE_VALUE, NULL);
    SXER7I("return");
}                                                                         /* Coverage Exclusion - todo: win32 coverage */

static void
sxe_httpd_default_close_handler(SXE_HTTPD_REQUEST * request)              /* Coverage Exclusion - todo: win32 coverage */
{                                                                         /* Coverage Exclusion: TBD */
    SXE * this = request->sxe;                                            /* Coverage Exclusion: TBD */
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXEL7I("Closing HTTPD request %p: no on_close handler", request);
}                                                                         /* Coverage Exclusion: TBD */

/** Set the on_connect handler for an HTTP server, which is called on the end of each request line (default: no event)
 */
sxe_httpd_connect_handler
sxe_httpd_set_connect_handler(SXE_HTTPD * httpd, sxe_httpd_connect_handler new_handler)
{
    sxe_httpd_connect_handler old_handler = httpd->on_connect ;
    httpd->on_connect                     = new_handler;
    return old_handler;
}

/** Set the on_request handler for an HTTP server, which is called on the end of each request line (default: no event)
 */
sxe_httpd_request_handler
sxe_httpd_set_request_handler(SXE_HTTPD * httpd, sxe_httpd_request_handler new_handler)
{
    sxe_httpd_request_handler old_handler = httpd->on_request ;
    httpd->on_request                     = new_handler;
    return old_handler;
}

/** Set the on_header handler for an HTTP server, which is called on the end of each HTTP header (default: no event)
 */
sxe_httpd_header_handler
sxe_httpd_set_header_handler(SXE_HTTPD * httpd, sxe_httpd_header_handler new_handler)
{
    sxe_httpd_header_handler old_handler = httpd->on_header;
    httpd->on_header                     = new_handler;
    return old_handler;
}

/** Set the on_eoh handler for an HTTP server, which is called on the end all HTTP headers (default: no event)
 */
sxe_httpd_eoh_handler
sxe_httpd_set_eoh_handler(SXE_HTTPD * httpd, sxe_httpd_eoh_handler new_handler)
{
    sxe_httpd_eoh_handler old_handler = httpd->on_eoh ;
    httpd->on_eoh                     = new_handler;
    return old_handler;
}

/** Set the on_body handler for an HTTP server, which is called on each recieved body chunk (default: no event)
 */
sxe_httpd_body_handler
sxe_httpd_set_body_handler(SXE_HTTPD *httpd, sxe_httpd_body_handler new_handler)
{
    sxe_httpd_body_handler old_handler = httpd->on_body ;
    httpd->on_body                     = new_handler;
    return old_handler;
}

/** Set the on_body handler for an HTTP server, which is called on each recieved body chunk (default: reply with a 404)
 */
sxe_httpd_respond_handler
sxe_httpd_set_respond_handler(SXE_HTTPD *httpd, sxe_httpd_respond_handler new_handler)
{
    sxe_httpd_respond_handler old_handler = httpd->on_respond ;
    httpd->on_respond                     = new_handler != NULL ? new_handler : sxe_httpd_default_respond_handler;
    return old_handler;
}

/** Set the on_close handler for an HTTP server, which is called on each recieved body chunk (default: no action)
 */
sxe_httpd_close_handler
sxe_httpd_set_close_handler(SXE_HTTPD *httpd, sxe_httpd_close_handler new_handler)
{
    sxe_httpd_close_handler old_handler = httpd->on_close ;
    httpd->on_close                     = new_handler != NULL ? new_handler : sxe_httpd_default_close_handler;
    return old_handler;
}

const char *
sxe_httpd_request_state_to_string(unsigned state)
{
    switch (state) {
    case SXE_HTTPD_REQUEST_STATE_FREE:                     return "FREE";
    case SXE_HTTPD_REQUEST_STATE_IDLE:                     return "IDLE";
    case SXE_HTTPD_REQUEST_STATE_BAD:                      return "BAD";
    case SXE_HTTPD_REQUEST_STATE_ACTIVE:                   return "ACTIVE";
    case SXE_HTTPD_REQUEST_STATE_DEFERRED:                 return "DEFERRED";
    default:                                               return NULL;    /* not a state - just keep the compiler happy  */
    }
}

const char *
sxe_httpd_request_get_state_and_substate_as_string(SXE_HTTPD_REQUEST * request)
{
    switch (sxe_pool_index_to_state(request->server->requests, request - request->server->requests)) {
    case SXE_HTTPD_REQUEST_STATE_FREE:                     return "FREE";                     /* COVERAGE EXCLUSION - For debug logging only */
    case SXE_HTTPD_REQUEST_STATE_IDLE:                     return "IDLE";
    case SXE_HTTPD_REQUEST_STATE_BAD:                      return "BAD";
    case SXE_HTTPD_REQUEST_STATE_ACTIVE:
        switch (request->substate) {
        case SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_LINE:    return "ACTIVE/RECEIVING_LINE";    /* COVERAGE EXCLUSION - For debug logging only */
        case SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_HEADERS: return "ACTIVE/RECEIVING_HEADERS"; /* COVERAGE EXCLUSION - For debug logging only */
        case SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_BODY:    return "ACTIVE/RECEIVING_BODY";    /* COVERAGE EXCLUSION - For debug logging only */
        case SXE_HTTPD_REQUEST_SUBSTATE_SENDING_LINE:      return "ACTIVE/SENDING_LINE";      /* COVERAGE EXCLUSION - For debug logging only */
        case SXE_HTTPD_REQUEST_SUBSTATE_SENDING_HEADERS:   return "ACTIVE/SENDING_HEADERS";   /* COVERAGE EXCLUSION - For debug logging only */
        case SXE_HTTPD_REQUEST_SUBSTATE_SENDING_BODY:      return "ACTIVE/SENDING_BODY";      /* COVERAGE EXCLUSION - For debug logging only */
        case SXE_HTTPD_REQUEST_SUBSTATE_RESPONSE_DRAINING: return "ACTIVE/RESPONSE_DRAINING"; /* COVERAGE EXCLUSION - For debug logging only */
        case SXE_HTTPD_REQUEST_SUBSTATE_RESPONSE_DRAINED:  return "ACTIVE/RESPONSE_DRAINED";  /* COVERAGE EXCLUSION - For debug logging only */
        default:                                           return NULL;                       // not a substate - just keeps the compiler happy
        }
    case SXE_HTTPD_REQUEST_STATE_DEFERRED:                 return "DEFERRED";                 /* COVERAGE EXCLUSION - For debug logging only */
    default:                                               return NULL;                       // not a state    - just keeps the compiler happy
    }
}

static void
sxe_httpd_clear_request(SXE_HTTPD_REQUEST * request)
{
    SXE * this = request->sxe;

    SXE_UNUSED_PARAMETER(this);
    SXEE6I("(request=%p)", request);

    request->substate          = SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_LINE;
    request->in_content_length = 0;
    request->in_content_seen   = 0;
    request->paused            = false;

    if (request->out_buffer != NULL) {
        sxe_buffer_clear(request->out_buffer);
        request->out_buffer = NULL;
    }

    SXER6I("return");
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

    SXEE6I("(request=%p, request_id=%u)", request, request_id);
    sxe_httpd_clear_request(request);

    if (request->sxe != NULL) {
        sxe_close(request->sxe);
    }

    state = sxe_pool_index_to_state(requests, request_id);
    sxe_pool_set_indexed_element_state(requests, request_id, state, SXE_HTTPD_REQUEST_STATE_FREE);
    SXER6I("return");
}

/**
 * Send a simple HTTP response
 *
 * @param request     Request to respond to
 * @param on_complete Function to call back when the response has been completely sent or NULL to fire and forget
 * @param user_data   User data passed back to on_complete
 * @param code        HTTP response code (e.g. 200)
 * @param status      HTTP status (e.g. OK)
 * @param body        The body of the response or NULL if there is no body data
 * @param ...         NULL terminated list of '\0' terminated header key and value (paired) strings (list must be even length)
 *
 * @return SXE_RETURN_OK if the entire response was copied to the TCP send window, SXE_RETURN_IN_PROGRESS if it was queued,
 *         SXE_RETURN_NO_UNUSED_ELEMENTS if the HTTP server ran out of send buffers, or a sxe_send_buffer() error.
 *
 * @exception Aborts if a header key is followed by NULL rather than a value
 */
SXE_RETURN
sxe_httpd_response_simple(SXE_HTTPD_REQUEST * request, void (*on_complete)(struct SXE_HTTPD_REQUEST *, SXE_RETURN, void *),
                          void * user_data, int code, const char * status, const char * body, ...)
{
    SXE        * this   = request->sxe;
    SXE_RETURN   result = SXE_RETURN_ERROR_INTERNAL;
    va_list      headers;
    const char * name;
    const char * value;
    unsigned     length;

    SXE_UNUSED_PARAMETER(this);
    SXEE6I("(request=%p,code=%d,status=%s,body='%s',...)", request, code, status, body);

    if ((result = sxe_httpd_response_start(request, code, status)) != SXE_RETURN_OK && result != SXE_RETURN_IN_PROGRESS) {
        goto SXE_ERROR_OUT;
    }

    va_start(headers, body);

    while ((name = va_arg(headers, const char *)) != NULL) {
        SXEA1((value = va_arg(headers, const char *)) != NULL, "%s: header '%s' has no value", __func__, name);

        if ((result = sxe_httpd_response_header(request, name, value, 0)) != SXE_RETURN_OK && result != SXE_RETURN_IN_PROGRESS) {
            va_end(headers);     /* COVERAGE EXCLUSION: TODO: Cover send failure (and buffer exhaustion) */
            goto SXE_ERROR_OUT;  /* COVERAGE EXCLUSION: TODO: Cover send failure (and buffer exhaustion) */
        }
    }

    va_end(headers);
    length = body == NULL ? 0 : strlen(body);

    if ((result = sxe_httpd_response_content_length(request, length)) != SXE_RETURN_OK && result != SXE_RETURN_IN_PROGRESS) {
        goto SXE_ERROR_OUT;      /* COVERAGE EXCLUSION: TODO: Cover send failure (and buffer exhaustion) */
    }

    if (body != NULL && (result = sxe_httpd_response_copy_body_data(request, body, length)) != SXE_RETURN_OK
     && result != SXE_RETURN_IN_PROGRESS)
    {
        goto SXE_ERROR_OUT;      /* COVERAGE EXCLUSION: TODO: Cover send failure (and buffer exhaustion) */
    }

    result = sxe_httpd_response_end(request, on_complete, user_data);

SXE_ERROR_OUT:
    SXER6I("return result=%s", sxe_return_to_string(result));
    return result;
}

/* TODO: inline */

static void
sxe_httpd_reap_oldest_connection(SXE_HTTPD * httpd)
{
    SXE_TIME                element_time;
    SXE_HTTPD_REQUEST_STATE element_state;
    unsigned                oldest_id;
    SXE_TIME                oldest_time  = 0;
    SXE_HTTPD_REQUEST_STATE oldest_state = SXE_HTTPD_REQUEST_STATE_BAD;

    SXEE6("(httpd=%p)", httpd);

    /* reap connections in BAD state first */
    oldest_id = sxe_pool_get_oldest_element_index(httpd->requests, SXE_HTTPD_REQUEST_STATE_BAD);

    if (oldest_id == SXE_POOL_NO_INDEX) {
        SXEL2("%s: no bad connections to reap", __func__);
        oldest_state = SXE_HTTPD_REQUEST_STATE_IDLE;

        for (element_state = oldest_state; element_state != SXE_HTTPD_REQUEST_STATE_NUMBER_OF_STATES; element_state++) {
            element_time = sxe_pool_get_oldest_element_time(httpd->requests, element_state);

            if (oldest_time == 0 || (element_time != 0 && element_time < oldest_time)) {
                oldest_time  = element_time;
                oldest_state = element_state;
            }
        }

        oldest_id = sxe_pool_get_oldest_element_index(httpd->requests, oldest_state);
    }

    SXEA1(oldest_id != SXE_POOL_NO_INDEX, "The oldest state has an index");
    SXEL7("%s: Going to reap request index %u", __func__, oldest_id);

    /* check if we already called on_close (when transitioning to state "BAD")
     */
    if (oldest_state != SXE_HTTPD_REQUEST_STATE_BAD) {
        (*httpd->on_close)(&httpd->requests[oldest_id]);
    }

    sxe_httpd_close(&httpd->requests[oldest_id]);
    SXER6("return");
}

static void
sxe_httpd_event_connect(SXE * this)
{
    SXE_HTTPD         * httpd = SXE_USER_DATA(this);
    SXE_HTTPD_REQUEST * request;
    unsigned            id;

    SXEE6I("(this=%p)", this);
    id = sxe_pool_set_oldest_element_state(httpd->requests, SXE_HTTPD_REQUEST_STATE_FREE, SXE_HTTPD_REQUEST_STATE_IDLE);
    SXEA1(id != SXE_POOL_NO_INDEX, "%s: Ran out of connections, should not happen", __func__);

    request = &httpd->requests[id];
    memset(request, '\0', sizeof(*request));
    SXE_USER_DATA(this)  = request;
    request->ident[0]    = 'C';
    request->ident[1]    = 'L';
    request->ident[2]    = 'N';
    request->ident[3]    = 'T';
    request->sxe         = this;
    request->server      = httpd;
    request->out_buffer  = NULL;
    request->sendfile_fd = -1;

    if (httpd->on_connect != NULL) {
        (*httpd->on_connect)(request);
    }

    /* reap the oldest connection if we have no free connections
     */
    if (sxe_pool_get_number_in_state(httpd->requests, SXE_HTTPD_REQUEST_STATE_FREE) == 0) {
        SXEL6("Connection pool is full: reaping the oldest connection");
        sxe_httpd_reap_oldest_connection(httpd);
    }

    SXER6I("return");
}

static void
sxe_httpd_event_close(SXE * this)
{
    SXE_HTTPD_REQUEST * request = SXE_USER_DATA(this);
    SXE_HTTPD_REQUEST * pool    = NULL;
    unsigned            id      = SXE_POOL_NO_INDEX;

    SXEE6I("(request=%p)", request);

    if (request->ident[0] != 'C' ||
        request->ident[1] != 'L' ||
        request->ident[2] != 'N' ||
        request->ident[3] != 'T')
    {
        SXEL3I("Close event caught before corresponding connect -- connection timed out or SSL error?"); /* Coverage exclusion: Can't reliably make this condition happen on all OS's */
        goto SXE_EARLY_OUT;                                                                              /* Coverage exclusion: Can't reliably make this condition happen on all OS's */
    }

    request->sxe = NULL;    /* Suppress further activity on this connection */
    pool         = request->server->requests;
    id           = SXE_HTTPD_REQUEST_INDEX(pool, request);

    /* check if we already called on_close (when transitioning to state "BAD") */
    if (sxe_pool_index_to_state(pool, id) != SXE_HTTPD_REQUEST_STATE_BAD) {
        (*request->server->on_close)(request);
    }

    sxe_httpd_close(request);

SXE_EARLY_OUT:
    SXER6I("return");
}

static void
sxe_httpd_give_buffer(SXE_HTTPD * httpd, SXE_BUFFER * buffer)
{
    unsigned id;

    SXEE6("(httpd=%p, buffer=%p)", httpd, buffer);

    if (buffer < SXE_HTTPD_INTERNAL_BUFFER(httpd, 0) || buffer > SXE_HTTPD_INTERNAL_BUFFER(httpd, httpd->buffer_count - 1)) {
        SXEL6("This is a user SXE_HTTPD_BUFFER");
        goto SXE_EARLY_OUT;
    }

    id = SXE_HTTPD_INTERNAL_BUFFER_INDEX(httpd, buffer);
    sxe_pool_set_indexed_element_state(httpd->buffers, id, SXE_HTTPD_BUFFER_USED, SXE_HTTPD_BUFFER_FREE);

SXE_EARLY_OUT:
    SXER6("return // free internal buffers %d", sxe_pool_get_number_in_state(httpd->buffers, SXE_HTTPD_BUFFER_FREE));
}

static void
sxe_httpd_event_send_done(SXE * this, SXE_RETURN final_result)
{
    SXE_HTTPD_REQUEST * request      = SXE_USER_DATA(this);
    SXE_HTTPD_REQUEST * request_pool = request->server->requests;
    unsigned            request_id   = SXE_HTTPD_REQUEST_INDEX(request_pool, request);
    unsigned            state        = sxe_pool_index_to_state(request_pool, request_id);

    SXEE6I("(request=%p, final_result=%s) // state=%s", request, sxe_return_to_string(final_result), sxe_httpd_request_state_to_string(state));

    if (state == SXE_HTTPD_REQUEST_STATE_FREE) {
        goto SXE_EARLY_OUT; /* Coverage exclusion: got a close while draining data! */
    }

    request->sendfile_fd = -1;

    if (request->on_sent_handler != NULL) {
        (*request->on_sent_handler)(request, final_result, request->on_sent_userdata);
    }

    sxe_httpd_clear_request(request);
    request->out_result = SXE_RETURN_OK;

    state = sxe_pool_index_to_state(request_pool, request_id);
    switch (state) {
    case SXE_HTTPD_REQUEST_STATE_FREE:
    case SXE_HTTPD_REQUEST_STATE_BAD:
        goto SXE_EARLY_OUT;                                               /* Coverage exclusion: TODO: have a test where on_complete() calls sxe_httpd_close() */
    case SXE_HTTPD_REQUEST_STATE_DEFERRED:
        request->substate = SXE_HTTPD_REQUEST_SUBSTATE_RESPONSE_DRAINED;  /* Coverage Exclusion: This seems broken, review this code */
        break;
    case SXE_HTTPD_REQUEST_STATE_ACTIVE:
        sxe_pool_set_indexed_element_state(request_pool, request_id, SXE_HTTPD_REQUEST_STATE_ACTIVE, SXE_HTTPD_REQUEST_STATE_IDLE);
        break;
    default:
        SXEA1(0, "Unexpected state %s in sxe_httpd_event_send_done()", sxe_httpd_request_state_to_string(state)); /* Coverage exclusion: unexpected */
    }

    sxe_buf_resume(this, SXE_BUF_RESUME_IMMEDIATE);

SXE_EARLY_OUT:
    SXER6I("return");
}

static void
sxe_httpd_event_buffer_sent(void * user_data, SXE_BUFFER * buffer)
{
    SXE_HTTPD_REQUEST * request      = (SXE_HTTPD_REQUEST *)user_data;
    SXE               * this         = request->sxe;
    SXE_HTTPD         * httpd        = request->server;

    SXEE6I("(request=%p, buffer=%p)", request, buffer);
    SXEA1I(request->out_buffer_count > 0, "%s: No outstanding buffers on request %p", __func__, request);

    sxe_httpd_give_buffer(httpd, buffer);

    if (--request->out_buffer_count > 0) {
        SXEL70("Still %d queued response buffers -- nothing else to do", request->out_buffer_count);
        goto SXE_EARLY_OUT;                                                                                                        /* COVERAGE EXCLUSION: TODO: Test case for multiple buffers queued to send (consistently) */
    }

#ifndef _WIN32
    if (request->sendfile_fd >= 0) {
        SXEL70("Starting sendfile after writing remaining buffers");
        sxe_sendfile(this, request->sendfile_fd, &request->sendfile_offset, request->sendfile_length, sxe_httpd_event_send_done);
        goto SXE_EARLY_OUT;
    }
#endif

    if (request->substate != SXE_HTTPD_REQUEST_SUBSTATE_RESPONSE_DRAINING) {
        SXEL70("All queued response buffers sent, but more output is coming (state != RESPONSE_DRAINING)");
        goto SXE_EARLY_OUT;
    }

    sxe_httpd_event_send_done(this, SXE_RETURN_OK);

SXE_EARLY_OUT:
    SXER6I("return");
}

static void
sxe_httpd_event_user_buffer_sent(void * user_data, SXE_BUFFER * buffer)
{
    SXE_HTTPD_REQUEST * request  = (SXE_HTTPD_REQUEST *)user_data;
    SXE               * this     = request->sxe;
    SXE_HTTPD_BUFFER  * user_buf = (SXE_HTTPD_BUFFER  *)buffer;

    SXE_UNUSED_PARAMETER(this);
    SXEE6I("(request=%p, buffer=%p)", request, buffer);

    // Call our callback first
    sxe_httpd_event_buffer_sent(user_data, buffer);

    // now call their callback
    if (user_buf->user_on_empty_cb != NULL) {
        user_buf->user_on_empty_cb(user_buf->user_data, buffer);
    }

    SXER6I("return");
}

static SXE_RETURN
sxe_httpd_take_buffer(SXE_HTTPD * httpd, SXE_HTTPD_REQUEST * request)
{
    SXE_RETURN result = SXE_RETURN_NO_UNUSED_ELEMENTS;
    unsigned   id;

    SXEE7("(httpd=%p,request=%p) // free buffers %d", httpd, request, sxe_pool_get_number_in_state(httpd->buffers, SXE_HTTPD_BUFFER_FREE));
    if (sxe_log_control.level == 6) do { SXEL6("%s(httpd=%p,request=%p){}", __func__, httpd, request); } while (0);

    request->out_buffer = NULL;
    id                  = sxe_pool_set_oldest_element_state(httpd->buffers, SXE_HTTPD_BUFFER_FREE, SXE_HTTPD_BUFFER_USED);

    if (id == SXE_POOL_NO_INDEX) {
        SXEL3("%s: Warning: no buffers available: increase buffers or lower concurrency", __func__);   /* Coverage exclusion: win32: test running out of send buffers */
        goto SXE_EARLY_OUT;                                                                            /* Coverage exclusion: win32: test running out of send buffers */
    }

    request->out_buffer_count++;
    request->out_buffer = SXE_HTTPD_INTERNAL_BUFFER(httpd, id);
    sxe_buffer_construct_plus(request->out_buffer, (char *)(request->out_buffer + 1), 0, httpd->buffer_size, request,
                              sxe_httpd_event_buffer_sent);
    result = SXE_RETURN_OK;

SXE_EARLY_OUT:
    SXER7("return result=%s // out_buffer=%p", sxe_return_to_string(result), request->out_buffer);
    return result;
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

    SXEE6("(method_string='%.*s')", method_length, method_string);

    for (i = 0; methods[i].name != NULL; i++) {
        if (method_length == methods[i].length && 0 == memcmp(method_string, methods[i].name, methods[i].length)) {
            method = methods[i].method;
            break;
        }
    }

    SXER6("return method=%u", method);
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

    SXEE6("(version_string='%.*s')", version_length, version_string);

    for (i = 0; versions[i].value != NULL; i++) {
        if (version_length == versions[i].length && 0 == memcmp(version_string, versions[i].value, versions[i].length)) {
            version = versions[i].version;
            break;
        }
    }

    SXER6("return version=%u", version);
    return version;
}

/* Note: This function should be called after the whole request line is received
 */
static SXE_RETURN
sxe_httpd_parse_request_line(SXE_HTTPD_REQUEST * request)
{
    SXE_HTTPD  * server = request->server;
    SXE        * this   = request->sxe;
    SXE_RETURN   result;
    const char * method_string;
    unsigned     method_length;
    const char * url_string;
    unsigned     url_length;
    const char * version_string;
    unsigned     version_length;

    SXEE6I("sxe_httpd_parse_request_line(request=%p)", request);

    /* Look for the method */
    result = sxe_http_message_parse_next_line_element(&request->message, SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN);

    if (result != SXE_RETURN_OK) {
        goto SXE_ERROR_OUT;
    }

    method_string    = sxe_http_message_get_line_element(       &request->message);
    method_length    = sxe_http_message_get_line_element_length(&request->message);
    request->method  = sxe_httpd_parse_method(method_string, method_length);

    if (request->method == SXE_HTTP_METHOD_INVALID) {
        SXEL2I("%s: Invalid method in HTTP request line: '%.*s'", __func__, method_length, method_string);
        result = SXE_RETURN_ERROR_BAD_MESSAGE;
        goto SXE_ERROR_OUT;
    }

    SXEL7I("Method: '%.*s' (%u)", method_length, method_string, request->method);

    /* Look for the Url */
    result = sxe_http_message_parse_next_line_element(&request->message, SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN);

    if (result != SXE_RETURN_OK) {
        goto SXE_ERROR_OUT;
    }

    url_string    = sxe_http_message_get_line_element(       &request->message);
    url_length    = sxe_http_message_get_line_element_length(&request->message);
    SXEL7I("Url: '%.*s'", url_length, url_string);

    /* Look for the HTTP version */
    result = sxe_http_message_parse_next_line_element(&request->message, SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN);

    if (result != SXE_RETURN_OK) {
        goto SXE_ERROR_OUT;
    }

    version_string    = sxe_http_message_get_line_element(       &request->message);
    version_length    = sxe_http_message_get_line_element_length(&request->message);
    request->version  = sxe_httpd_parse_version(version_string, version_length);

    if (request->version == SXE_HTTP_VERSION_UNKNOWN) {
        SXEL2I("%s: Invalid version in HTTP request line: '%.*s'", __func__, version_length, version_string);
        result = SXE_RETURN_ERROR_BAD_MESSAGE;
        goto SXE_ERROR_OUT;
    }

    SXEL7I("Version: '%.*s'", version_length, version_string);

    /* Look for the end of line */
    result = sxe_http_message_parse_next_line_element(&request->message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE);

    if (result != SXE_RETURN_OK && result != SXE_RETURN_END_OF_FILE) {
        goto SXE_ERROR_OUT;
    }
    else {
        result = SXE_RETURN_OK; /* EOF is okay here */
    }

    if (server->on_request != NULL) {
        (*server->on_request)(request, method_string, method_length, url_string, url_length, version_string, version_length);
    }
    else {
        SXEL6I("Request %p on HTTP server %p complete: No on_request handler set", request, server);
    }

SXE_ERROR_OUT:
    SXER6I("return result=%s", sxe_return_to_string(result));
    return result;
}

/**
 * Pause further events on an HTTPD request
 *
 * @request Pointer to the request object
 */
void
sxe_httpd_pause(SXE_HTTPD_REQUEST * request)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);

    SXEE6I("(request=%p)", request);
    SXEA6I(!request->paused, "HTTPD request is already paused");
    SXEL7I("Request %u (%p) is in state %s", SXE_CAST(unsigned, request - request->server->requests), request,
           sxe_httpd_request_get_state_and_substate_as_string(request));
    request->paused = true;
  //  sxe_pause(this);                    /* sxe_pause() is idempotent */
    SXER6I("return");
}

static void
sxe_httpd_event_read(SXE * this, int additional_length)
{
    SXE_HTTPD_REQUEST     * request;
    SXE_HTTPD_REQUEST_STATE state;
    SXE_HTTP_MESSAGE      * message;
    SXE_HTTPD             * server;
    SXE_HTTPD_REQUEST     * request_pool;
    char                  * start;
    char                  * end;
    unsigned                length;
    unsigned                consumed             = 0;
    unsigned                buffer_left          = 0;
    int                     response_status_code = 400;
    const char            * response_reason      = "Bad request";
    SXE_RETURN              result;
    const char            * key;
    unsigned                key_length;
    const char            * value;
    unsigned                value_length;
    unsigned                i;
    unsigned                request_id;

    SXEE6I("sxe_httpd_event_read(additional_length=%d)", additional_length);
    SXE_UNUSED_PARAMETER(additional_length);
    request      = SXE_USER_DATA(this);
    message      = &request->message;
    server       = request->server;
    request_pool = server->requests;
    request_id   = SXE_HTTPD_REQUEST_INDEX(request_pool, request);

    /* If the application requested a pause, do not process the read event, just let it queue up in the buffer.
     * sxe_httpd_pause() pauses the underlying SXE, but it can be unpaused automatically based on consuming bytes.
     * Unfortunately it's quite difficult to contrive a test case to cover this case, so it is excluded from coverage tests.
     */
    if (request->paused) {
        SXEL6I("Ignoring read event: request is paused");
        goto SXE_EARLY_OUT; /* Coverage exclusion: spurious - see block comment for details. */
    }

    state = sxe_pool_index_to_state(request_pool, request_id);
    SXEA6I(state != SXE_HTTPD_REQUEST_STATE_FREE, "Received data (i.e. a read event) on a free HTTPD request");

    /* In the bad state, don't actively close the connection, but 'sink' the data. Let the socket buffer pile up; once full no
     * more events are triggered, and eventually, the request will be reaped.
     */
    if (state == SXE_HTTPD_REQUEST_STATE_BAD) {
        SXEL6I("Ignoring %u bytes of data from a client in BAD state", SXE_BUF_USED(this));
        goto SXE_EARLY_OUT;
    }

    if (state == SXE_HTTPD_REQUEST_STATE_IDLE) {
        SXEL7I("HTTPD request state IDLE -> ACTIVE, substate RECEIVING_LINE");
        sxe_http_message_construct(message, SXE_BUF(this), SXE_BUF_USED(this));
        sxe_pool_set_indexed_element_state(request_pool, request_id, state, SXE_HTTPD_REQUEST_STATE_ACTIVE);
        request->substate = SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_LINE;
    }

    switch (request->substate) {
    case SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_LINE:
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

            /* Buffer is not full. Try again when there is more data */
            goto SXE_EARLY_OUT;    /* Coverage EXCLUSION - TODO: Cover case for partial request line sent */
        }

        if (*--end != '\r') {
            goto SXE_ERROR_OUT;
        }

        if ((result = sxe_httpd_parse_request_line(request)) != SXE_RETURN_OK) {
            goto SXE_ERROR_OUT;
        }

        SXEL7I("HTTPD request substate RECEIVING_LINE -> RECEIVING_HEADERS");
        request->substate = SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_HEADERS;

        /* fall through */

    case SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_HEADERS:
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
                    SXEL7I("%u bytes ignored , move the remaining %u bytes to the beginning of buffer", consumed, buffer_left);
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
                        SXEL3I("%s: Found a header which is too big(>=%u), ignore it", __func__, SXE_BUF_SIZE);
                        sxe_buf_clear(this);
                        sxe_http_message_set_ignore_line(message);
                        goto SXE_EARLY_OUT;
                    }

                    SXEL7I("About to consume the headers processed so far");
                    sxe_buf_consume(this, consumed);
                    goto SXE_EARLY_OUT; /* COVERAGE EXCLUSION - TODO: WIN32 COVERAGE */
                }

                SXEL7I("Haven't found the end of the headers yet; waiting for more data");
                goto SXE_EARLY_OUT; /* COVERAGE EXCLUSION - TODO: WIN32 COVERAGE */
            }

            SXEA1I(result == SXE_RETURN_OK, "Unexpected return from sxe_http_message_parse_next_header: %s",
                    sxe_return_to_string(result));

            key          = sxe_http_message_get_header_name(message);
            key_length   = sxe_http_message_get_header_name_length(message);
            value        = sxe_http_message_get_header_value(message);
            value_length = sxe_http_message_get_header_value_length(message);

            /* Is this the content-length header?
             */
            if (key_length == SXE_LITERAL_LENGTH(HTTPD_CONTENT_LENGTH)
             && strncasecmp(key, HTTPD_CONTENT_LENGTH, key_length) == 0)
            {
                if (value_length == 0) {
                    SXEL3I("%s: Bad request: Content-Length header field value is empty", __func__);
                    goto SXE_ERROR_OUT;
                }

                request->in_content_length = 0;

                for (i = 0; i < value_length; i++) {
                    if (!isdigit(value[i])) {
                        SXEL3I("%s: Bad request: Content-Length header field value contains a non-digit '%c'", __func__,
                                value[i]);
                        goto SXE_ERROR_OUT;
                    }

                    request->in_content_length = request->in_content_length * 10 + value[i] - '0';
                }

                SXEL7I("content-length: %u", request->in_content_length);
            }

            if (server->on_header != NULL) {
                SXEL7I("About to call on_header handler with key '%.*s' and value '%.*s'", key_length, key, value_length, value);
                (*server->on_header)(request, key, key_length, value, value_length);
            }
            else {
                SXEL7I("Received header with key '%.*s' and value '%.*s' on request %p, server %p: no on_header handler set",
                       key_length, key, value_length, value, request, server);
            }
        }

        consumed = sxe_http_message_consume_parsed_headers(message);
        sxe_buf_consume(this, consumed);    /* eat the terminator - om nom nom  */

        if (server->on_eoh != NULL) {
            (*server->on_eoh)(request);
        }
        else {
            SXEL7I("Received end of headers on request %p, server %p: no on_eoh handler set", request, server);
        }

        request->in_content_seen = 0;

        SXEL7I("HTTPD request substate RECEIVING_HEADERS -> RECEIVING_BODY");
        request->substate = SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_BODY;

        if (request->paused) {    /* sxe_httpd_pause() may have been called in on_eoh callback */
            goto SXE_EARLY_OUT;
        }

        /* fall through */

    case SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_BODY:
        SXEL7I("HTTPD request substate RECEIVING_BODY");
        length = SXE_BUF_USED(this);

        /* If there is a body and there is data in the buffer
         */
        if (request->in_content_length > 0 && length > 0) {
            const char * chunk  = SXE_BUF(this);

            if (length > request->in_content_length - request->in_content_seen) {
                length = request->in_content_length - request->in_content_seen; /* Coverage Exclusion - todo: win32 coverage */
            }

            SXEL7I("content length %u - body chunk %d: %.*s", request->in_content_length, length, length, chunk);
            consumed += length;
            sxe_buf_consume(this, length);
            request->in_content_seen += length;
            SXEA6I(length > 0, "Zero length body chunk");    /* Jim believes this is an invariant */

            if (WANT_BODY(request) && server->on_body != NULL) {
                SXEL7I("calling callback: ->on_body");
                (*server->on_body)(request, chunk, length);
            }
            else if (!WANT_BODY(request)) {
                SXEL7I("Body data received on request %p, server %p: ignoring body for request type %d", request, server, request->method);
            }
            else {
                SXEL7I("Body data received on request %p, server %p: No on_body handler set", request, server);
            }

            if (request->paused || sxe_pool_deferred(request_pool, request_id)) {    /* sxe_httpd_pause() may have been called in on_body handler */
                SXEL7I("request is %s; delaying state change to deferred event", request->paused ? "paused" : "deferred");
                goto SXE_EARLY_OUT;
            }
        }

        if (request->in_content_seen < request->in_content_length) {
            SXEL7I("Still receiving body");
            goto SXE_EARLY_OUT;
        }

        SXEL7I("HTTPD request state RECEIVING_BODY -> SENDING_LINE");
        request->substate = SXE_HTTPD_REQUEST_SUBSTATE_SENDING_LINE;
        SXEL7I("calling callback: ->on_respond");
        (*server->on_respond)(request);
        goto SXE_EARLY_OUT;

    /* In the response states, just let data pile up. This could happen due to pipelining.
     */
    default:
        SXEL5I("HTTPD received data in state: %s", sxe_httpd_request_get_state_and_substate_as_string(request));
        goto SXE_EARLY_OUT;
    }

SXE_ERROR_OUT:
    sxe_buf_clear(this);
    sxe_httpd_set_state_error(request);
    sxe_httpd_response_simple(request, NULL, NULL, response_status_code, response_reason, NULL,
                              HTTPD_CONNECTION_CLOSE_HEADER, HTTPD_CONNECTION_CLOSE_VALUE, NULL);
    (*server->on_close)(request); /* Effectively closed (from the calling application's perspective) */
    consumed = 0;

SXE_EARLY_OUT:
    /* sxe_consume() pauses; unpause unless sxe_httpd_pause() was called.
     */
    if (consumed > 0 && !request->paused) {
        sxe_buf_resume(this, SXE_BUF_RESUME_IMMEDIATE);
    }

    SXER6I("return");
}

static void
sxe_httpd_event_deferred_resume(void * array, unsigned i)
{
    SXE_HTTPD_REQUEST * request = &((SXE_HTTPD_REQUEST *)array)[i];
    SXE               * this    = request->sxe;

    SXEE6I("(array=%p, i=%u)", array, i);

    if (request->substate == SXE_HTTPD_REQUEST_SUBSTATE_RESPONSE_DRAINED) {
        sxe_pool_set_indexed_element_state(array, i, SXE_HTTPD_REQUEST_STATE_DEFERRED, SXE_HTTPD_REQUEST_STATE_IDLE); /* COVERAGE EXCLUSION - TODO: Cover case for resuming a drained session */
        request->substate = SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_LINE;                                                /* COVERAGE EXCLUSION - TODO: Cover case for resuming a drained session */
    }
    else {
        sxe_pool_set_indexed_element_state(array, i, SXE_HTTPD_REQUEST_STATE_DEFERRED, SXE_HTTPD_REQUEST_STATE_ACTIVE);
    }

    SXEL7I("Defered request set to state %s", sxe_httpd_request_get_state_and_substate_as_string(request));
    sxe_httpd_event_read(this, 0);
    SXER6I("return");
}

void
sxe_httpd_resume(SXE_HTTPD_REQUEST * request)
{
    SXE                   * this = request->sxe;
    SXE_HTTPD_REQUEST     * array;
    unsigned                i;
    SXE_HTTPD_REQUEST_STATE state;

    SXEE6I("(request=%p)", request);
    array = request->server->requests;
    SXEA6I(array <= request && request <= &array[request->server->max_connections - 1],
           "Request %p is outside of the valid range for its server (%p..%p)",
           request, array, &array[request->server->max_connections - 1]);
    state = sxe_pool_index_to_state(request->server->requests, request - request->server->requests);
    SXEA6I(state != SXE_HTTPD_REQUEST_STATE_FREE, "Attempt to resume free request %p", request);

    if (state == SXE_HTTPD_REQUEST_STATE_BAD) {
        SXEL6("Ignoring request to resume bad request %p", request);
        goto SXE_EARLY_OUT; /* COVERAGE EXCLUSION: TODO: NEED TO TEST */
    }

    sxe_buf_resume(this, SXE_BUF_RESUME_IMMEDIATE);
    i                    = request - array;
    request->paused      = false;
    SXEL7I("Request %u (@ %p) was in state %s; now DEFERRED", i, request, sxe_httpd_request_get_state_and_substate_as_string(request));
    sxe_pool_defer_event(array, i, &sxe_httpd_event_deferred_resume);

SXE_EARLY_OUT:
    SXER6I("return");
}

void
sxe_httpd_set_state_error(SXE_HTTPD_REQUEST * request)
{
    SXE_HTTPD_REQUEST * request_pool;
    unsigned            request_id;
    unsigned            old_state;

    SXEE6("(request=%p)", request);
    request_pool = request->server->requests;
    request_id   = SXE_HTTPD_REQUEST_INDEX(request_pool, request);
    old_state    = sxe_pool_index_to_state(request_pool, request_id);

    /* Set the state to BAD so from now on it will just early out (sink mode) */
    sxe_pool_set_indexed_element_state(    request_pool, request_id, old_state, SXE_HTTPD_REQUEST_STATE_BAD);

    /* Set the substate to SENDING_LINE so that it's possible to respond with
     * an error message. */
    request->substate = SXE_HTTPD_REQUEST_SUBSTATE_SENDING_LINE;

    SXER6("return");
}

/**
 * Start responding to an HTTP request
 *
 * @param request Pointer to the request
 * @param code    HTTP status code to respond with (e.g. 200)
 * @param status  Status string to respond with (e.g. "OK")
 *
 * @return SXE_RETURN_OK if sent or SXE_RETURN_NO_UNUSED_ELEMENTS if unable to allocate a buffer
 *
 * @exception Aborts if the request already has a partial response or the request line is too big to fit in a buffer
 */
SXE_RETURN
sxe_httpd_response_start(SXE_HTTPD_REQUEST * request, int code, const char * status)
{
    SXE_RETURN   result = SXE_RETURN_ERROR_INTERNAL;
    SXE        * this   = request->sxe;
    SXE_HTTPD  * httpd   = request->server;
    int          length;

    SXEE6I("(request=%p,code=%d,status=%s)", request, code, status);
    SXEA1I(request->substate == SXE_HTTPD_REQUEST_SUBSTATE_SENDING_LINE,
           "%s: Request %p is in substate %s (expected ACTIVE/SENDING_LINE)",
           __func__, request, sxe_httpd_request_get_state_and_substate_as_string(request));
    SXEA6I(request->out_buffer == NULL, "%s: Request %p already has a partial response", __func__, request);

    if ((result = sxe_httpd_take_buffer(httpd, request)) != SXE_RETURN_OK) {
        goto SXE_ERROR_OUT;
    }

    length = sxe_buffer_printf(request->out_buffer, "HTTP/1.1 %d %s\r\n", code, status);
    SXEA1I(!sxe_buffer_is_overflow(request->out_buffer), "Response line is longer (%u) than the buffer size (%u)", length,
           sxe_buffer_length(request->out_buffer));
    request->substate = SXE_HTTPD_REQUEST_SUBSTATE_SENDING_HEADERS;

SXE_ERROR_OUT:
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

/**
 * Send an HTTP header as part of an HTTP response
 *
 * @param request      Pointer to the request
 * @param header       Pointer to the header name ('\0' terminated)
 * @param value        Pointer to the header value
 * @param value_length Length of the value or 0 to treat value as a '\0' terminated string and take its length
 *
 * @return SXE_RETURN_OK if sent or SXE_RETURN_NO_UNUSED_ELEMENTS if unable to allocate a buffer, or a sxe_send_buffer() error
 *
 * @exception Aborts if the response has not already been started or if the end of headers has already been sent
 */
SXE_RETURN
sxe_httpd_response_header(SXE_HTTPD_REQUEST * request, const char * header, const char * value, unsigned value_length)
{
    SXE_RETURN   result        = SXE_RETURN_OK;
    SXE_HTTPD  * httpd         = request->server;
    SXE        * this          = request->sxe;
    unsigned     header_length = strlen(header);
    int          length;

    value_length = value_length != 0 ? value_length : strlen(value);
    SXEE6I("(request=%p,header=%s,value=%.*s)", request, header, value_length, value);
    SXEA1I(request->substate == SXE_HTTPD_REQUEST_SUBSTATE_SENDING_HEADERS,
           "%s: Request %p is in substate %s (expected ACTIVE/SENDING_HEADERS)",
           __func__, request, sxe_httpd_request_get_state_and_substate_as_string(request));
    SXEA1I(request->out_buffer != NULL, "%s: sending headers for request %p, but no output buffer", __func__, request);

    /* Try to write the header into the existing buffer; NOTE: need + 1: space for the '\0' from printf, even though its not sent
     */
    if (sxe_buffer_get_room(request->out_buffer) >= header_length + value_length + SXE_LITERAL_LENGTH(": \r\n") + 1) {
        length = sxe_buffer_printf(request->out_buffer, "%s: %.*s\r\n", header, value_length, value);
        SXEA6I(!sxe_buffer_is_overflow(request->out_buffer), "%s: Buffer overflowed even though there was room", __func__);
        SXEL6I("Wrote %u bytes to buffer %p: now %u bytes", length, request->out_buffer, sxe_buffer_length(request->out_buffer));
        goto SXE_EARLY_OUT;
    }

    SXEL7I("New header is %zu bytes, but current output buffer only has room for %u: sending it",
           header_length + value_length + SXE_LITERAL_LENGTH(": \r\n"), sxe_buffer_get_room(request->out_buffer));

    /* Send the previous (full) buffer
     */
    if ((result = sxe_send_buffer(this, request->out_buffer)) != SXE_RETURN_OK && result != SXE_RETURN_IN_PROGRESS) {
        goto SXE_ERROR_OUT; /* COVERAGE EXCLUSION - TODO: test send failure */
    }

    if ((result = sxe_httpd_take_buffer(httpd, request)) != SXE_RETURN_OK) {
        goto SXE_ERROR_OUT;                         /* COVERAGE EXCLUSION - TODO: test buffer exhaustion */
    }

    length = sxe_buffer_printf(request->out_buffer, "%s: %.*s\r\n", header, value_length, value);
    SXEA1I(!sxe_buffer_is_overflow(request->out_buffer), "Header is longer (%u) than the buffer size (%u)", length, httpd->buffer_size);

SXE_EARLY_OR_ERROR_OUT:
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

/**
 * Send a content length header as part of an HTTP response
 *
 * @param request Pointer to the request
 * @param length  Content length in bytes
 *
 * @return SXE_RETURN_OK if sent or SXE_RETURN_NO_UNUSED_ELEMENTS if unable to allocate a buffer
 *
 * @exception Aborts if the request already has a partial response queued
 */
SXE_RETURN
sxe_httpd_response_content_length(SXE_HTTPD_REQUEST * request, unsigned length)
{
    SXE      * this = request->sxe;
    char       ibuf[32];
    SXE_RETURN result;

    SXE_UNUSED_PARAMETER(this);
    SXEE7I("(request=%p,length=%d)", request, length);
    snprintf(ibuf, sizeof(ibuf), "%u", length);
    result = sxe_httpd_response_header(request, HTTPD_CONTENT_LENGTH, ibuf, 0);
    SXER7I("return result=%s", sxe_return_to_string(result));
    return result;
}

static SXE_RETURN
sxe_httpd_response_eoh(SXE_HTTPD_REQUEST * request)
{
    SXE_RETURN   result = SXE_RETURN_ERROR_INTERNAL;
    SXE        * this = request->sxe;
    SXE_HTTPD  * httpd = request->server;

    SXE_UNUSED_PARAMETER(this);
    SXEE6I("(request=%p)", request);
    SXEA1I(request->substate == SXE_HTTPD_REQUEST_SUBSTATE_SENDING_HEADERS,
           "%s: Request %p is in substate %s (expected ACTIVE/SENDING_HEADERS)",
           __func__, request, sxe_httpd_request_get_state_and_substate_as_string(request));

    if (sxe_buffer_get_room(request->out_buffer) >= SXE_LITERAL_LENGTH("\r\n")) {
        sxe_buffer_cat(request->out_buffer, SXE_BUFFER_CAST("\r\n"));
        SXEL7I("sxe_httpd_response_eoh(): wrote EOH to buffer %p: now %u bytes", request->out_buffer,
               sxe_buffer_length(request->out_buffer));
        goto SXE_EARLY_OUT;
    }

    /* Send the previous (full) buffer
     */
    if ((result = sxe_send_buffer(this, request->out_buffer)) != SXE_RETURN_OK && result != SXE_RETURN_IN_PROGRESS) {
        goto SXE_ERROR_OUT;                                              /* COVERAGE EXCLUSION: TODO: Cover send failure */
    }

    if (sxe_httpd_take_buffer(httpd, request) != SXE_RETURN_OK) {
        result = SXE_RETURN_NO_UNUSED_ELEMENTS;                          /* COVERAGE EXCLUSION: TODO: Cover buffer exhaustion */
        goto SXE_ERROR_OUT;                                              /* COVERAGE EXCLUSION: TODO: Cover buffer exhaustion */
    }

    sxe_buffer_cat(request->out_buffer, SXE_BUFFER_CAST("\r\n"));

SXE_EARLY_OUT:
    result            = SXE_RETURN_OK;
    request->substate = SXE_HTTPD_REQUEST_SUBSTATE_SENDING_BODY;

SXE_ERROR_OUT:
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

static SXE_RETURN
sxe_httpd_response_copy_data_and_send(SXE_HTTPD_REQUEST * request, const char * chunk, unsigned length)
{
    SXE_RETURN  result = SXE_RETURN_ERROR_INTERNAL;
    SXE       * this   = request->sxe;
    SXE_HTTPD * httpd   = request->server;
    unsigned    amount_to_copy;
    SXE_BUFFER  buffer;

    SXE_UNUSED_PARAMETER(this);

    if (length == 0) {
        length = strlen(chunk);
    }

    SXEE6I("(chunk=%p,length=%u)", chunk, length);
    SXEA1I(chunk != NULL, "%s: chunk pointer is NULL", __func__);

    for (;;) {
        if ((amount_to_copy = sxe_buffer_get_room(request->out_buffer)) > 0) {
            amount_to_copy = amount_to_copy > length ? length : amount_to_copy;
            sxe_buffer_construct_const(&buffer, chunk, amount_to_copy);
            sxe_buffer_cat(request->out_buffer, &buffer);
            chunk  += amount_to_copy;
            length -= amount_to_copy;
        }

        if (length == 0) {
            break;
        }

        if ((result = sxe_send_buffer(this, request->out_buffer)) != SXE_RETURN_OK && result != SXE_RETURN_IN_PROGRESS) {
            goto SXE_ERROR_OUT;                                 /* Coverage exclusion: todo: make sxe_send_buffer() return an error */
        }

        if (sxe_httpd_take_buffer(httpd, request) != SXE_RETURN_OK) {
            result = SXE_RETURN_NO_UNUSED_ELEMENTS;
            goto SXE_ERROR_OUT;
        }
    }

    result = SXE_RETURN_OK;

SXE_ERROR_OUT:
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

/**
 * Copy a chunk of body to an HTTP request's response; If the EOH has not been added, it is added first.
 *
 * @param request Pointer to an HTTP request object
 * @param chunk   Pointer to the raw chunk of data
 * @param length  Length of the chunk or 0 if the chunk is a '\0' terminated string
 *
 * @return SXE return code; SXE_RETURN_NO_UNUSED_ELEMENTS if there are not enough free buffers available to copy the entire chunk.
 *
 * @note Data is copied into the response, so the caller's data buffe can be reused immediately. To avoid copying data, use
 *       sxe_httpd_response_add_body_buffer().
 *
 * @note To avoid adding EOH (i.e. if you are writing raw HTTP headers and body), use sxe_httpd_response_copy_raw_data()
 */
SXE_RETURN
sxe_httpd_response_copy_body_data(SXE_HTTPD_REQUEST * request, const char * chunk, unsigned length)
{
    SXE_RETURN   result = SXE_RETURN_NO_UNUSED_ELEMENTS;
    SXE        * this = request->sxe;

    SXE_UNUSED_PARAMETER(this);
    SXEE6I("(request=%p, chunk=%p, length=%u)", request, chunk, length);
    SXEA6I(request->out_buffer != NULL, "%s() called before sxe_httpd_response_start()", __func__);

    if (request->substate != SXE_HTTPD_REQUEST_SUBSTATE_SENDING_BODY
     && (result = sxe_httpd_response_eoh(request)) != SXE_RETURN_OK)
    {
        goto SXE_EARLY_OUT;                                     /* Coverage exclusion: todo: test running out of send buffers */
    }

    result = sxe_httpd_response_copy_data_and_send(request, chunk, length);

SXE_EARLY_OUT:
    // sxe_httpd_give_buffers(httpd, &buffer_list);    /* Return any new buffers to the pool if anything went wrong. */
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

/**
 * Copy a raw chunk (line, header and/or body) to the response
 *
 * @param request Pointer to an HTTP request object
 * @param chunk   Pointer to the raw chunk of data
 * @param length  Length of the chunk
 *
 * @return SXE return code; can be SXE_RETURN_NO_UNUSED_ELEMENTS if there are not enough free buffers available to copy the
 *         entire data.
 *
 * @note Data is copied into the response, so the caller's data buffer can be reused immediately. To avoid copying data, use
 *       sxe_httpd_response_add_raw_buffer().
 *
 * @note Does not add an EOH automatically
 */
SXE_RETURN
sxe_httpd_response_copy_raw_data(SXE_HTTPD_REQUEST * request, const char * chunk, unsigned length)
{
    SXE_RETURN   result = SXE_RETURN_NO_UNUSED_ELEMENTS;
    SXE        * this = request->sxe;
    SXE_HTTPD  * httpd = request->server;

    SXE_UNUSED_PARAMETER(this);

    SXEE6I("(request=%p, chunk=%p, length=%u)", request, chunk, length);

    /* If this is the beginning of the response, grab a buffer.
     */
    if (request->substate == SXE_HTTPD_REQUEST_SUBSTATE_SENDING_LINE) {
        SXEA6I(request->out_buffer == NULL, "Request %p: line has not been sent, but a buffer has been queued", request);

        if ((result = sxe_httpd_take_buffer(httpd, request)) != SXE_RETURN_OK) {
            goto SXE_ERROR_OUT; /* Coverage exclusion: todo: test running out of send buffers */
        }
    }

    result            = sxe_httpd_response_copy_data_and_send(request, chunk, length);
    request->substate = SXE_HTTPD_REQUEST_SUBSTATE_SENDING_BODY;

SXE_ERROR_OUT:
    //sxe_httpd_give_buffers(httpd, &buffer_list);
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

/**
 * Add a body buffer to the response
 *
 * @param request Pointer to an HTTP request object
 * @param buffer  Pointer to the SXE_BUFFER to append
 *
 * @return SXE_RETURN_OK or SXE_RETURN_NO_UNUSED_ELEMENTS if a buffer could not be allocated for the EOH marker
 *
 * @note The buffer is appended to the response; the data in the buffer must not be modified until the data has been copied to
 *       the TCP output window. A callback can be passed to sxe_buffer_construct_plus() to notify the application that the
 *       buffer has been emptied.
 */
SXE_RETURN
sxe_httpd_response_add_body_buffer(SXE_HTTPD_REQUEST * request, SXE_HTTPD_BUFFER * buffer)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    SXE      * this   = request->sxe;

    SXE_UNUSED_PARAMETER(this);
    SXEE6I("(request=%p, buffer=%p)", request, buffer);
    SXEA6I(request->out_buffer != NULL, "%s() called before sxe_httpd_response_start()", __func__);
    ((SXE_BUFFER *)buffer)->user_data = request;
    request->out_buffer_count++;

    if (request->substate != SXE_HTTPD_REQUEST_SUBSTATE_SENDING_BODY
     && (result = sxe_httpd_response_eoh(request)) != SXE_RETURN_OK)
    {
        goto SXE_ERROR_OUT; /* Coverage exclusion: todo: test running out of send buffers */
    }

    if ((result = sxe_send_buffer(this, request->out_buffer)) != SXE_RETURN_OK && result != SXE_RETURN_IN_PROGRESS) {
        goto SXE_ERROR_OUT; /* Coverage exclusion: todo: test send failure */
    }

    result              = SXE_RETURN_OK;
    request->out_buffer = (SXE_BUFFER *)buffer;

SXE_ERROR_OUT:
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

/**
 * Add a raw buffer to the response buffer queue.
 *
 * @param request Pointer to an HTTP request object
 * @param buffer  Pointer to the SXE_BUFFER to append
 *
 * @return SXE_RETURN_OK or an error from sxe_send_buffer()
 *
 * @note The buffer is appended to the response queue, so its contents must not be modified until it has been sent. When the
 *       buffer is copied to the socket send window, its on_empty call back will be called.
 */
SXE_RETURN
sxe_httpd_response_add_raw_buffer(SXE_HTTPD_REQUEST * request, SXE_HTTPD_BUFFER * buffer)
{
    SXE_RETURN   result = SXE_RETURN_ERROR_INTERNAL;
    SXE        * this = request->sxe;

    SXE_UNUSED_PARAMETER(this);

    SXEE6I("(request=%p, buffer=%p)", request, buffer);
    ((SXE_BUFFER *)buffer)->user_data = request;
    request->out_buffer_count++;

    // If this is the beginning of the response, assume that the caller will send the whole response.
    if (request->substate == SXE_HTTPD_REQUEST_SUBSTATE_SENDING_LINE) {
        SXEA6I(request->out_buffer == NULL, "Request %p: line has not been sent, but a buffer has been queued", request);
    }

    if (request->out_buffer != NULL) {
        if ((result = sxe_send_buffer(this, request->out_buffer)) != SXE_RETURN_OK && result != SXE_RETURN_IN_PROGRESS) {
            goto SXE_ERROR_OUT; /* Coverage exclusion: TODO test this at all */
        }
    }

    result              = SXE_RETURN_OK;
    request->out_buffer = (SXE_BUFFER *)buffer;
    request->substate   = SXE_HTTPD_REQUEST_SUBSTATE_SENDING_BODY;

SXE_ERROR_OUT:
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

#ifndef _WIN32
/**
 * Send (part of) a file as part of an HTTP response body
 *
 * @param request   Pointer to an HTTP request object
 * @param fd        File descriptor of the file to send data from
 * @param length    Amount of data to send
 * @param on_sent   Function to when the file has been sent
 * @param user_data Caller provided data to be passed to the on_sent function
 *
 * @return SXE_RETURN_OK
 *
 * @note All data waiting to be sent will be sent before the file is sent. Do not send more data until the file is sent.
 *
 * @exception Aborts if the response line has not already been sent
 */
SXE_RETURN
sxe_httpd_response_sendfile(SXE_HTTPD_REQUEST * request, int fd, unsigned length, sxe_httpd_on_sent_handler on_sent, void * user_data)
{
    SXE_RETURN   result = SXE_RETURN_NO_UNUSED_ELEMENTS;
    SXE        * this   = request->sxe;

    SXE_UNUSED_PARAMETER(this);
    SXEE6I("(request=%p,fd=%d,length=%u,on_sent=%p,user_data=%p)", request, fd, length, on_sent, user_data);
    SXEA6I(request->out_buffer != NULL, "%s() called before sxe_httpd_response_start()", __func__);

    request->on_sent_handler  = on_sent;
    request->on_sent_userdata = user_data;
    request->sendfile_fd      = fd;
    request->sendfile_length  = length;
    request->sendfile_offset  = lseek(fd, 0, SEEK_CUR);

    if (request->substate != SXE_HTTPD_REQUEST_SUBSTATE_SENDING_BODY
        && (result = sxe_httpd_response_eoh(request)) != SXE_RETURN_OK
        && result != SXE_RETURN_IN_PROGRESS)
    {
        goto SXE_EARLY_OUT;                                                   /* Coverage exclusion: todo: test running out of send buffers */
    }

    request->substate = SXE_HTTPD_REQUEST_SUBSTATE_RESPONSE_DRAINING;
    result = sxe_send_buffer(request->sxe, request->out_buffer);

SXE_EARLY_OUT:
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}
#endif

/**
 * Finish sending an HTTP response, calling back when the entire response has been copied to the TCP send window
 *
 * @param request     Request being responded to
 * @param on_complete Function to call back when the response has been completely sent or NULL to fire and forget
 * @param user_data   User data passed back to on_complete
 *
 * @return SXE_RETURN_OK if the message was sent or an error from sxe_send_buffer()
 *
 * @note If the caller used add_raw_buffer or add_body_buffer, the on_complete callback may be called before the caller's
 *       buffers are copied to TCP. Use the buffer's on_empty callback (see sxe_buffer_construct_plus()) to determine when
 *       caller's buffers are sent.
 */

SXE_RETURN
sxe_httpd_response_end(SXE_HTTPD_REQUEST * request, sxe_httpd_on_sent_handler on_complete, void * user_data)
{
    SXE_RETURN          result = SXE_RETURN_NO_UNUSED_ELEMENTS;
    SXE               * this   = request->sxe;
    SXE_HTTPD_REQUEST * request_pool;
    unsigned            request_id;

    SXEE6I("(request=%p, on_complete=%p, user_data=%p)", request, on_complete, user_data);
    request_pool              = request->server->requests;
    request_id                = SXE_HTTPD_REQUEST_INDEX(request_pool, request);
    request->on_sent_handler  = on_complete;
    request->on_sent_userdata = user_data;

    if (request->substate != SXE_HTTPD_REQUEST_SUBSTATE_SENDING_BODY
     && (result = sxe_httpd_response_eoh(request)) != SXE_RETURN_OK)
    {
        goto SXE_EARLY_OUT;                                     /* Coverage exclusion: todo: test running out of send buffers */
    }

    request->substate   = SXE_HTTPD_REQUEST_SUBSTATE_RESPONSE_DRAINING;
    request->out_result = sxe_send_buffer(this, request->out_buffer);
    request->out_buffer = NULL;

    if (request->out_result == SXE_RETURN_IN_PROGRESS) {
        goto SXE_EARLY_OUT;
    }

SXE_EARLY_OUT:
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

/**
 * Listen for connections to an HTTPD server
 *
 * @param httpd    HTTPD server
 * @param address IP address to listen on
 * @param port    TCP port to listen on
 *
 * @return pointer to SXE or NULL on failure to allocate or listen on SXE
 */
SXE *
sxe_httpd_listen(SXE_HTTPD * httpd, const char * address, unsigned short port)
{
    SXE * this;

    SXEE6("(address=%s, port=%hu)", address, port);

    if ((this = sxe_new_tcp(NULL, address, port, sxe_httpd_event_connect, sxe_httpd_event_read, sxe_httpd_event_close)) == NULL) {
        SXEL2("sxe_httpd_listen: Failed to allocate a SXE for TCP");                           /* Coverage Exclusion: No need to test */
        goto SXE_ERROR_OUT;                                                                    /* Coverage Exclusion: No need to test */
    }

    SXE_USER_DATA(this) = httpd;

    if (sxe_listen(this) != SXE_RETURN_OK) {
        SXEL2("sxe_httpd_listen: Failed to listen on address %s, port %hu", address, port);    /* Coverage Exclusion: No need to test */
        sxe_close(this);                                                                       /* Coverage Exclusion: No need to test */
        this = NULL;                                                                           /* Coverage Exclusion: No need to test */
    }

SXE_ERROR_OUT:
    SXER6("return %p", this);
    return this;
}

#ifndef _WIN32
/**
 * Listen for pipe (AKA unix domain socket) connections to an HTTPD server
 *
 * @param httpd Pointer to the HTTPD server
 * @param path Path name of the pipe ('\0' terminated)
 *
 * @return pointer to SXE or NULL on failure to allocate or listen on SXE
 */
SXE *
sxe_httpd_listen_pipe(SXE_HTTPD * httpd, const char * path)
{
    SXE * this;

    SXEE6("(path=%s)", path);

    if ((this = sxe_new_pipe(NULL, path, sxe_httpd_event_connect, sxe_httpd_event_read, sxe_httpd_event_close)) == NULL) {
        SXEL2("sxe_httpd_listen_pipe: Failed to allocate a SXE for pipes");    /* Coverage Exclusion: No need to test */
        goto SXE_ERROR_OUT;                                                    /* Coverage Exclusion: No need to test */
    }

    SXE_USER_DATA(this) = httpd;

    if (sxe_listen(this) != SXE_RETURN_OK) {
        SXEL2("sxe_httpd_listen_pipe: Failed to listen on path %s", path);     /* Coverage Exclusion: No need to test */
        sxe_close(this);                                                       /* Coverage Exclusion: No need to test */
        this = NULL;                                                           /* Coverage Exclusion: No need to test */
    }

SXE_ERROR_OUT:
    SXER6("return %p", this);
    return this;
}
#endif

unsigned
sxe_httpd_diag_get_free_connections(SXE_HTTPD * httpd)
{
    return sxe_pool_get_number_in_state(httpd->requests, SXE_HTTPD_REQUEST_STATE_FREE);
}

unsigned
sxe_httpd_diag_get_free_buffers(SXE_HTTPD * httpd)
{
    return sxe_pool_get_number_in_state(httpd->buffers, SXE_HTTPD_BUFFER_FREE);
}

void
sxe_httpd_buffer_construct(SXE_HTTPD_BUFFER * httpd_buf, char * memory, unsigned length, unsigned size,
                           void * user_data, void (* user_on_empty_cb)(void * user_data, struct SXE_HTTPD_BUFFER * buffer))
{
    sxe_buffer_construct_plus(&httpd_buf->buffer, memory, length, size, NULL, sxe_httpd_event_user_buffer_sent);
    httpd_buf->user_data        = user_data;
    httpd_buf->user_on_empty_cb = (void(*)(void *, struct SXE_BUFFER *))user_on_empty_cb;
}

void
sxe_httpd_buffer_construct_const(SXE_HTTPD_BUFFER * httpd_buf, const char * memory, unsigned length,
                                 void * user_data, void (* user_on_empty_cb)(void * user_data, struct SXE_HTTPD_BUFFER * buffer))
{
    sxe_buffer_construct_const(&httpd_buf->buffer, memory, length);
    httpd_buf->buffer.on_empty  = sxe_httpd_event_user_buffer_sent;
    httpd_buf->user_data        = user_data;
    httpd_buf->user_on_empty_cb = (void(*)(void *, struct SXE_BUFFER *))user_on_empty_cb;
}

unsigned
sxe_httpd_buffer_length(const SXE_HTTPD_BUFFER * buf)
{
    return sxe_buffer_length(&buf->buffer);
}

/**
 * Construct an HTTPD server
 *
 * @param httpd         Pointer to an HTTPD server object
 * @param connections  Number of connections to support; must be >= 2     TODO: Add 1 for free connection
 * @param buffer_count Number of buffers;                must be >= 10
 * @param buffer_size  Size of buffers;                  must be >= 512
 * @param options      Currently unused;                 must be 0
 *
 * @exception Aborts if input preconditions are violated
 */
void
sxe_httpd_construct(SXE_HTTPD * httpd, unsigned connections, unsigned buffer_count, unsigned buffer_size, unsigned options)
{
    SXEE6("(httpd=%p, connections=%u, buffer_count=%u, buffer_size=%u, options=%x)", httpd, connections, buffer_count, buffer_size, options);
    SXEA1(connections  >=   2, "Requires at least 2 connections");
    SXEA1(buffer_count >=  10, "Requires minimum 10 buffers");
    SXEA1(buffer_size  >= 512, "Requires minimum buffer_size of 512");
    SXEA1(options      ==   0, "Options must be 0");

    httpd->ident[0] = 'S';
    httpd->ident[1] = 'R';
    httpd->ident[2] = 'V';
    httpd->ident[3] = 'R';

    httpd->max_connections = connections;
    httpd->requests        = sxe_pool_new("httpd", connections, sizeof(SXE_HTTPD_REQUEST),
                                         SXE_HTTPD_REQUEST_STATE_NUMBER_OF_STATES, SXE_POOL_OPTION_UNLOCKED | SXE_POOL_OPTION_TIMED);
    sxe_pool_set_state_to_string(httpd->requests, sxe_httpd_request_state_to_string);
    sxe_pool_defer_allow(httpd->requests, &httpd->defer, sxe_private_main_loop, SXE_HTTPD_REQUEST_STATE_DEFERRED);

    httpd->buffer_count    = buffer_count;
    httpd->buffer_size     = buffer_size;
    httpd->buffers         = sxe_pool_new("httpd-buffers", buffer_count, SXE_HTTPD_INTERNAL_BUFFER_SIZE(httpd),
                                        SXE_HTTPD_BUFFER_NUMBER_OF_STATES, SXE_POOL_OPTION_UNLOCKED);
    httpd->on_connect      = NULL;
    httpd->on_request      = NULL;
    httpd->on_header       = NULL;
    httpd->on_eoh          = NULL;
    httpd->on_body         = NULL;
    httpd->on_respond      = sxe_httpd_default_respond_handler;
    httpd->on_close        = sxe_httpd_default_close_handler;

    SXER6("return");
}

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
