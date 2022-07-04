#include "sxe-util.h"
#include "tap.h"

int
main(void)
{
    plan_tests(13);

    is(sxe_uint64_log2(1),                      0, "sxe_uint64_log2(1) == 0");
    is(sxe_uint64_log2(2),                      1, "sxe_uint64_log2(2) == 1");
    is(sxe_uint64_log2(3),                      1, "sxe_uint64_log2(3) == 1");
    is(sxe_uint64_log2(4),                      2, "sxe_uint64_log2(4) == 2");
    is(sxe_uint64_log2(128),                    7, "sxe_uint64_log2(128) == 7");
    is(sxe_uint64_log2(256),                    8, "sxe_uint64_log2(256) == 0");
    is(sxe_uint64_log2(0x10000),               16, "sxe_uint64_log2(0x10000) == 16");
    is(sxe_uint64_log2(0x1000000),             24, "sxe_uint64_log2(0x1000000) == 24");
    is(sxe_uint64_log2(0xFFFFFFFFFFFFFFFFULL), 63, "sxe_uint64_log2((0xFFFFFFFFFFFFFFFFULL) == 31");

    is(sxe_uint64_align(7, 9), 9,          "Align on an odd multiple");
    is(sxe_uint64_align(1, 4096), 4096,    "Align on a page boundary");
    is(sxe_uint64_align(8192, 4096), 8192, "Already aligned on a page boundary");
    is(sxe_uint64_align(0xEFFFFFFFFFFFFFFFULL, 4096), 0xF000000000000000ULL,
       "(sxe_uint64_align(0xEFFFFFFFFFFFFFFFULL, 4096) == 0xF000000000000000ULL");

    return exit_status();
}
