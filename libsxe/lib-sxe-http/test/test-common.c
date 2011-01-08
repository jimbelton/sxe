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

#include "sxe-http.h"
#include "sxe-util.h"
#include "tap.h"

int
main(void)
{
    SXE_HTTP_URL url;

    plan_tests(27);

    /* Basic URL
     */
    is(sxe_http_url_parse(&url, "http://www.google.com", 0, 0),    SXE_RETURN_OK,        "Parsed 'http://www.google.com'");
    is(url.scheme_length,                                          4,                    "Scheme is 4 bytes");
    is_strncmp(url.scheme,                                         "http",           4,  "Scheme is http");
    is(url.host_length,                                            14,                   "Host is 13 bytes");
    is_strncmp(url.host,                                           "www.google.com", 14, "Host is www.google.com");
    is(url.port_length,                                            0,                    "Port length is default (0)");
    is(url.port,                                                   0,                    "Port is default (0)");
    is(url.path_length,                                            0,                    "Path length is default (0)");
    is(url.path,                                                   NULL,                 "Path is default (NULL)");

    /* With port and path
     */
    is(sxe_http_url_parse(&url, "https://sophos.com:8080/path", 0, 0), SXE_RETURN_OK,    "Parsed 'https://sophos.com:8080/path'");
    is(url.scheme_length,                                              5,                "Scheme is 4 bytes");
    is_strncmp(url.scheme,                                             "https",      5,  "Scheme is https");
    is(url.host_length,                                                10,               "Host is 13 bytes");
    is_strncmp(url.host,                                               "sophos.com", 10, "Host is sophos.com");
    is(url.port_length,                                                5,                "':8080' is 5 bytes");
    is(url.port,                                                       8080,             "Port is 8080");
    is(url.path_length,                                                0,                "Path length is 0 (-> '\\0' terminated");
    is_eq(url.path,                                                    "path",           "Path is 'path'");

    /* With port but no path, and using a length, not a '\0' terminated string
     */
    is(sxe_http_url_parse(&url, "ftp://sex.com:6969XXX", SXE_LITERAL_LENGTH("ftp://sex.com:6969"), 0), SXE_RETURN_OK,
       "Parsed 'ftp://sex.com:6969XXX' (XXX is trailing garbage)");
    is(url.port,                                                       6969,             "Port is 6969");
    is(url.path,                                                       NULL,             "Path is NULL");

    /* Failure cases
     */
    is(sxe_http_url_parse(&url, "http//missing.colon",     0, 0), SXE_RETURN_ERROR_INVALID_URI, "Scheme must end with a ':'");
    is(sxe_http_url_parse(&url, "://leading.colon",        0, 0), SXE_RETURN_ERROR_INVALID_URI, "URL can't start with a ':'");
    is(sxe_http_url_parse(&url, "http:/missing.slash",     0, 0), SXE_RETURN_ERROR_INVALID_URI, "Schemme must be followed by '//'");
    is(sxe_http_url_parse(&url, "http://",                 0, 0), SXE_RETURN_ERROR_INVALID_URI, "URL must have a hostname");
    is(sxe_http_url_parse(&url, "http://foo.com:/no/port", 0, 0), SXE_RETURN_ERROR_INVALID_URI, "Colon must be followed by a port");
    is(sxe_http_url_parse(&url, "http://127.0.0.1:2bad",   0, 0), SXE_RETURN_ERROR_INVALID_URI, "Port must be decimal followed by '/'");

    return exit_status();
}

