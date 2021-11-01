#include <string.h>
#include <tap.h>

#include "sxe-unicode.h"

int
main(void)
{
    char utf8[8];

    plan_tests(9);
    memset(utf8, 0, sizeof(utf8));

    is(sxe_unicode_to_utf8('A', utf8),        1,                  "'A' is encoded in one byte");
    is_eq(utf8,                               "A",                "'A' is encoded as 'A'");
    is(sxe_unicode_to_utf8(0xA2, utf8),       2,                  "Non-ASCII ISO 8851 characters take 2 bytes");    // Cent sign
    is_eq(utf8,                               "\xC2\xA2",         "0xA2 is encoded as 'A'");
    is(sxe_unicode_to_utf8(0x20AC, utf8),     3,                  "'\u20AC' is encoded as 3 bytes");
    is_eq(utf8,                               "\xE2\x82\xAC",     "0x20AC is encoded as 0xE2 0x82 0xAC");
    is(sxe_unicode_to_utf8(0x10348, utf8),    4,                  "Gothic letter hwair is encoded as 4 bytes");
    is_eq(utf8,                               "\xF0\x90\x8D\x88", "'\U00010348' is encoded as 0xF0 0x90 0x8D 0x88");
    is(sxe_unicode_to_utf8(0xFFFFFFFF, utf8), 0,                  "0xFFFFFFFF is an invalid code point");

    return exit_status();
}
