/* Copyright (c) 2010 Sophos Group.
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

#include "sxe-socket.h"

#include "sha1.h"
#include "sxe-util.h"

SXE_RETURN
sha1_from_hex(SOPHOS_SHA1 * sha1, const char * sha1_in_hex)
{
    SXE_RETURN result;

    SXEE82("sxe_sha1_from_hex(sha1=%p,sha1_in_hex='%s'", sha1, sha1_in_hex);
    result = sxe_hex_to_bytes((unsigned char *)sha1, sha1_in_hex, SHA1_IN_HEX_LENGTH);
    SXER81("return %s", sxe_return_to_string(result));
    return result;
}

SXE_RETURN
sha1_to_hex(SOPHOS_SHA1 * sha1, char * sha1_in_hex, unsigned sha1_in_hex_length)
{
    SXE_RETURN result = SXE_RETURN_OK;

    SXEE87("sxe_sha1_to_hex(sha1=%08x%08x%08x%08x%08x,sha1_in_hex='%p',sha1_in_hex_length='%u'",
           sha1->word[4], sha1->word[3], sha1->word[2], sha1->word[1], sha1->word[0],
           sha1_in_hex, sha1_in_hex_length);
    SXEA11(sha1_in_hex_length == (SHA1_IN_HEX_LENGTH + 1), "Incorrect length of char * for sha1_to_hex(): '%u'", sha1_in_hex_length);

    snprintf(sha1_in_hex     , 9, "%08x", htonl(sha1->word[0]));
    snprintf(sha1_in_hex +  8, 9, "%08x", htonl(sha1->word[1]));
    snprintf(sha1_in_hex + 16, 9, "%08x", htonl(sha1->word[2]));
    snprintf(sha1_in_hex + 24, 9, "%08x", htonl(sha1->word[3]));
    snprintf(sha1_in_hex + 32, 9, "%08x", htonl(sha1->word[4]));
    SXEL62("sha1_in_hex: '%.*s'", 40, sha1_in_hex);

    SXER81("return %s", sxe_return_to_string(result));
    return result;
}

