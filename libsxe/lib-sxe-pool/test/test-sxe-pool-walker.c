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

#include "sxe-log.h"
#include "sxe-pool.h"
#include "tap.h"

#define TEST_ELEMENT_NUMBER 5

typedef unsigned TEST_ELEMENT_TYPE;

typedef enum TEST_STATE {
    TEST_STATE_FREE,
    TEST_STATE_USED,
    TEST_ELEMENT_NUMBER_OF_STATES    /* This is not a state and must be last */
} TEST_STATE;

int
main(void)
{
    TEST_ELEMENT_TYPE * array;
    SXE_POOL_WALKER     walker;

    plan_tests(9);
    array    = sxe_pool_new("looppool", TEST_ELEMENT_NUMBER, sizeof(TEST_ELEMENT_TYPE), TEST_ELEMENT_NUMBER_OF_STATES,
                            SXE_POOL_OPTION_LOCKED);

    sxe_pool_walker_construct(&walker, array, TEST_STATE_FREE);
    is(sxe_pool_get_oldest_element_count(array, TEST_STATE_FREE),                  1, "Oldest element has insertion count 1");
    is(sxe_pool_set_oldest_element_state(array, TEST_STATE_FREE, TEST_STATE_USED), 4, "Allocated the oldest element (4)");
    is(sxe_pool_get_oldest_element_count(array, TEST_STATE_FREE),                  2, "2nd oldest element has insertion count 2");
    is(sxe_pool_walker_step(&walker), 3,                                              "First step in free list is (3)");
    is(sxe_pool_set_oldest_element_state(array, TEST_STATE_FREE, TEST_STATE_USED), 3, "Allocated the oldest element (3)");
    is(sxe_pool_walker_step(&walker), 2,                                              "Second step in free list is (2)");
    is(sxe_pool_walker_step(&walker), 1,                                              "Third step in free list is (1)");
    is(sxe_pool_set_indexed_element_state(array, 1, TEST_STATE_FREE, TEST_STATE_USED),
       SXE_POOL_LOCK_TAKEN,                                                           "Moved current element (1) to used list");
    is(sxe_pool_walker_step(&walker), 0,                                              "Fourth step in free list is (0)");
    return exit_status();
}
