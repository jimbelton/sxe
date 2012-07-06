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

#include "md5.h"
#include "sxe-http.h"
#include "sxe-util.h"
#include "tap.h"

#define STRLEN(x) (sizeof(x) - 1)

char www_auth[] = "Digest realm=LiveConnect, qop=auth, nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\"";
char auth[]     = "Digest realm=LiveConnect,\r\n"
                  "       qop=auth,\r\n"
                  "       username=\"8d04cf7a135f3fd70fc21afe7a6513fc30bde3b7\",\r\n"
                  "       nc=00000001,\r\n"
                  "       nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\"\r\n"
                  "       cnonce=\"0a4f113b\",\r\n"
                  "       response=\"6630fae49393a05397450978507c4ef1\"\r\n"
                  ;

#define IS_FIELD(h,n,v) do { \
    value = sxe_http_auth_get_field(h, STRLEN(h), n, &length); \
    is(length, SXE_LITERAL_LENGTH(v), "Header contains " n " of length %ld", (unsigned long)SXE_LITERAL_LENGTH(v)); \
    is_strncmp(v, value, length, "Header contains " n "=%.*s", length, value); \
} while (0)

int
main(void)
{
    const char   * value;
    unsigned       length;
    SXE_HTTP_NONCE unused;

    plan_tests(28);

    /* sxe_http_auth_get_field() */
    {
        /* WWW-Authenticate */
        is(sxe_http_auth_get_field(www_auth, STRLEN(www_auth), "field", &length), NULL, "Header does not contain 'field'");
        IS_FIELD(www_auth, "realm", "LiveConnect");
        IS_FIELD(www_auth, "qop",   "auth");
        IS_FIELD(www_auth, "nonce", "dcd98b7102dd2f0e8b11d0f600bfb0c093");

        /* Authorization */
        IS_FIELD(auth, "realm",    "LiveConnect");
        IS_FIELD(auth, "qop",      "auth");
        IS_FIELD(auth, "nonce",    "dcd98b7102dd2f0e8b11d0f600bfb0c093");
        IS_FIELD(auth, "username", "8d04cf7a135f3fd70fc21afe7a6513fc30bde3b7");
        IS_FIELD(auth, "nc",       "00000001");
        IS_FIELD(auth, "cnonce",   "0a4f113b");
        IS_FIELD(auth, "response", "6630fae49393a05397450978507c4ef1");

        /* Edge cases */
        is(sxe_http_auth_get_field("Fudgsicle", STRLEN("Fudgsicle"), "field", &length), NULL, "Header does not contain 'field'");
        is(sxe_http_auth_get_field("field=",    STRLEN("field="),    "field", &length), NULL, "Header does not contain 'field'");
        IS_FIELD("Hello fake=\"field\" field=\"hello\"", "field", "hello");
    }

    /* sxe_http_auth_calculate_ha1() */
    {
        char ha1[MD5_IN_HEX_LENGTH + 1];
        const char username[] = "8d04cf7a135f3fd70fc21afe7a6513fc30bde3b7";
        const char password[] = "2243a6149cf338200a31fa9a8c5fa960a4b0a323";

        sxe_http_auth_calculate_ha1(username, STRLEN(username),
                                    "LiveConnect", SXE_LITERAL_LENGTH("LiveConnect"),
                                    password, STRLEN(password),
                                    ha1);

        is_eq(ha1, "64c01794167c5998430c2be08953e7cc", "HA1 is calculated correctly");
    }

    /* sxe_http_auth_calculate_ha2() */
    {
        char ha2[MD5_IN_HEX_LENGTH + 1];
        const char url[] = "/some/url";

        sxe_http_auth_calculate_ha2("GET", 3, url, STRLEN(url), ha2);

        is_eq(ha2, "230a3bb96966703857d40a40a6c09b03", "HA2 is calculated correctly");
    }

    /* sxe_http_auth_calculate_response() */
    {
        char response[MD5_IN_HEX_LENGTH + 1];
        const char ha1[] = "e16e314b9b3135ebb59d5629730c55b0";
        const char ha2[] = "230a3bb96966703857d40a40a6c09b03";
        const char nonce[] = "dcd98b7102dd2f0e8b11d0f600bfb0c093";
        const char cnonce[] = "0a4f113b";
        const char nc[] = "00000001";

        sxe_http_auth_calculate_response(ha1, ha2, nonce, STRLEN(nonce), nc, STRLEN(nc), cnonce, STRLEN(cnonce), response);

        is_eq(response, "a3647628f8db48fb0f24c451d2ae06d5", "response is calculated correctly");
    }

    sxe_http_nonce_next(&unused);    /* For coverage */
    return exit_status();
}

/* vim: set expandtab: */
