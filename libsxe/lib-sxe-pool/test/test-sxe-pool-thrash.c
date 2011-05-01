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

#include <stdio.h>
#include <string.h>

#include "sxe-log.h"
#include "sxe-pool.h"
#include "sxe-spinlock.h"
#include "sxe-thread.h"
#include "tap.h"

#define TEST_ELEMENT_NUMBER 4

typedef unsigned TEST_ELEMENT_TYPE;

typedef enum TEST_STATE {
    TEST_STATE_FREE,
    TEST_STATE_USED,
    TEST_ELEMENT_NUMBER_OF_STATES    /* This is not a state and must be last */
} TEST_STATE;

typedef struct TEST_STATS {
    unsigned shoves;
    unsigned empties;
    unsigned walks;
    unsigned steps;
} TEST_STATS;

TEST_ELEMENT_TYPE * test_array;
TEST_STATS          test_stats[TEST_ELEMENT_NUMBER_OF_STATES];

static const char *
test_state_to_string(unsigned state)
{
    switch (state) {
    case TEST_STATE_FREE: return "FREE";
    case TEST_STATE_USED: return "USED";
    }

    return NULL;
}

static SXE_THREAD_RETURN SXE_STDCALL
test_mover_thread(void * state_as_ptr)
{
    TEST_STATE from_state = (TEST_STATE)state_as_ptr;
    TEST_STATE to_state   = from_state == TEST_STATE_FREE ? TEST_STATE_USED : TEST_STATE_FREE;

    SXEE81("test_mover_thread(from_state=%s)", test_state_to_string(from_state));

    for (;;) {
        switch(sxe_pool_set_oldest_element_state(test_array, from_state, to_state)) {
        case SXE_POOL_LOCK_NOT_TAKEN: SXEA10(SXE_POOL_LOCK_NOT_TAKEN, "Lock timed out"); break;
        case SXE_POOL_NO_INDEX:       test_stats[from_state].empties++; SXE_YIELD();     break;
        default:                      test_stats[from_state].shoves++;                   break;
        }
    }

    SXER80("return 0");
    return 0;
}

static SXE_THREAD_RETURN SXE_STDCALL
test_walker_thread(void * state_as_ptr)
{
    TEST_STATE      state = (TEST_STATE)state_as_ptr;
    SXE_POOL_WALKER walker;
    unsigned        node;

    SXEE81("test_mover_thread(state=%s)", test_state_to_string(state));

    for (;;) {
        sxe_pool_walker_construct(&walker, test_array, state);

        for (;;) {
            switch(node = sxe_pool_walker_step(&walker)) {
            case SXE_POOL_LOCK_NOT_TAKEN: SXEA10(SXE_POOL_LOCK_NOT_TAKEN, "Lock timed out"); break;
            default:                                                                         break;
            }

            if (node == SXE_POOL_NO_INDEX) {
                break;
            }

            test_stats[state].steps++;
        }

        test_stats[state].walks++;

        if (sxe_pool_get_number_in_state(test_array, state) == 0) {
            SXE_YIELD();
        }
    }

    SXER80("return 0");
    return 0;
}

int
main(int argc, char * argv[])
{
    unsigned   seconds_to_run = 1;
    SXE_THREAD thread;
    TEST_STATE state;

    if (argc > 1) {
        if (strcmp(argv[1], "-i") != 0) {
            fprintf(stderr, "usage: test-sxe-pool-thrash [-i] - thrash a pool with multiple threads\n"
                            "       -i = Run forever (default = run for one second)\n");
            exit(1);
        }

        seconds_to_run = ~0U;
    }

    plan_tests(1);
    test_array  = sxe_pool_new("tidalpool", TEST_ELEMENT_NUMBER, sizeof(TEST_ELEMENT_TYPE), TEST_ELEMENT_NUMBER_OF_STATES,
                               SXE_POOL_OPTION_LOCKED);
    sxe_pool_set_state_to_string(test_array, test_state_to_string);
    SXEA10(sxe_thread_create(&thread, test_mover_thread,  (void *)TEST_STATE_FREE, SXE_THREAD_OPTION_DEFAULTS) == SXE_RETURN_OK,
           "Unable to create thread");
    SXEA10(sxe_thread_create(&thread, test_walker_thread, (void *)TEST_STATE_FREE, SXE_THREAD_OPTION_DEFAULTS) == SXE_RETURN_OK,
           "Unable to create thread");
    SXEA10(sxe_thread_create(&thread, test_mover_thread,  (void *)TEST_STATE_USED, SXE_THREAD_OPTION_DEFAULTS) == SXE_RETURN_OK,
           "Unable to create thread");
    SXEA10(sxe_thread_create(&thread, test_walker_thread, (void *)TEST_STATE_USED, SXE_THREAD_OPTION_DEFAULTS) == SXE_RETURN_OK,
           "Unable to create thread");

    while (seconds_to_run--) {
        usleep(1000000);    /* 1 second */

        fprintf(stderr, "\nState     Shoves    Empties      Walks      Steps\n");

        for (state = TEST_STATE_FREE; state < TEST_ELEMENT_NUMBER_OF_STATES; state++) {
            fprintf(stderr, "%.5s %10u %10u %10u %10u\n", state == TEST_STATE_FREE ? "FREE" : "USED", test_stats[state].shoves,
                    test_stats[state].empties, test_stats[state].walks, test_stats[state].steps);
        }
    }

    pass("Ran for one second without aborting");
    return exit_status();
}
