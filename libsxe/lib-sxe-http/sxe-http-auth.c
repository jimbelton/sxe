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
#include <string.h>
#include <unistd.h>

#include "md5.h"
#include "sxe-http.h"
#include "sxe-log.h"
#include "sxe-util.h"

static SXE_HTTP_NONCE sxe_http_nonce = {0, 0};    /* The next SXE_HTTP_NONCE value to return, initialized on the first call */

/**
 * Generate a 128 bit nonce value
 *
 * @return A unique number, even across restarts of the process
 *
 * @note The number is seeded with the time, an is incremented on each call
 * @note This function is NOT THREAD SAFE
 */
void
sxe_http_nonce_next(SXE_HTTP_NONCE * nonce)
{
    if (sxe_http_nonce.time == 0 && sxe_http_nonce.sequence_number == 0) {
        sxe_http_nonce.time = sxe_time_get();
    }

    *nonce = sxe_http_nonce;
    sxe_http_nonce.sequence_number++;

    if (sxe_http_nonce.sequence_number == 0) {
        sxe_http_nonce.time++;                                /* COVERAGE EXCLUSION: Requires getting next nonce 2^64 times */
    }
}

/**
 * Calculate the hash of the 'A1' value (HA1) for HTTP digest authentication.
 *
 * Uses the username, realm, and password to calculate the HA1 value. This
 * typically should only be used by an HTTP client -- on a server, it's better
 * to store this value in the credentials database, indexed by the username.
 * That way not even the hashed password is stored on the server.
 *
 * @param username
 * @param username_length
 * @param realm
 * @param realm_length
 * @param password
 * @param password_length
 * @param HA1
 *
 * @note HA1 = MD5("{username}:{realm}:{password}")
 *
 * @note HA1 must have SXE_HTTP_DIGEST_BUFSIZE bytes available.
 *
 * @see
 *  - http://en.wikipedia.org/wiki/Digest_access_authentication
 *  - http://tools.ietf.org/html/rfc2617
 */
void
sxe_http_auth_calculate_ha1(const char * username, unsigned username_length,
                            const char * realm,    unsigned realm_length,
                            const char * password, unsigned password_length,
                            char       * HA1)
{
    MD5_CTX    ctx;
    SOPHOS_MD5 md5;

    MD5_Init(&ctx);
    MD5_Update(&ctx, username, username_length);
    MD5_Update(&ctx, ":", 1);
    MD5_Update(&ctx, realm, realm_length);
    MD5_Update(&ctx, ":", 1);
    MD5_Update(&ctx, password, password_length);
    MD5Result(&ctx, &md5);

    md5_to_hex(&md5, HA1, MD5_IN_HEX_LENGTH + 1);
}

/**
 * Calculate the hash of the 'A2' value (HA2) for HTTP digest authentication.
 *
 * @param method        HTTP method (SXE_HTTP_METHOD_GET, etc)
 * @param url           URL being requested
 * @param url_length    Length of URL
 * @param HA2           [out] Buffer in which to store calculated HA2 value.
 *                      Must have at least (SXE_HTTP_DIGEST_LENGTH + 1) bytes.
 *
 * @note HA2 = MD5("{method}:{url}")
 *
 * @note HA2 must have SXE_HTTP_DIGEST_BUFSIZE bytes available.
 */
void
sxe_http_auth_calculate_ha2(const char * method, unsigned method_length, const char * url, unsigned url_length, char * HA2)
{
    MD5_CTX ctx;
    SOPHOS_MD5 md5;

    MD5_Init(&ctx);
    MD5_Update(&ctx, method, method_length);
    MD5_Update(&ctx, ":", 1);
    MD5_Update(&ctx, url, url_length);
    MD5Result(&ctx, &md5);

    md5_to_hex(&md5, HA2, MD5_IN_HEX_LENGTH + 1);
}

/**
 * Calculate the appropriate 'response' field for HTTP digest authentication.
 *
 * See also:
 *  - http://en.wikipedia.org/wiki/Digest_access_authentication
 *  - http://tools.ietf.org/html/rfc2617
 *
 * @param buffer        Value of 'Authorization' header
 * @param buffer_length Length of the header
 * @param HA1           Precalculated hash of the A1 value (see RFC2617 for
 *                      the definition of A1)
 * @param HA1           Precalculated hash of the A2 value (see RFC2617 for
 *                      the definition of A2)
 *
 * Uses the 'qop', 'nc', 'nonce', 'cnonce', HA1 and HA2 values to calculate
 * the appropriate 'response' field. Returns true if the 'response' field
 * in the in the header matches; otherwise false.
 *
 * @note response = MD5("{HA1}:{nonce}:{nc}:{cnonce}:auth:{HA2}")
 */
void
sxe_http_auth_calculate_response(const char * HA1, const char * HA2,       const char * nonce,  unsigned nonce_length,
                                 const char * nc,  unsigned     nc_length, const char * cnonce, unsigned cnonce_length,
                                 char * response)
{
    MD5_CTX    ctx;
    SOPHOS_MD5 md5;

    SXEE60("(...)");

    MD5_Init(&ctx);
    SXEL93("%s(): ha1='%.*s'", __func__, MD5_IN_HEX_LENGTH, HA1);
    MD5_Update(&ctx, HA1, MD5_IN_HEX_LENGTH);
    MD5_Update(&ctx, ":", 1);
    SXEL93("%s(): nonce='%.*s'", __func__, nonce_length, nonce);
    MD5_Update(&ctx, nonce, nonce_length);
    MD5_Update(&ctx, ":", 1);
    SXEL93("%s(): nc='%.*s'", __func__, nc_length, nc);
    MD5_Update(&ctx, nc, nc_length);
    MD5_Update(&ctx, ":", 1);
    SXEL93("%s(): cnonce='%.*s'", __func__, cnonce_length, cnonce);
    MD5_Update(&ctx, cnonce, cnonce_length);
    MD5_Update(&ctx, ":", 1);
    MD5_Update(&ctx, "auth", strlen("auth")); /* qop=auth */
    MD5_Update(&ctx, ":", 1);
    SXEL93("%s(): ha2='%.*s'", __func__, MD5_IN_HEX_LENGTH, HA2);
    MD5_Update(&ctx, HA2, MD5_IN_HEX_LENGTH);
    MD5Result(&ctx, &md5);

    md5_to_hex(&md5, response, MD5_IN_HEX_LENGTH + 1);
    SXER62("return // response='%.*s'", MD5_IN_HEX_LENGTH, response);
}

/**
 * Extract a field from a 'WWW-Authenticate' or 'Authorization' header. The
 * format of these headers are as follows:
 *
 *    WWW-Authenticate: {type} realm="{realm}" qop=auth nonce="{nonce}"
 *    Authorization:    {type} realm="{realm}" qop=auth nonce="{nonce}" cnonce="{cnonce}" nc="{nc}" username="{username}" response="{response}"
 *
 * @param name          NUL-terminated field to search for
 * @param buffer        Value of 'WWW-Authenticate' or 'Authorization' header
 * @param buffer_length Length of the header
 * @param field_length  [out] Length of the returned field
 *
 * @return a pointer to the field value or NULL if not found; the field_length is the length of the value.
 *
 * @example
 *
 * Given this header:
 *
 *    WWW-Authenticate: Digest realm="hello" qop=auth nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093"
 *
 * And this code:
 *
 *    nonce = sxe_http_auth_get_field("nonce", header, header_len, &nonce_len);
 *
 * Then 'nonce' will point at 'dcd98b7102dd2f0e8b11d0f600bfb0c093', and nonce_len will be set to 34.
 *
 * @see
 *  - http://en.wikipedia.org/wiki/Digest_access_authentication
 *  - http://tools.ietf.org/html/rfc2617
 */
const char *
sxe_http_auth_get_field(const char * buffer, unsigned buffer_length, const char * name, unsigned *field_length)
{
    unsigned     name_len = (unsigned)strlen(name);
    const char * end      = buffer + buffer_length;
    const char * vstart   = NULL;
    const char * vend     = NULL;

    SXEE84("(name='%s', buffer=%p, buffer_length=%u, field_length=%p)", name, buffer, buffer_length, field_length);

    while ((vstart = sxe_strnstr(buffer, name, buffer_length)) != NULL) {
        vstart += name_len;

        if (vstart[0] != '=') {
            buffer         = &vstart[1];
            buffer_length -= (&vstart[1] - buffer);
            continue;
        }

        vstart++; // skip '='

        if (vstart[0] == '"') {
            vstart++;
        }

        for (vend = vstart; vend < end && strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz9876543210-_", vend[0]) != NULL; vend++) {
        }

        if (vend == end) {
            vstart = NULL;
        }

        break;
    }

    *field_length = vend - vstart;
    SXER83("return %p // %.*s", vstart, *field_length, vstart);
    return vstart;
}

/* vim: set expandtab: */
