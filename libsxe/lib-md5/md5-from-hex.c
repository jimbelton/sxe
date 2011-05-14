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

#include "md5.h"
#include "sxe-util.h"

#ifdef WHEN_I_NEED_IT
SXE_RETURN
md5_from_hex(SOPHOS_MD5 * md5, const char * md5_in_hex)
{
    SXE_RETURN result;

    SXEE82("(md5=%p,md5_in_hex='%s'", md5, md5_in_hex);
    result = sxe_hex_to_bytes((unsigned char *)md5, md5_in_hex, MD5_IN_HEX_LENGTH);
    SXER81("return %s", sxe_return_to_string(result));
    return result;
}
#endif

SXE_RETURN
md5_to_hex(SOPHOS_MD5 * md5, char * md5_in_hex, unsigned md5_in_hex_length)
{
    SXE_RETURN result = SXE_RETURN_OK;

    SXEE86("(md5=%08x%08x%08x%08x,md5_in_hex='%p',md5_in_hex_length='%u'",
           md5->word[3], md5->word[2], md5->word[1], md5->word[0],
           md5_in_hex, md5_in_hex_length);
    SXEA11(md5_in_hex_length == (MD5_IN_HEX_LENGTH + 1), "Incorrect length of char * for md5_to_hex(): '%u'", md5_in_hex_length);

    snprintf(md5_in_hex     , 9, "%08x", htonl(md5->word[0]));
    snprintf(md5_in_hex +  8, 9, "%08x", htonl(md5->word[1]));
    snprintf(md5_in_hex + 16, 9, "%08x", htonl(md5->word[2]));
    snprintf(md5_in_hex + 24, 9, "%08x", htonl(md5->word[3]));
    SXEL62("md5_in_hex: '%.*s'", MD5_IN_HEX_LENGTH, md5_in_hex);

    SXER81("return %s", sxe_return_to_string(result));
    return result;
}

