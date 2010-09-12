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
#include <time.h>

#include "mock.h"
#include "sxe-pool.h"
#include "sxe-socket.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_ELEMENTS 3

enum TEST_STATE {
    TEST_STATE_FREE = 0,
    TEST_STATE_USED,
    TEST_STATE_NUMBER_OF_STATES   /* This is not a state; it MUST come last */
};

#define TEST_POOL_GET_NUMBER_FREE(  pool) sxe_pool_get_number_in_state(pool, TEST_STATE_FREE)
#define TEST_POOL_GET_NUMBER_USED(  pool) sxe_pool_get_number_in_state(pool, TEST_STATE_USED)
#define TEST_POOL_GET_NUMBER_ABUSED(pool) sxe_pool_get_number_in_state(pool, TEST_STATE_ABUSED)

static struct timeval test_mock_gettimeofday_timeval;

static int
test_mock_gettimeofday(struct timeval * SXE_SOCKET_RESTRICT tv, struct timezone * SXE_SOCKET_RESTRICT tz)
{
    SXE_UNUSED_PARAMETER(tz);

    /* Note: It's safe to use log functions here because they don't use gettimeofday() :-) */
    SXEE63("%s(tv=%p, tz=%p)", __func__, tv, tz);
    SXEA60(tv != NULL, "tv must never contain NULL");
    memcpy(tv, &test_mock_gettimeofday_timeval, sizeof(*tv));
    SXER61("return // gettimeofday: %f", (double)tv->tv_sec + 1.e-6 * (double)tv->tv_usec);
    return 0;
}

int
main(void)
{
    unsigned      * pool;
    unsigned        i;
    SXE_POOL_WALKER walker;

    plan_tests(8);
    gettimeofday(&test_mock_gettimeofday_timeval, NULL);
    MOCK_SET_HOOK(gettimeofday, test_mock_gettimeofday);    /* Hook gettimeofday to mock it */

    ok((pool = sxe_pool_new("tub", TEST_ELEMENTS, sizeof(unsigned), TEST_STATE_NUMBER_OF_STATES, SXE_POOL_LOCKS_ENABLED)) != NULL,
       "Allocated the tub");

    /* Get the elements into a consistent order.
     */
    for (i = 0; i < TEST_ELEMENTS; i++) {
        pool[i] = i;
        is(sxe_pool_touch_indexed_element(pool, i), SXE_POOL_LOCK_TAKEN, "Touching element %u succeeded", i);
    }

    sxe_pool_walker_construct(&walker, pool, TEST_STATE_FREE);
    is(sxe_pool_walker_step(&walker), 0, "Element 0 is first");
    is(sxe_pool_walker_step(&walker), 1, "Element 1 is second");
    is(sxe_pool_set_indexed_element_state(pool, 1, TEST_STATE_FREE, TEST_STATE_USED), SXE_POOL_LOCK_TAKEN, "Moving element 1 to used queue succeeded");
    is(sxe_pool_walker_step(&walker), 2, "Element 2 is third");

    return exit_status();
}
