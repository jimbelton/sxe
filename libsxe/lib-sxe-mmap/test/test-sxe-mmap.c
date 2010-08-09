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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "sxe-log.h"
#include "sxe-mmap.h"
#include "sxe-spawn.h"
#include "sxe-spinlock.h"
#include "sxe-test.h"
#include "tap.h"

#define TEST_FILE_SIZE      (1024 * 1024 * 64)
#define TEST_MMAP_INSTANCES 32
#define TEST_WAIT           4.0
#if SXE_COVERAGE
#define TEST_ITERATIONS     5000
#else
#define TEST_ITERATIONS     25000
//#define TEST_PING_PONGS     500000 /* uncomment this to play ping pong */
#endif

int
main(int argc, char ** argv)
{
#ifdef WINDOWS_NT
    SXEL10("WARNING: Need to implement sxe_spawn() on Windows to run this test file!");
#else
    int                     fd;
    unsigned                instance;
    SXE_RETURN              result;
    SXE_MMAP                memmap;
    volatile unsigned     * shared;
    volatile SXE_SPINLOCK * shared_spinlock;
    SXE_SPAWN               hell_spawn[TEST_MMAP_INSTANCES];
    unsigned                i;
    unsigned                total;
    unsigned                count;
    unsigned                count_hi = 0;
    double                  start_time;
#if defined TEST_PING_PONGS
    double                  time_before;
    double                  time_after;
    unsigned                yield_count = 0;
#endif

    if (argc > 1) {
        instance = atoi(argv[1]);
        sxe_mmap_open(&memmap, "memmap");
        shared  = SXE_MMAP_ADDR(&memmap);
        SXEL11("Instance %u about to set shared memory", instance);
        shared[instance] = instance;
        SXED10((char *)(unsigned long)SXE_MMAP_ADDR(&memmap), sizeof(long) * (TEST_MMAP_INSTANCES + 2));
        SXEL11("Instance %u just     set shared memory", instance);
        SXEA11(shared[instance] == instance, "WTF! Thought I wrote %u", instance);

        shared_spinlock = (SXE_SPINLOCK *)(unsigned)(shared + 1024);

        InterlockedAdd((long* )(unsigned)&shared[0], 1);
        start_time = sxe_get_time_in_seconds();
        while (shared[0] != TEST_MMAP_INSTANCES) {
            SXEA10((TEST_WAIT + start_time ) > sxe_get_time_in_seconds(), "Unexpected timeout... is the hardware too slow?");
            usleep(10000);
        }
        SXEL11("Instance %u ready to rumble", instance);

        start_time = sxe_get_time_in_seconds();
        for (i = 0; i < TEST_ITERATIONS; i++) {
            SXEA10((TEST_WAIT + start_time ) > sxe_get_time_in_seconds(), "Unexpected timeout... is the hardware too slow?");
            SXE_SPINLOCK_TAKE(shared_spinlock[0], 1, 0, count);
            SXEA12(SXE_SPINLOCK_IS_TAKEN(count), "Instance %u failed to take lock // lock-count=%u",
                                                 instance, count);
            count_hi = count_hi < count ? count : count_hi;
            shared_spinlock[1           ].lock++;
            shared_spinlock[1 + instance].lock++;
            SXE_SPINLOCK_GIVE(shared_spinlock[0], 0, 1);
        }

        start_time = sxe_get_time_in_seconds();
        while (shared[0] != 0xDEADBABE) {
            SXEA10((TEST_WAIT + start_time ) > sxe_get_time_in_seconds(), "Unexpected timeout... is the hardware too slow?");
            usleep(10000);
        }

#if defined TEST_PING_PONGS
        /* Instance 1 will stick around to play some ping pong.
         */
        if (instance == 1) {
            for (i = 0; i < TEST_PING_PONGS; i++) {
                SXE_SPINLOCK_TAKE(shared_spinlock[0], 1, 0, count);
                SXEA12(SXE_SPINLOCK_IS_TAKEN(count), "Instance %u failed to take lock // lock-count=%u",
                                                      instance, count);
                count_hi = count_hi < count ? count : count_hi;
                sxe_get_time_in_seconds(); /* causes child to delay very slightly */
            }
        }
#endif

        sxe_mmap_close(&memmap);
        SXEL12("Instance %2u exiting // count_hi %u", instance, count_hi);
        return 0;
    }

    plan_tests(3);

    /* TODO: Make multiple simultaneous runs safe */

    SXEA11((fd = open("memmap", O_CREAT | O_TRUNC | O_WRONLY, 0666)) >= 0, "Failed to create file 'memmap': %s",         strerror(errno));
    SXEA12(ftruncate(fd, TEST_FILE_SIZE)                             >= 0, "Failed to extend the file to %lu bytes: %s", TEST_FILE_SIZE, strerror(errno));

    close(fd);
    sxe_register(TEST_MMAP_INSTANCES + 1, 0);
    SXEA11((result = sxe_init()) == SXE_RETURN_OK,                         "Failed to initialize the SXE package: %s",    sxe_return_to_string(result));

    sxe_mmap_open(&memmap, "memmap");
    shared = SXE_MMAP_ADDR(&memmap);

    shared_spinlock = (SXE_SPINLOCK *)(unsigned)(shared + 1024);
    SXE_SPINLOCK_INIT(shared_spinlock[0], 0);
    SXE_SPINLOCK_INIT(shared_spinlock[1], 0);
    shared[0] = 0;

    for (instance = 1; instance <= TEST_MMAP_INSTANCES; instance++) {
        char buffer[12];

        snprintf(buffer, sizeof(buffer), "%u", instance);
        result = sxe_spawn(NULL, &hell_spawn[instance - 1], argv[0], buffer, NULL, NULL, NULL, NULL);
        SXEA13(result == SXE_RETURN_OK, "Failed to spawn '%s %s': %s", argv[0], buffer, sxe_return_to_string(result));
    }

    i = 0;
    start_time = sxe_get_time_in_seconds();
    while (i < TEST_MMAP_INSTANCES) {
        SXEA10((TEST_WAIT + start_time ) > sxe_get_time_in_seconds(), "Unexpected timeout... is the hardware too slow?");
        usleep(10000);

        for (i = 0, instance = 1; instance <= TEST_MMAP_INSTANCES; instance++) {
            i += shared[instance] == instance ? 1 : 0;
        }
    };

    is(i, TEST_MMAP_INSTANCES, "All %u children have set their instance numbers", TEST_MMAP_INSTANCES);

    start_time = sxe_get_time_in_seconds();
    while (shared_spinlock[1].lock < (TEST_MMAP_INSTANCES * TEST_ITERATIONS)) {
        SXEA10((TEST_WAIT + start_time ) > sxe_get_time_in_seconds(), "Unexpected timeout... is the hardware too slow?");
        usleep(10000);
    }

    total = 0;
    for (instance = 1; instance <= TEST_MMAP_INSTANCES; instance++) {
        total += shared_spinlock[1 + instance].lock;
        SXEL12("Instance %u incremented %d times", instance, shared_spinlock[1 + instance].lock);
    }

    is(total                  , (TEST_MMAP_INSTANCES * TEST_ITERATIONS), "Total of counts is as expected");
    is(shared_spinlock[1].lock, (TEST_MMAP_INSTANCES * TEST_ITERATIONS), "Shared count    is as expected");
    shared[0] = 0xDEADBABE;

#if defined TEST_PING_PONGS
    /* Play some ping pong; Instance 1 will go first.
     */
    time_before = sxe_get_time_in_seconds();
    for (i = 0; i < TEST_PING_PONGS; i++) {
            SXE_SPINLOCK_TAKE(shared_spinlock[0], 0, 1, count);
            SXEA11(SXE_SPINLOCK_IS_TAKEN(count), "Parent failed to take lock // lock-count=%u",
                                                       count);
            yield_count += count;
    }
    time_after = sxe_get_time_in_seconds();
    SXEL14("Just switched back and forth %u times in %f seconds, or %f times/second; yield_count=%u", TEST_PING_PONGS, (time_after - time_before), TEST_PING_PONGS / (time_after - time_before), yield_count);
#endif

    start_time = sxe_get_time_in_seconds();
    for (instance = 0; (instance < TEST_MMAP_INSTANCES); instance++) {
        SXEA10((TEST_WAIT + start_time ) > sxe_get_time_in_seconds(), "Unexpected timeout... is the hardware too slow?");
        waitpid(hell_spawn[instance].pid, NULL, 0);
    }

    sxe_mmap_close(&memmap);
    return exit_status();
#endif
}
