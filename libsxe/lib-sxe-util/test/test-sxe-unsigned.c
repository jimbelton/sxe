#include "sxe-util.h"
#include "tap.h"

int
main(void)
{
    plan_tests(16);

    is(sxe_unsigned_log2(1),          0,        "sxe_log2_unsigned(1)         == 0");
    is(sxe_unsigned_log2(2),          1,        "sxe_log2_unsigned(2)         == 1");
    is(sxe_unsigned_log2(3),          1,        "sxe_log2_unsigned(3)         == 1");
    is(sxe_unsigned_log2(4),          2,        "sxe_log2_unsigned(4)         == 2");
    is(sxe_unsigned_log2(128),        7,        "sxe_log2_unsigned(128)       == 7");
    is(sxe_unsigned_log2(256),        8,        "sxe_log2_unsigned(256)       == 0");
    is(sxe_unsigned_log2(0x10000),   16,        "sxe_log2_unsigned(0x10000)   == 16");
    is(sxe_unsigned_log2(0x1000000), 24,        "sxe_log2_unsigned(0x1000000) == 24");

    is(sxe_unsigned_mask(1),         1,         "sxe_unsigned_mask(1)         == 1");
    is(sxe_unsigned_mask(2),         3,         "sxe_unsigned_mask(2)         == 3");
    is(sxe_unsigned_mask(3),         3,         "sxe_unsigned_mask(3)         == 3");
    is(sxe_unsigned_mask(255),       255,       "sxe_unsigned_mask(255)       == 255");
    is(sxe_unsigned_mask(256),       511,       "sxe_unsigned_mask(256)       == 511");
    is(sxe_unsigned_mask(511),       511,       "sxe_unsigned_mask(511)       == 511");
    is(sxe_unsigned_mask(0xFFFF),    0xFFFF,    "sxe_unsigned_mask(0xFFFF)    == 0xFFFF");
    is(sxe_unsigned_mask(0x1000000), 0x1FFFFFF, "sxe_unsigned_mask(0x1000000) == 0x1FFFFFF");

    return exit_status();
}
