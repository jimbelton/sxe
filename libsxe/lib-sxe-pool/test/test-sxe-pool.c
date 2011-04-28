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
#include <stdio.h>
#include <stddef.h>
#include <time.h>

#include "mock.h"
#include "sxe-log.h"
#include "sxe-pool.h"
#include "sxe-socket.h"
#include "sxe-util.h"
#include "tap.h"

enum TEST_STATE {
    TEST_STATE_FREE = 0,
    TEST_STATE_USED,
    TEST_STATE_ABUSED,
    TEST_STATE_NUMBER_OF_STATES   /* This is not a state; it MUST come last */
};

#define TEST_POOL_GET_NUMBER_FREE(  pool) sxe_pool_get_number_in_state(pool, TEST_STATE_FREE)
#define TEST_POOL_GET_NUMBER_USED(  pool) sxe_pool_get_number_in_state(pool, TEST_STATE_USED)
#define TEST_POOL_GET_NUMBER_ABUSED(pool) sxe_pool_get_number_in_state(pool, TEST_STATE_ABUSED)

unsigned test_pool_1_timeout_call_count = 0;
unsigned test_pool_2_timeout_call_count = 0;

static void
test_pool_1_timeout(void * array, unsigned array_index, void * caller_info)
{
    SXEE83("test_pool_1_timeout(array=%p,array_index=%u,caller_info=%p)", array, array_index, caller_info);
    SXE_UNUSED_PARAMETER(caller_info);
    test_pool_1_timeout_call_count ++;

    if (1 == test_pool_1_timeout_call_count) {
        sxe_pool_set_indexed_element_state(array, array_index, TEST_STATE_ABUSED, TEST_STATE_FREE);
    }

    if (2 == test_pool_1_timeout_call_count) {
        sxe_pool_set_indexed_element_state(array, array_index, TEST_STATE_USED,   TEST_STATE_FREE);
    }

    SXER80("return");
}

static void
test_pool_2_timeout(void * array, unsigned array_index, void * caller_info)
{
    SXEE82("test_pool_2_timeout(array=%p,array_index=%u)", array, array_index);
    SXE_UNUSED_PARAMETER(caller_info);
    test_pool_2_timeout_call_count ++;

    if (1 == test_pool_2_timeout_call_count) {
        sxe_pool_set_indexed_element_state(array, array_index, TEST_STATE_USED,   TEST_STATE_FREE);
    }

    if (2 == test_pool_2_timeout_call_count) {
        sxe_pool_set_indexed_element_state(array, array_index, TEST_STATE_ABUSED, TEST_STATE_USED);
    }

    if (3 == test_pool_2_timeout_call_count) {
        sxe_pool_set_indexed_element_state(array, array_index, TEST_STATE_USED,   TEST_STATE_FREE);
    }

    SXER80("return");
}

static void
test_pool_3_timeout(void * array, unsigned array_index, void * caller_info)
{
    SXEE82("test_pool_3_timeout(array=%p,array_index=%u)", array, array_index);
    SXE_UNUSED_PARAMETER(caller_info);
    sxe_pool_set_indexed_element_state(array, array_index, 0, 1);
    sxe_pool_set_indexed_element_state(array, array_index, 1, 0);
    SXER80("return");
}

static struct timeval test_mock_gettimeofday_timeval;

static int
test_mock_gettimeofday(struct timeval  * SXE_SOCKET_RESTRICT tv,
#ifdef __APPLE__
                       void            * SXE_SOCKET_RESTRICT tz
#else
                       struct timezone * SXE_SOCKET_RESTRICT tz
#endif
                      )
{
    /* Note: It's safe to use log functions here because they don't use mocked versions of gettimeofday() :-) */
    SXEE63("%s(tv=%p, tz=%p)", __func__, tv, tz);
    SXEA60(tv != NULL, "tv must never contain NULL");
    SXE_UNUSED_PARAMETER(tz);
    memcpy(tv, &test_mock_gettimeofday_timeval, sizeof(*tv));
    SXER61("return // gettimeofday: %f", (double)tv->tv_sec + 1.e-6 * (double)tv->tv_usec);
    return 0;
}

int
main(void)
{
    unsigned      * pool;
    size_t          size;
    void          * base[2];
    unsigned        i;
    SXE_POOL_WALKER walker;
    unsigned        j;
    unsigned        id;
    unsigned      * pool_1_timeout;
    unsigned      * pool_2_timeout;
    unsigned      * pool_3_timeout;
    double          pool_1_timeouts[] = {0.0, 4.00, 3.00};
    double          pool_2_timeouts[] = {0.0, 1.00, 2.00};
    double          pool_3_timeouts[] = {1.0, 0.00};
    unsigned        node_a;
    unsigned        node_b;
    unsigned        oldest;
    time_t          time_0;
    SXE_TIME        oldtime;
    SXE_TIME        curtime;
    char            timestamp[SXE_TIMESTAMP_LENGTH(3)];
    unsigned        state;
    unsigned        oldest_index;

    time(&time_0);
    plan_tests(120);

    /* Initialization causes expected state
     */
    ok((size = sxe_pool_size(4, sizeof(*pool), TEST_STATE_NUMBER_OF_STATES)) >= 4 * sizeof(*pool),
       "Expect pool size %u to be at least the size of the array %u", (unsigned)size, 4 * (unsigned)sizeof(*pool));
    SXEA10((base[0] = malloc(size)) != NULL,                                  "Couldn't allocate memory for 1st copy of pool");
    pool = sxe_pool_construct(base[0], "cesspool", 4, sizeof(*pool), TEST_STATE_NUMBER_OF_STATES, SXE_POOL_OPTION_TIMED);
    SXEA10((base[1] = malloc(size)) != NULL,                                  "Couldn't allocate memory for 2nd copy of pool");
    memcpy(base[1], base[0], size);

    is_eq(sxe_pool_get_name(pool),            "cesspool",                     "sxe_pool_get_name() works");

    for (i = 0; i < 2; i++) {
        if (i == 1) {
            pool = sxe_pool_from_base(base[1]);
            memset(base[0], 0xF0, size);
            is(base[1],                       sxe_pool_to_base(pool),         "Pool %u array maps back to pool", i);

        }
        else {
            is(pool,                          sxe_pool_from_base(base[i]),    "Pool %u array is the expected one", i);
        }

        is(TEST_POOL_GET_NUMBER_FREE(pool),   4,                              "4 free objects in newly created pool");
        sxe_pool_walker_construct(&walker, pool, TEST_STATE_FREE);

        for (j = 0; (id = sxe_pool_walker_step(&walker)) != SXE_POOL_NO_INDEX; j++) {
            SXEA11(id <= 3, "test_visit_count: id %u is > 3", id);
        }

        is(j,                                 4,                              "Visited 4 free objects in newly created pool");
        is(TEST_POOL_GET_NUMBER_USED(pool),   0,                              "0 used objects in newly created pool");
        is(TEST_POOL_GET_NUMBER_ABUSED(pool), 0,                              "0 abused objects in newly created pool");
        ok(sxe_pool_get_oldest_element_index(pool, TEST_STATE_USED) == SXE_POOL_NO_INDEX, "Oldest object is SXE_POOL_NO_INDEX when pool is empty");
        ok(sxe_pool_get_oldest_element_time( pool, TEST_STATE_USED) == 0,     "Oldest time is 0 when pool is empty");

        /*****
         * Verify the state between each of the following operations
         * - Use 2 nodes (0 and 2)
         * - Touch 0 (oldest used)
         * - Mark 0 as free
         * - Touch 0 (a free node)
         * - Mark 2 as free
         * (Note, assertions in the code catch double free and double use)
         */
        sxe_pool_set_indexed_element_state(pool, 0, TEST_STATE_FREE, TEST_STATE_USED);
        sxe_pool_set_indexed_element_state(pool, 2, TEST_STATE_FREE, TEST_STATE_USED);
        is(sxe_pool_index_to_state(pool, 0), TEST_STATE_USED,                 "Index 0 is in use");
        is(sxe_pool_index_to_state(pool, 2), TEST_STATE_USED,                 "Index 2 is in use");

        oldtime = sxe_pool_get_oldest_element_time(pool, TEST_STATE_USED);
        ok(oldtime                                                != 0,       "Pool not empty so oldest time is %s, not 0",
           sxe_time_to_string(oldtime, timestamp, sizeof(timestamp)));
        oldest_index = sxe_pool_get_oldest_element_index(pool, TEST_STATE_USED);
        ok(oldtime == sxe_pool_get_element_time_by_index(pool, oldest_index), "Get time by index matches get time by oldest element");

        is(TEST_POOL_GET_NUMBER_FREE(pool),  2  ,                             "Pool state is now: 2 free");
        is(TEST_POOL_GET_NUMBER_USED(pool),  2  ,                             "Pool state is now: 2 used");

        /* Wait for the next tick before touching your nodes :)
         */
        while ((curtime = sxe_time_get()) <= oldtime) {
        }

        sxe_pool_touch_indexed_element(pool, 0);
        is(sxe_pool_get_oldest_element_index(pool, TEST_STATE_USED), 2,       "After touching 0, element 2 should be oldest");
        sxe_pool_touch_indexed_element(pool, 2);
        is(sxe_pool_get_oldest_element_index(pool, TEST_STATE_USED), 0,       "After touching 2, element 0 should be oldest");
        ok(sxe_pool_get_oldest_element_time(pool, TEST_STATE_USED) > oldtime, "Touching element 0 updated its time to %s",
           sxe_time_to_string(sxe_pool_get_oldest_element_time(pool, TEST_STATE_USED), timestamp, sizeof(timestamp)));
        is(TEST_POOL_GET_NUMBER_FREE(pool),  2  ,                             "Pool state is now: 2 free");
        is(TEST_POOL_GET_NUMBER_USED(pool),  2  ,                             "Pool state is now: 2 used");

        state = TEST_STATE_FREE;
        is(sxe_pool_try_to_set_indexed_element_state(pool, 0, TEST_STATE_ABUSED, &state), SXE_POOL_INCORRECT_STATE,
                                                                              "Element 0 is not in ABUSED state");
        is(state, TEST_STATE_USED,                                            "Element 0 is in the USED state");
        sxe_pool_set_indexed_element_state(pool, 0, TEST_STATE_USED, TEST_STATE_FREE);
        is(sxe_pool_index_to_state(pool, 0), TEST_STATE_FREE,                 "Index 0 has now been freed");
        is(sxe_pool_index_to_state(pool, 2), TEST_STATE_USED,                 "Index 2 is still in use");
        ok(sxe_pool_get_oldest_element_time(pool, TEST_STATE_USED) >  0.0,    "Pool still not empty so oldest time is not 0");
        is(TEST_POOL_GET_NUMBER_FREE(pool),  3  ,                             "Pool state is now: 3 free");
        is(TEST_POOL_GET_NUMBER_USED(pool),  1  ,                             "Pool state is now: 1 used");

        sxe_pool_touch_indexed_element(pool, 0);
        is(sxe_pool_index_to_state(pool, 0), TEST_STATE_FREE,                 "Index 0 is still free");
        is(sxe_pool_index_to_state(pool, 2), TEST_STATE_USED,                 "Index 2 is still in use");
        ok(sxe_pool_get_oldest_element_time(pool, TEST_STATE_USED) >  0.0,    "Pool not empty so oldest time not 0");
        is(TEST_POOL_GET_NUMBER_FREE(pool),  3  ,                             "Touch of a free node doesn't change pool state");
        is(TEST_POOL_GET_NUMBER_USED(pool),  1  ,                             "Touch of a free node doesn't change pool state");

        sxe_pool_set_indexed_element_state(pool, 2, TEST_STATE_USED, TEST_STATE_FREE);
        is(sxe_pool_index_to_state(pool, 0), TEST_STATE_FREE,                 "Index 0 is still free");
        is(sxe_pool_index_to_state(pool, 2), TEST_STATE_FREE,                 "Index 2 is free as well");
        ok(sxe_pool_get_oldest_element_time(pool, TEST_STATE_USED) == 0.0,    "Pool is empty so oldest time is 0.0");
        is(TEST_POOL_GET_NUMBER_FREE(pool),   4  ,                            "Pool state is now: 4 free");
        is(TEST_POOL_GET_NUMBER_USED(pool),   0  ,                            "Pool state is now: 0 used");

        /* test allocating two objects consecutively using sxe_pool_next_free()
         */
        node_a = sxe_pool_set_oldest_element_state(pool, TEST_STATE_FREE, TEST_STATE_USED);
        is(sxe_pool_index_to_state(pool, node_a), TEST_STATE_USED,            "The node A given really is in use");
        node_b = sxe_pool_set_oldest_element_state(pool, TEST_STATE_FREE, TEST_STATE_USED);
        is(sxe_pool_index_to_state(pool, node_b), TEST_STATE_USED,            "The node B given really is in use");

        /* track the state of the pool (check for oldest time and object, used and freed objects)
         */
        is((oldest = sxe_pool_get_oldest_element_index(pool, TEST_STATE_USED)), node_a, "The oldest is node A");
        oldtime = sxe_pool_get_oldest_element_time(pool, TEST_STATE_USED);
        ok(oldtime >= ((SXE_TIME)time_0 << 32),                               "Oldest time %s >= start time %lu",
           sxe_time_to_string(oldtime, timestamp, sizeof(timestamp)), time_0);
        sxe_pool_touch_indexed_element(pool, node_a);
        is((oldest = sxe_pool_get_oldest_element_index(pool, TEST_STATE_USED)), node_b, "The oldest is now node B");
        is(TEST_POOL_GET_NUMBER_FREE(pool), 2,                                "Pool state is now: 2 free");
        is(TEST_POOL_GET_NUMBER_USED(pool), 2,                                "Pool state is now: 2 used");

        /* TODO: Better tests of the time values and how the oldest time changes
         *       in different scenarios (touch, mark_free, get_next_free, ...) */

        state = TEST_STATE_FREE;
        is(sxe_pool_try_to_set_indexed_element_state(pool, node_a, TEST_STATE_USED, &state), node_a,
                                                                              "Try to set succeeded in setting element state");
        is(state, TEST_STATE_FREE,                                            "New state was not changed, as expected");
        ok(sxe_pool_set_oldest_element_state(pool, TEST_STATE_FREE, TEST_STATE_USED) != node_a, "Doesn't immediately reallocate node A after freeing it");

        /* allocate all objects in pool, then test expected behaviour
         */
        ok(sxe_pool_set_oldest_element_state(pool, TEST_STATE_FREE, TEST_STATE_ABUSED) != SXE_POOL_NO_INDEX, "Allocated a third object");
        ok(sxe_pool_set_oldest_element_state(pool, TEST_STATE_FREE, TEST_STATE_ABUSED) != SXE_POOL_NO_INDEX, "Allocated a fourth object");
        ok(sxe_pool_set_oldest_element_state(pool, TEST_STATE_FREE, TEST_STATE_ABUSED) == SXE_POOL_NO_INDEX, "Couldn't allocate a fifth object");

        /* Make sure a third state works
         */
        is((oldest = sxe_pool_get_oldest_element_index(pool, TEST_STATE_ABUSED)), 2,         "Object 2 is oldest in abused state");
        is(sxe_pool_index_to_state(pool, oldest), TEST_STATE_ABUSED,                         "Object thinks it's abused");
        sxe_pool_set_indexed_element_state(pool, oldest, TEST_STATE_ABUSED, TEST_STATE_FREE);
        is(TEST_POOL_GET_NUMBER_FREE(pool),                          1,                      "Pool has one free object");
        is((oldest = sxe_pool_get_oldest_element_index(pool, TEST_STATE_ABUSED)), 3,         "Object 3 is in abused state");
        is((oldest = sxe_pool_get_oldest_element_index(pool, TEST_STATE_USED)),   1,         "Object 1 is in used state");
        is(sxe_pool_index_to_state(pool, oldest), TEST_STATE_USED,                           "Object thinks it's used");
        sxe_pool_set_indexed_element_state(pool, oldest, TEST_STATE_USED, TEST_STATE_FREE);
        is((oldest = sxe_pool_get_oldest_element_index(pool, TEST_STATE_USED)),   0,         "Object 0 is in used state");
    }

    #define TEST_TIMEOUT "Test timeout: "

    SXEA11(gettimeofday(&test_mock_gettimeofday_timeval, NULL) == 0, "Failed to get time of day: %s", strerror(errno));
    MOCK_SET_HOOK(gettimeofday, test_mock_gettimeofday);    /* Hook gettimeofday to mock it */

    pool_1_timeout = sxe_pool_new_with_timeouts("pool_1_timeout", 4, sizeof(*pool_1_timeout), TEST_STATE_NUMBER_OF_STATES,
                     pool_1_timeouts, test_pool_1_timeout, NULL);
    pool_2_timeout = sxe_pool_new_with_timeouts("pool_2_timeout", 4, sizeof(*pool_2_timeout), TEST_STATE_NUMBER_OF_STATES,
                     pool_2_timeouts, test_pool_2_timeout, NULL);
    test_mock_gettimeofday_timeval.tv_sec += 100;
    sxe_pool_check_timeouts();
    is(test_pool_1_timeout_call_count, 0, TEST_TIMEOUT "test_pool_1_timeout() not called; after 100 seconds: all elements still in TEST_STATE_FREE with infinite timeout");
    is(test_pool_2_timeout_call_count, 0, TEST_TIMEOUT "test_pool_2_timeout() not called; after 100 seconds: all elements still in TEST_STATE_FREE with infinite timeout");
    sxe_pool_set_oldest_element_state(pool_1_timeout, TEST_STATE_FREE, TEST_STATE_USED  );
    sxe_pool_set_oldest_element_state(pool_1_timeout, TEST_STATE_FREE, TEST_STATE_ABUSED);
    sxe_pool_set_oldest_element_state(pool_2_timeout, TEST_STATE_FREE, TEST_STATE_USED  );
    sxe_pool_set_oldest_element_state(pool_2_timeout, TEST_STATE_FREE, TEST_STATE_ABUSED);
    test_mock_gettimeofday_timeval.tv_sec += 1;
    sxe_pool_check_timeouts();
    is(test_pool_1_timeout_call_count, 0, TEST_TIMEOUT "test_pool_1_timeout() not called; after 1 second(s): elements with TEST_STATE_USED/TEST_STATE_ABUSED have rest time 3/2 seconds");
    is(test_pool_2_timeout_call_count, 1, TEST_TIMEOUT "test_pool_2_timeout()     called; after 1 second(s): elements with TEST_STATE_USED/TEST_STATE_ABUSED have rest time 0/1 seconds");
    test_mock_gettimeofday_timeval.tv_sec += 2;
    sxe_pool_check_timeouts();
    is(test_pool_1_timeout_call_count, 1, TEST_TIMEOUT "test_pool_1_timeout()     called; after 2 second(s): elements with TEST_STATE_USED/TEST_STATE_ABUSED have rest time 1/0 seconds");
    is(test_pool_2_timeout_call_count, 2, TEST_TIMEOUT "test_pool_2_timeout()     called; after 2 second(s): elements with ---------------/TEST_STATE_ABUSED have rest time -/-1 seconds");
    test_mock_gettimeofday_timeval.tv_sec += 1;
    sxe_pool_check_timeouts();
    is(test_pool_1_timeout_call_count, 2, TEST_TIMEOUT "test_pool_1_timeout()     called; after 1 second(s): elements with TEST_STATE_USED/----------------- have rest time 0/- seconds");
    is(test_pool_2_timeout_call_count, 3, TEST_TIMEOUT "test_pool_2_timeout()     called; after 1 second(s): elements with TEST_STATE_USED/----------------- have rest time 0/- seconds");

    sxe_pool_delete(pool_1_timeout);   /* For coverage */

    /* timeout with a pool of 1 objects */
    pool_3_timeout = sxe_pool_new_with_timeouts("pool_3_timeout", 1, sizeof(*pool_3_timeout), 2, pool_3_timeouts,
                                                test_pool_3_timeout, NULL);
    test_mock_gettimeofday_timeval.tv_sec += 2;
    sxe_pool_check_timeouts();

    return exit_status();
}
