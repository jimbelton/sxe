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
#include "sxe-list.h"

/* How much header space do we allocate per connection?
 */
#define SXE_HTTPD_MAX_HEADER_BLOCK      16384
#define SXE_HTTPD_HEADERS_PER_REQUEST       4
#define SXE_HTTPD_HEADERS_MIN              64

typedef enum {
    SXE_HTTPD_CONN_FREE = 0,        /* not connected */
    SXE_HTTPD_CONN_IDLE,            /* client connected */
    SXE_HTTPD_CONN_REQ_LINE,        /* VERB <SP> URL [<SP> VERSION] */
    SXE_HTTPD_CONN_REQ_HEADERS,     /* reading headers */
    SXE_HTTPD_CONN_REQ_BODY,        /* at BODY we run the handler for each chunk. */
    SXE_HTTPD_CONN_REQ_RESPONSE,    /* generating the response (read disabled) */
    SXE_HTTPD_CONN_NUMBER_OF_STATES
} SXE_HTTPD_CONN_STATE;

typedef enum {
    SXE_HTTP_METHOD_INVALID=0,
    SXE_HTTP_METHOD_GET,
    SXE_HTTP_METHOD_HEAD,
    SXE_HTTP_METHOD_PUT,
    SXE_HTTP_METHOD_POST,
    SXE_HTTP_METHOD_DELETE
} SXE_HTTP_METHOD;

#define WANT_BODY(request) (request->method == SXE_HTTP_METHOD_PUT || request->method == SXE_HTTP_METHOD_POST)

typedef struct {
    SXE_LIST_NODE node;
    int      is_response;
    unsigned key_off;
    unsigned key_len;
    unsigned val_off;
    unsigned val_len;
} SXE_HTTP_HEADER;

#define SXE_HTTP_HEADER_KEY(request, hdr) (((hdr)->is_response ? (request)->out_buf : (request)->in_buf) + (hdr)->key_off)
#define SXE_HTTP_HEADER_KEY_LEN(hdr) (hdr)->key_len
#define SXE_HTTP_HEADER_VAL(request, hdr) (((hdr)->is_response ? (request)->out_buf : (request)->in_buf) + (hdr)->val_off)
#define SXE_HTTP_HEADER_VAL_LEN(hdr) (hdr)->val_len
#define SXE_HTTP_HEADER_EQUALS(request, hdr, test) (0 == strncasecmp(SXE_HTTP_HEADER_KEY(request, hdr), (test), SXE_HTTP_HEADER_KEY_LEN(hdr)))

struct SXE_HTTPD;
struct SXE_HTTP_REQUEST;

/* The handler is called in three scenarios:
 *
 *  1. When a client connects: state will be SXE_HTTPD_CONN_IDLE.
 *  2. When a client's request headers have been parsed: state will be
 *     SXE_HTTPD_CONN_REQ_EOH.
 *  3. For each chunk of the request body. The last chunk is signified by
 *     state of SXE_HTTPD_CONN_REQ_BODY and chunk_len of zero.
 */
typedef void (*sxe_httpd_handler)(struct SXE_HTTP_REQUEST *, SXE_HTTPD_CONN_STATE state, char *chunk, size_t chunk_len, void *);
typedef void (*sxe_httpd_sendfile_handler)(struct SXE_HTTP_REQUEST *, SXE_RETURN, void *);

typedef struct SXE_HTTP_REQUEST {
    struct SXE_HTTPD * server;
    SXE * sxe;

    /* The request line and headers */
    char                    in_buf[SXE_HTTPD_MAX_HEADER_BLOCK];
    unsigned                in_len;
    unsigned                method_len;
    unsigned                url_off;            /* offset into in_buf[] */
    unsigned                url_len;
    unsigned                version_off;        /* offset into in_buf[] */
    unsigned                version_len;
    unsigned                headers_off;        /* offset into in_buf[] */
    unsigned                headers_len;

    /* Parsed info from the request line and headers */
    SXE_LIST                in_headers;
    SXE_HTTP_METHOD         method;
    unsigned                in_content_length;     /* value from Content-Length header */
    unsigned                in_content_seen;       /* number of body bytes read */

    /* The response headers (NOT the body) */
    char                    out_buf[SXE_HTTPD_MAX_HEADER_BLOCK];
    unsigned                out_len;
    SXE_LIST                out_headers;
    unsigned                out_written;    /* did we sxe_write() out_buf[] yet? */

    /* Handle sendfile */
    sxe_httpd_sendfile_handler sendfile_handler;
    void *                     sendfile_userdata;
} SXE_HTTP_REQUEST;

#define SXE_HTTP_REQUEST_URL(request)     ((request)->in_buf + (request)->url_off)
#define SXE_HTTP_REQUEST_URL_LEN(request) ((request)->url_len)

typedef struct SXE_HTTPD {
    SXE_HTTP_HEADER *       h_array;
    SXE_LIST                h_freelist;
    SXE_HTTP_REQUEST *      requests;
    void *                  user_data;
    sxe_httpd_handler       handler;
} SXE_HTTPD;

#include "sxe-httpd-proto.h"

#endif

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
