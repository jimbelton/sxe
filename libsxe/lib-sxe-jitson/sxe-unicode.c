#include "sxe-log.h"
#include "sxe-unicode.h"

/* From https://www.ietf.org/rfc/rfc3629.txt
 *
 * Char. number range  |        UTF-8 octet sequence
 *    (hexadecimal)    |              (binary)
 * --------------------+---------------------------------------------
 * 0000 0000-0000 007F | 0xxxxxxx
 * 0000 0080-0000 07FF | 110xxxxx 10xxxxxx
 * 0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
 * 0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 */

unsigned
sxe_unicode_to_utf8(unsigned unicode, char *utf_out)
{
    if (unicode <= 0x7F) {
        utf_out[0] = (char)unicode;
        return 1;
    }

    if (unicode <= 0x7FF) {
        utf_out[0] = (char)(0xC0 | (unicode >> 6));
        utf_out[1] = (char)(0x80 | (unicode & 0x3F));
        return 2;
    }

    if (unicode <= 0xFFFF) {
        utf_out[0] = (char)(0xE0 | (unicode >> 12));
        utf_out[1] = (char)(0x80 | ((unicode >> 6) & 0x3F));
        utf_out[2] = (char)(0x80 | (unicode & 0x3F));
        return 3;
    }

    if (unicode <= 0x10FFFF) {
        utf_out[0] = (char)(0xF0 | (unicode >> 18));
        utf_out[1] = (char)(0x80 | ((unicode >> 12) & 0x3F));
        utf_out[2] = (char)(0x80 | ((unicode >> 6) & 0x3F));
        utf_out[3] = (char)(0x80 | (unicode & 0x3F));
        return 4;
    }

    SXEL2("sxe_unicode_to_utf8: %u is not a valid unicode code point", unicode);
    return 0;
}
