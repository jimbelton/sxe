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

#include "sxe-log.h"
#include "sxe-http.h"

SXE_RETURN
sxe_http_url_parse(SXE_HTTP_URL * url_out, const char * string, unsigned string_length, unsigned options)
{
    unsigned     string_maximum_length = string_length == 0 ? ~0U : string_length;
    SXE_RETURN   result                = SXE_RETURN_ERROR_INVALID_URI;
    const char * reason;
    unsigned     idx;
    unsigned     start;

    SXEE85("sxe_http_url_parse(url_out=%p, string='%.*s', string_length=%u, options=0x%x)",
           url_out, string_maximum_length, string, string_length, options);
    SXE_UNUSED_ARGUMENT(options);
    memset(url_out, 0, sizeof(*url_out));

    /* Parse the scheme
     */
    for (idx = 0; true; idx++) {
        if ((idx >= string_maximum_length) || (string[idx] == '\0')) {
            reason = "scheme is not terminated with a colon";
            goto SXE_ERROR_OUT;
        }

        if (string[idx] == ':') {
            if (idx == 0) {
                reason = "starts with a colon";
                goto SXE_ERROR_OUT;
            }

            url_out->scheme        = string;
            url_out->scheme_length = idx;
            idx++;
            break;
        }
    }

    if ((idx + 1 >= string_maximum_length) || (string[idx] != '/') || (string[idx + 1] != '/')) {
        reason = "scheme is not followed by '//'";
        goto SXE_ERROR_OUT;
    }

    idx += 2;

    /* TODO: If needed, add support for username:password@ in URLs */

    /* Parse the hostname
     */
    for (start = idx; (idx < string_maximum_length) && (string[idx] != '\0') && (string[idx] != ':') && (string[idx] != '/'); idx++) {
    }

    if (idx == start) {
        reason = "has an empty host name";
        goto SXE_ERROR_OUT;
    }

    url_out->host        = &string[start];
    url_out->host_length = idx - start;

    if ((idx >= string_maximum_length) || (string[idx] == '\0')) {
        result = SXE_RETURN_OK;
        goto SXE_EARLY_OUT;
    }

    idx++;

    /* Parse the port number, if any
     */
    if (string[idx - 1] == ':') {
        if ((idx >= string_maximum_length) || (string[idx] < '0') || (string[idx] > '9')) {
            reason = "':' is not followed by a decimal port number";
            goto SXE_ERROR_OUT;
        }

        for (start = idx; (idx < string_maximum_length) && (string[idx] >= '0') && (string[idx] <= '9'); idx++) {
            url_out->port = url_out->port * 10 + string[idx] - '0';
        }

        url_out->port_length = idx - start + 1;

        if ((idx >= string_maximum_length) || (string[idx] == '\0')) {
            result = SXE_RETURN_OK;
            goto SXE_EARLY_OUT;
        }

        if (string[idx] != '/') {
            reason = "port number is not followed by a '/'";
            goto SXE_ERROR_OUT;
        }

        idx++;
    }

    /* TODO: Parse the path for a search part, if needed */

    url_out->path        = &string[idx];
    url_out->path_length = string_length == 0 ? 0 : string_length - idx;
    result               = SXE_RETURN_OK;
    goto SXE_EARLY_OUT;

SXE_ERROR_OUT:
    SXEL33("sxe_http_url_parse: warning: URL %s: '%.*s'", reason, string_maximum_length, string);

SXE_EARLY_OUT:
    SXER81("return result=%s", sxe_return_to_string(result));
    return result;
}
