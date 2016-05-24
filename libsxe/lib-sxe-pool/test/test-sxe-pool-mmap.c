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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <process.h>    /* For spawn() */
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#include "sxe-log.h"
#include "sxe-mmap.h"
#include "sxe-spinlock.h"
#include "sxe-pool.h"
#include "tap.h"

#define TEST_WAIT             4.0
#define TEST_CLIENT_INSTANCES 32

enum TEST_STATE {
    TEST_STATE_FREE,
    TEST_STATE_CLIENT_TAKE,
    TEST_STATE_CLIENT_DONE,
    TEST_STATE_SERVER,
    TEST_STATE_NUMBER_OF_STATES
};

int
main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;
#if 0    /* COWARDLY DISABLING TEST FOR NOW */
    int             fd;
    double          start_time;
    unsigned        count;
    unsigned        id;
    unsigned      * pool;
    unsigned      * shared;
    size_t          size;
    SXE_MMAP        memmap;
    int             child[TEST_CLIENT_INSTANCES];
    SXE_POOL_WALKER walker;

    if (argc > 1) {
        count = atoi(argv[1]);
        sxe_mmap_open(&memmap, "memmap");
        shared  = (unsigned *)(unsigned long)SXE_MMAP_ADDR(&memmap);
        pool    = sxe_pool_from_base(shared);
        SXEL63("Instance %u mapped to shared pool // base=%p, pool=%p", count, shared, pool);

        do {
            usleep(10000 * count);
            id = sxe_pool_set_oldest_element_state(pool, TEST_STATE_FREE, TEST_STATE_CLIENT_TAKE);
            SXEA10(id != SXE_POOL_LOCK_NOT_TAKEN, "Got SXE_POOL_LOCK_NOT_TAKEN");;
        } while (id == SXE_POOL_NO_INDEX);

        SXEL62("Instance %u got pool element %u", count, id);
        pool[id] = count;
        sxe_pool_set_indexed_element_state(pool, id, TEST_STATE_CLIENT_TAKE, TEST_STATE_CLIENT_DONE);
        sxe_mmap_close(&memmap);
        SXEL61("Instance %u exiting", count);
        return 0;
    }

    plan_tests(6);
    ok((size = sxe_pool_size(TEST_CLIENT_INSTANCES/2, sizeof(*pool), TEST_STATE_NUMBER_OF_STATES))
               >= TEST_CLIENT_INSTANCES * sizeof(*pool),
       "Expect pool size %"PRIuPTR" to be at least the size of the array %"PRIuPTR"",
       (uintptr_t)size, (uintptr_t)(TEST_CLIENT_INSTANCES * sizeof(*pool)));

    SXEA11((fd = open("memmap", O_CREAT | O_TRUNC | O_WRONLY, 0666)) >= 0, "Failed to create file 'memmap': %s",         strerror(errno));
    SXEA12(ftruncate(fd, size)                                       >= 0, "Failed to extend the file to %lu bytes: %s", size, strerror(errno));
    close(fd);
    sxe_mmap_open(&memmap, "memmap");
    shared = (unsigned *)(unsigned long)SXE_MMAP_ADDR(&memmap);

    pool = sxe_pool_construct(shared, "shared-pool", TEST_CLIENT_INSTANCES/2, sizeof(*pool), TEST_STATE_NUMBER_OF_STATES,
                              SXE_POOL_OPTION_LOCKED);

    for (count = 0; count < TEST_CLIENT_INSTANCES; count++) {
        char buffer[12];

        snprintf(buffer, sizeof(buffer), "%u", count + 1);
        child[count] = spawnl(P_NOWAIT, argv[0], argv[0], buffer, NULL);
        SXEA13(child[count] != -1, "Failed to spawn '%s %s': %s", argv[0], buffer, strerror(errno));
    }

    start_time = sxe_get_time_in_seconds();

    for (count = 0; (count < TEST_CLIENT_INSTANCES); ) {
        SXEA10((TEST_WAIT + start_time ) > sxe_get_time_in_seconds(), "Unexpected timeout... is the hardware too slow?");
        usleep(10000);
        id = sxe_pool_set_oldest_element_state(pool, TEST_STATE_CLIENT_DONE, TEST_STATE_FREE);

        /* Assert here in  the test. The actual service would take specific action here */
        SXEA12(id != SXE_POOL_LOCK_NOT_TAKEN, "Parent: Failed to acqure lock .. yield limit reached. id %u vs %u", id,
               SXE_POOL_LOCK_NOT_TAKEN);

        if (id != SXE_POOL_NO_INDEX) {
            SXEL62("Looks like instance %u got element %u", pool[id], id);
            count++;
        }
    }

    ok(count == TEST_CLIENT_INSTANCES, "All clients got an element in the pool");
    start_time = sxe_get_time_in_seconds();

    for (count = 0; (count < TEST_CLIENT_INSTANCES); count++) {
        SXEA10((TEST_WAIT + start_time ) > sxe_get_time_in_seconds(), "Unexpected timeout... is the hardware too slow?");
        SXEA12(cwait(NULL, child[count], WAIT_CHILD) == child[count], "Unexpected return from cwait for process 0x%08x: %s",
               child[count], strerror(errno));
    }

    sxe_spinlock_take((SXE_SPINLOCK *)sxe_pool_to_base(pool));
    is(sxe_pool_set_indexed_element_state(pool, 0, 0, 1), SXE_POOL_LOCK_NOT_TAKEN, "Can't set state of an element if the pool is locked");
    is(sxe_pool_set_oldest_element_state( pool,    0, 1), SXE_POOL_LOCK_NOT_TAKEN, "Can't set state of oldest element if the pool is locked");
    is(sxe_pool_touch_indexed_element(    pool, 0),       SXE_POOL_LOCK_NOT_TAKEN, "Can't touch an element if the pool is locked");
    sxe_pool_walker_construct(&walker, pool, TEST_STATE_FREE);
    is(sxe_pool_walker_step(&walker),                     SXE_POOL_LOCK_NOT_TAKEN, "Can't step a walker if the pool is locked");
    sxe_spinlock_give((SXE_SPINLOCK *)sxe_pool_to_base(pool));

    sxe_pool_override_locked(pool); /* for coverage */
    sxe_mmap_close(&memmap);
    return exit_status();
#endif
    return 0;
}
