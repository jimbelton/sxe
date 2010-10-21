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

#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "mock.h"
#include "sxe-time.h"
#include "tap.h"

#define TEST_1MS 1000000    /* Number of nanoseconds in a millisecond */

static struct timeval test_tv;

static int
test_mock_gettimeofday(struct timeval * __restrict tv, __timezone_ptr_t tz)
{
    (void)tz;
    memcpy(tv, &test_tv, sizeof(test_tv));
    return 0;
}

int
main(void)
{
    time_t   expected;
    time_t   actual;
    char     buffer[SXE_TIMESTAMP_LENGTH(3)];
    char     extra_buffer[256];
    SXE_TIME sxe_time;
    SXE_TIME sxe_time_got;
    SXE_TIME sxe_time_expected;
    double   double_time_got;

    plan_tests(11);

    time(&expected);
    actual = (time_t)sxe_get_time_in_seconds();
    ok((actual == expected) || (actual == expected + 1),
       "Actual time (%lu) is as expected (%lu)", actual, expected);

    time(&expected);
    sxe_time = sxe_time_get();
    actual   = sxe_time_to_unix_time(sxe_time);
    ok((actual == expected) || (actual == expected + 1),
       "Whole part %lu of SXE time (%llu) is as expected (%lu)", actual, sxe_time, expected);

    is_eq(sxe_time_to_string((SXE_TIME)987654321, buffer, sizeof(buffer)), "19700101000000.987",
          "Formatted SXE time as expected (%s)", buffer);
    sxe_time_to_string((SXE_TIME)expected << 32, extra_buffer, sizeof(extra_buffer));
    is_eq(&extra_buffer[14], ".000000000", "Zeroed fractional part formatted as expected (stamp=%s, time=%llx)", extra_buffer,
          (SXE_TIME)expected << 32);

    /* Due to roundoff errors, check accuracy to a millisecond (1000000 ns)
     */
    sxe_time_got      = sxe_time_from_double_seconds(1286482481.666);
    sxe_time_expected = ((SXE_TIME)1286482481 << 32) + (1ULL << 32) * 666000000 / 1000000000;

    ok((sxe_time_expected - sxe_time_got < TEST_1MS) || (sxe_time_got - sxe_time_expected < TEST_1MS),
       "Time is correctly encoded from a double (1286482481.666 - (%u,%u) < 0.001)", (uint32_t)(sxe_time_got >> 32),
       (uint32_t)sxe_time_got);

    MOCK_SET_HOOK(gettimeofday, test_mock_gettimeofday);

    test_tv.tv_sec  = 0;
    test_tv.tv_usec = 0;
    sxe_time        = sxe_time_get();
    ok(sxe_time == 0,                "tv(0,0) -> SXE time 0 (got %llu)", sxe_time);

    test_tv.tv_usec = 999999;
    sxe_time        = sxe_time_get();
    ok((1ULL << 32) - 1 - sxe_time < (1ULL << 32) / 1000, "tv(0,999999) -> SXE time ~%llu (got %llu, epsilon %llu)", (1ULL << 32) - 1,
       sxe_time, (1ULL << 32) / 1000);

    test_tv.tv_sec  = 1;
    test_tv.tv_usec = 0;
    sxe_time        = sxe_time_get();
    ok(sxe_time == (1ULL << 32), "tv(1, 0) -> SXE time 2^32 == %llu (got %llu)", 1ULL << 32, sxe_time);

    test_tv.tv_sec  = 1;
    test_tv.tv_usec = 666000;
    sxe_time        = sxe_time_get();
    double_time_got = sxe_time_to_double_seconds(sxe_time);
    ok(double_time_got - 1.666 < 0.000001, "Double time %f for SXE time 0x%llx is ~1.666 as expected", double_time_got, sxe_time);

    ok(sxe_time_from_double_seconds(double_time_got) == sxe_time, "Double time converted back to the same SXE time");

    actual = sxe_time_to_unix_time((sxe_time_from_double_seconds(1.9) - sxe_time_from_double_seconds(1.1)) * 1000);
    ok((actual > 798) && (actual < 802), "Difference 0.%03lu between 1.9 and 1.1 is > 0.798 and < 0.802", actual);
    return exit_status();
}
