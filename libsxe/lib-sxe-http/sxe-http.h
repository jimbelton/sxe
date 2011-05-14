/* Copyright 2011 Sophos Limited. All rights reserved. Sophos is a registered
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

#ifndef __SXE_HTTP_H__
#define __SXE_HTTP_H__

#include <string.h>
#include <sys/types.h>

#include "sxe-log.h"
#include "sxe-time.h"
#include "sxe-util.h"

#define SXE_HTTP_VERB_LENGTH_MAXIMUM 8

#define SXE_HTTP_DIGEST_LENGTH  32
#define SXE_HTTP_DIGEST_BUFSIZE (SXE_HTTP_DIGEST_LENGTH + 1)

typedef enum {
    SXE_HTTP_METHOD_INVALID=0,
    SXE_HTTP_METHOD_GET,
    SXE_HTTP_METHOD_HEAD,
    SXE_HTTP_METHOD_PUT,
    SXE_HTTP_METHOD_POST,
    SXE_HTTP_METHOD_DELETE
} SXE_HTTP_METHOD;

typedef enum {
    SXE_HTTP_VERSION_UNKNOWN=0,
    SXE_HTTP_VERSION_1_0,
    SXE_HTTP_VERSION_1_1
} SXE_HTTP_VERSION;

typedef enum SXE_HTTP_LINE_ELEMENT_TYPE {
    SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN,
    SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE
} SXE_HTTP_LINE_ELEMENT_TYPE;

typedef struct SXE_HTTP_URL {
    const char * scheme;
    unsigned     scheme_length;
    const char * host;
    unsigned     host_length;
    unsigned     port;
    unsigned     port_length;      /* Including ':', to allow host_length + port_length to be the length of a hash key */
    const char * path;
    unsigned     path_length;
} SXE_HTTP_URL;

typedef struct SXE_HTTP_MESSAGE {
    const char * buffer;
    unsigned     buffer_length;
    unsigned     consumed;
    unsigned     element_length;
    unsigned     name_length;
    unsigned     value_offset;
    unsigned     value_length;
    unsigned     next_field;
    unsigned     ignore_line;
    unsigned     ignore_length;
} SXE_HTTP_MESSAGE;

typedef struct SXE_HTTP_NONCE {
    SXE_TIME time;
    uint64_t sequence_number;
} SXE_HTTP_NONCE;

static inline const char *
sxe_http_message_get_line_element(SXE_HTTP_MESSAGE * message)
{
    SXEA11(message->element_length > 0, "%s: called when no line element parsed", __func__);
    return &message->buffer[message->consumed];
}

static inline unsigned
sxe_http_message_get_line_element_length(SXE_HTTP_MESSAGE * message)
{
    return message->element_length;
}

static inline unsigned
sxe_http_message_get_request_line_length(SXE_HTTP_MESSAGE * message)
{
    return message->element_length + message->consumed;
}

static inline const char *
sxe_http_message_get_header_name(SXE_HTTP_MESSAGE * message)
{
    SXEA11(message->name_length > 0, "%s: called when no header parsed", __func__);
    return &message->buffer[message->consumed];
}

static inline unsigned
sxe_http_message_get_header_name_length(SXE_HTTP_MESSAGE * message)
{
    return message->name_length;
}

static inline const char *
sxe_http_message_get_header_value(SXE_HTTP_MESSAGE * message)
{
    return &message->buffer[message->value_offset];
}

static inline unsigned
sxe_http_message_get_header_value_length(SXE_HTTP_MESSAGE * message) {
    return message->value_length;
}

static inline unsigned
sxe_http_message_get_ignore_length(SXE_HTTP_MESSAGE * message) {
    return message->ignore_length;
}

static inline unsigned
sxe_http_message_get_buffer_length(SXE_HTTP_MESSAGE * message) {
    return message->buffer_length;
}

static inline void
sxe_http_message_set_ignore_line(SXE_HTTP_MESSAGE * message) {
    message->buffer_length = 0;
    message->ignore_line   = 1;
}

static inline void
sxe_http_message_buffer_shift_ignore_length(SXE_HTTP_MESSAGE * message) {
    memmove(SXE_CAST_NOCONST(char *, message->buffer), message->buffer + message->ignore_length, message->buffer_length);
}

/**
 * Convert a nonce in a 32 character hex string
 *
 * @param nonce        Pointer to a nonce
 * @param nonce_in_hex Pointer to a buffer at least 2 * sizeof(SXE_HTTP_NONCE) characters in size.
 *
 * @return nonce_in_hex
 */
static inline char *
sxe_http_nonce_to_hex(SXE_HTTP_NONCE * nonce, char * nonce_in_hex)
{
    return sxe_hex_from_bytes(nonce_in_hex, (unsigned char *)nonce, sizeof(*nonce));
}

#include "lib-sxe-http-proto.h"

#endif
