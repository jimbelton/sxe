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

#ifndef __SXE_HTTPD_H__
#define __SXE_HTTPD_H__

#include "sxe.h"
#include "sxe-http.h"
#include "sxe-list.h"
#include "sxe-pool.h"
#include "sxe-util.h"

#define HTTPD_CONTENT_LENGTH          "Content-Length"
#define HTTPD_CONNECTION_CLOSE_HEADER "Connection"
#define HTTPD_CONNECTION_CLOSE_VALUE  "close"

/* Pool states of an HTTP request object */
typedef enum {
    SXE_HTTPD_REQUEST_STATE_FREE = 0,              /* not connected                                                           */
    SXE_HTTPD_REQUEST_STATE_BAD,                   /* invalid HTTP, sink mode until closed or reclaimed                       */
    SXE_HTTPD_REQUEST_STATE_IDLE,                  /* client connected, but connection is idle                                */
    SXE_HTTPD_REQUEST_STATE_ACTIVE,                /* an HTTP request is in progress (see substates, below)                   */
    SXE_HTTPD_REQUEST_STATE_DEFERRED,              /* request has a deferred event on it                                      */
    SXE_HTTPD_REQUEST_STATE_NUMBER_OF_STATES
} SXE_HTTPD_REQUEST_STATE;

/* Substates of an active HTTP request object */
typedef enum {
    SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_LINE,     /* receiving VERB <SP> URL [<SP> VERSION]                                  */
    SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_HEADERS,  /* reading headers                                                         */
    SXE_HTTPD_REQUEST_SUBSTATE_RECEIVING_BODY,     /* at BODY we run the handler for each chunk                               */
    SXE_HTTPD_REQUEST_SUBSTATE_SENDING_LINE,       /* Sending the response line (read disabled)                               */
    SXE_HTTPD_REQUEST_SUBSTATE_SENDING_HEADERS,    /* Sending the response headers (read disabled)                            */
    SXE_HTTPD_REQUEST_SUBSTATE_SENDING_BODY,       /* Sent EOH, sending the body (read disabled)                              */
    SXE_HTTPD_REQUEST_SUBSTATE_RESPONSE_DRAINING,  /* Queued the end of the response, waiting for it to drain (read disabled) */
    SXE_HTTPD_REQUEST_SUBSTATE_RESPONSE_DRAINED,   /* Response drained, deferred resume will reenable read                    */
    SXE_HTTPD_REQUEST_SUBSTATE_NUMBER_OF_SUBSTATES
} SXE_HTTPD_REQUEST_SUBSTATE;

#define WANT_BODY(request) ((request)->method == SXE_HTTP_METHOD_PUT || (request)->method == SXE_HTTP_METHOD_POST)

struct SXE_HTTPD;
struct SXE_HTTPD_REQUEST;

typedef void (*sxe_httpd_connect_handler)(struct SXE_HTTPD_REQUEST *);
typedef void (*sxe_httpd_request_handler)(struct SXE_HTTPD_REQUEST *, const char *method, unsigned mlen, const char *url,
                                          unsigned ulen, const char *version, unsigned vlen);
typedef void (*sxe_httpd_header_handler)( struct SXE_HTTPD_REQUEST *, const char *key, unsigned klen, const char *val, unsigned vlen);
typedef void (*sxe_httpd_eoh_handler)(    struct SXE_HTTPD_REQUEST *);
typedef void (*sxe_httpd_body_handler)(   struct SXE_HTTPD_REQUEST *, const char *chunk, unsigned chunklen);
typedef void (*sxe_httpd_respond_handler)(struct SXE_HTTPD_REQUEST *);
typedef void (*sxe_httpd_close_handler)(  struct SXE_HTTPD_REQUEST *);

/* This handler is invoked when any of the sxe_httpd_response_*() functions
 * have sent the entire response or portion of the response; or when an I/O
 * error occurs. */
typedef void (*sxe_httpd_on_sent_handler)(struct SXE_HTTPD_REQUEST *, SXE_RETURN, void *);

#define SXE_HTTPD_REQUEST_USER_DATA(request)              ((request)->user_data)
#define SXE_HTTPD_REQUEST_USER_DATA_AS_UNSIGNED(request)  SXE_CAST(uintptr_t, (request)->user_data)
#define SXE_HTTPD_REQUEST_SERVER_USER_DATA(request)       ((request)->server->user_data)
#define SXE_HTTPD_REQUEST_GET_CONTENT_LENGTH(request)     (WANT_BODY(request) ? (request)->in_content_length : 0)

/* HTTP request */
typedef struct SXE_HTTPD_REQUEST {
    char                      ident[4];
    struct SXE_HTTPD         * server;
    SXE                      * sxe;                  /* NOT FOR APPLICATION USE!                              */
    void                     * user_data;            /* not used by sxe_httpd -- to be used by applications   */
    SXE_HTTPD_REQUEST_SUBSTATE substate;             /* Substate of an active request/response (see above)    */

    /* Parsed info from the request line and headers */
    SXE_HTTP_METHOD            method;
    SXE_HTTP_VERSION           version;
    SXE_HTTP_MESSAGE           message;
    unsigned                   in_content_length;    /* value from Content-Length header                      */
    unsigned                   in_content_seen;      /* number of body bytes read                             */
    bool                       paused;               /* are input events paused?                              */

    /* Output handling */
    SXE_BUFFER *               out_buffer;           /* pointer to the current output buffer (sent when full) */
    unsigned                   out_buffer_count;     /* number of buffers allocated by HTTPD to this request  */
    SXE_RETURN                 out_result;

    /* Handle sxe_send() and sxe_sendfile() */
    sxe_httpd_on_sent_handler  on_sent_handler;
    void *                     on_sent_userdata;
    int                        sendfile_fd;
    unsigned                   sendfile_length;
    off_t                      sendfile_offset;
} SXE_HTTPD_REQUEST;

static inline SXE * sxe_httpd_request_get_sxe(SXE_HTTPD_REQUEST * request) { return request->sxe; };       /* For diagnostics */

/* HTTP server */
typedef struct SXE_HTTPD {
    char                      ident[4];
    SXE_HTTPD_REQUEST       * requests;
    SXE_POOL_DEFER            defer;
    unsigned                  max_connections;
    SXE_BUFFER              * buffers;
    unsigned                  buffer_size;
    unsigned                  buffer_count;
    void                    * user_data;
    sxe_httpd_connect_handler on_connect;
    sxe_httpd_request_handler on_request;
    sxe_httpd_header_handler  on_header;
    sxe_httpd_eoh_handler     on_eoh;
    sxe_httpd_body_handler    on_body;
    sxe_httpd_respond_handler on_respond;
    sxe_httpd_close_handler   on_close;
} SXE_HTTPD;

/* EXTERNAL HTTPD BUFFER */
typedef struct SXE_HTTPD_BUFFER {
    SXE_BUFFER                buffer;
    void                    * user_data;
    void                   (* user_on_empty_cb)(void * user_data, struct SXE_BUFFER * buffer);
} SXE_HTTPD_BUFFER;

#define SXE_HTTPD_SET_HANDLER(httpd, handler, function) sxe_httpd_set_ ## handler ## _handler((httpd), function)

#include "sxe-httpd-proto.h"

#endif

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
