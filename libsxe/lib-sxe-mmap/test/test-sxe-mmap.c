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
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <unistd.h>     /* for PATH_MAX     on WIN32 */
#include <limits.h>     /* for PATH_MAX not on WIN32 */
#include <sys/types.h>
#include <sys/wait.h>

#include "sxe-log.h"
#include "sxe-mmap.h"
#include "sxe-spinlock.h"
#include "sxe-test.h"
#include "sxe-time.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_MMAP_INSTANCES 16
#define TEST_FILE_SIZE      (1024 * 1024 * 64)
#define TEST_WAIT           5.0

#if SXE_COVERAGE
#define TEST_ITERATIONS     3000
#else
#define TEST_ITERATIONS     5000
//#define TEST_PING_PONGS     50000 // + 450000 /* uncomment this to play even more ping pong */
#endif

int
main(int argc, char ** argv)
{
    int                     fd;
    unsigned                instance = 0;                /* e.g. master=0, slaves=1, 2, 3, etc */
    char                  * unique_memmap_path_and_file; /* e.g. /tmp/test-sxe-mmap-pid-1234.bin */
    char                    unique_memmap_path_and_file_master_buffer[PATH_MAX];
    unsigned                unique_memmap_path_and_file_master_buffer_used;
    SXE_MMAP                memmap;
    volatile unsigned     * shared;
    SXE_SPINLOCK          * shared_spinlock;
    intptr_t                hell_spawn[TEST_MMAP_INSTANCES];
    unsigned                i;
    unsigned                total;
    unsigned                count_hi = 0;
    double                  start_time;
#ifdef TEST_PING_PONGS
    double                  time_before;
    double                  time_after;
#endif

    sxe_log_decrease_level(SXE_LOG_LEVEL_WARNING); /* A lot of stuff going on here, don't want logging to slow us down */

    if (argc > 1) {
        instance                    = atoi(argv[1]);
        unique_memmap_path_and_file =      argv[2] ;

        SXEL12("Instance %2u unique memmap path and file: %s", instance, unique_memmap_path_and_file);
        sxe_mmap_open(&memmap, unique_memmap_path_and_file);
        shared = SXE_MMAP_ADDR(&memmap);
        SXEL11("Instance %2u about to set shared memory", instance);
        shared[instance] = instance;
        SXED10(SXE_CAST(char *, SXE_MMAP_ADDR(&memmap)), sizeof(long) * (TEST_MMAP_INSTANCES + 2));
        SXEL11("Instance %2u just     set shared memory", instance);
        SXEA11(shared[instance] == instance, "WTF! Thought I wrote %u", instance);

        shared_spinlock = SXE_CAST(SXE_SPINLOCK *, shared + 1024);

        InterlockedExchangeAdd(SXE_CAST(long *, &shared[0]), 1);
        start_time = sxe_get_time_in_seconds();

        while (shared[0] != TEST_MMAP_INSTANCES) {
            SXEA10(sxe_get_time_in_seconds() < TEST_WAIT + start_time, "");
            usleep(10000);
        }

        SXEL11("Instance %2u ready to rumble", instance);
        start_time = sxe_get_time_in_seconds();

        for (i = 0; i < TEST_ITERATIONS; i++) {
            SXEA11(sxe_get_time_in_seconds() < start_time + TEST_WAIT, "Unexpected timeout i=%u... is the hardware too slow?", i);
            SXEA11(sxe_spinlock_take(&shared_spinlock[0]) == SXE_SPINLOCK_STATUS_TAKEN,
                                     "Instance %2u failed to take lock", instance);
            shared_spinlock[1           ].lock++;
            shared_spinlock[1 + instance].lock++;
            sxe_spinlock_give(&shared_spinlock[0]);
        }

        start_time = sxe_get_time_in_seconds();

        while (shared[0] != 0xDEADBABE) {
            SXEA10(sxe_get_time_in_seconds() < start_time + TEST_WAIT, "Unexpected timeout... is the hardware too slow?");
            usleep(10000);
        }

#if defined TEST_PING_PONGS
        /* Instance 1 will stick around to play some ping pong.
         */
        if (instance == 1) {
            for (i = 0; i < TEST_PING_PONGS; ) {
                unsigned flag;

                SXEA10(sxe_spinlock_take(&shared_spinlock[0]) != SXE_SPINLOCK_STATUS_NOT_TAKEN, "Child failed to take lock");
                flag = shared[1];

                if (flag == 0) {
                    shared[1] = 1;
                    i++;
                }

                sxe_spinlock_give(&shared_spinlock[0]);
                sxe_get_time_in_seconds(); /* causes child to delay very slightly */
            }
        }
#endif

        sxe_mmap_close(&memmap);
        SXEL12("Instance %2u exiting // count_hi %u", instance, count_hi);
        return 0;
    }

    plan_tests(3);

    sxe_test_get_temp_file_name("test-sxe-mmap-pool", unique_memmap_path_and_file_master_buffer, sizeof(unique_memmap_path_and_file_master_buffer), &unique_memmap_path_and_file_master_buffer_used);
    unique_memmap_path_and_file = &unique_memmap_path_and_file_master_buffer[0];
    SXEL12("Instance %2d unique memmap path and file: %s", instance, unique_memmap_path_and_file);

    /* TODO: make creating the file faster on Windows! */

    SXEL11("Instance %2d creating memory mapped file contents", instance);
    SXEA12((fd = open(unique_memmap_path_and_file, O_CREAT | O_TRUNC | O_WRONLY, 0666)) >= 0, "Failed to create file '%s': %s"            , unique_memmap_path_and_file, strerror(errno));
    SXEA12(ftruncate(fd, TEST_FILE_SIZE)                                                >= 0, "Failed to extend the file to %lu bytes: %s", TEST_FILE_SIZE, strerror(errno));
    close(fd);

    SXEL11("Instance %2d memory mapping file", instance);
    sxe_mmap_open(&memmap, unique_memmap_path_and_file);
    shared = SXE_MMAP_ADDR(&memmap);

    shared_spinlock = SXE_CAST(SXE_SPINLOCK *, shared + 1024);
    sxe_spinlock_construct(&shared_spinlock[0]);
    sxe_spinlock_construct(&shared_spinlock[1]);
    shared[0] = 0;

    SXEL11("Instance %2d spawning slaves", instance);
    for (instance = 1; instance <= TEST_MMAP_INSTANCES; instance++) {
        char buffer[12];

        SXEL12("Launching %d of %d", instance, TEST_MMAP_INSTANCES);
        snprintf(buffer, sizeof(buffer), "%u", instance);
        hell_spawn[instance - 1] = spawnl(P_NOWAIT, argv[0], argv[0], buffer, unique_memmap_path_and_file, NULL);
        SXEA15(hell_spawn[instance - 1] != -1, "Failed to spawn (%d of %d) '%s %s': %s", instance, TEST_MMAP_INSTANCES, argv[0], buffer, strerror(errno));
    }

    i = 0;
    start_time = sxe_get_time_in_seconds();

    while (i < TEST_MMAP_INSTANCES) {
        SXEA10((TEST_WAIT + start_time ) > sxe_get_time_in_seconds(), "Unexpected timeout... is the hardware too slow?");
        usleep(10000);

        for (i = 0, instance = 1; instance <= TEST_MMAP_INSTANCES; instance++) {
            i += shared[instance] == instance ? 1 : 0;
        }
    }

    is(i, TEST_MMAP_INSTANCES, "All %u children have set their instance numbers", TEST_MMAP_INSTANCES);
    start_time = sxe_get_time_in_seconds();

    while (shared_spinlock[1].lock < (TEST_MMAP_INSTANCES * TEST_ITERATIONS)) {
        SXEA10((TEST_WAIT + start_time ) > sxe_get_time_in_seconds(), "Unexpected timeout... is the hardware too slow?");
        usleep(10000);
    }

    total = 0;

    for (instance = 1; instance <= TEST_MMAP_INSTANCES; instance++) {
        total += shared_spinlock[1 + instance].lock;
        SXEL12("Instance %2u incremented %d times", instance, shared_spinlock[1 + instance].lock);
    }

    is(total                  , (TEST_MMAP_INSTANCES * TEST_ITERATIONS), "Total of counts is as expected");
    is(shared_spinlock[1].lock, (TEST_MMAP_INSTANCES * TEST_ITERATIONS), "Shared count    is as expected");
    shared[0] = 0xDEADBABE;

#if defined TEST_PING_PONGS
    /* Play some ping pong; Instance 1 will go first.
     */
    time_before = sxe_get_time_in_seconds();

    for (i = 0; i < TEST_PING_PONGS; ) {
        unsigned flag;

        SXEA10(sxe_spinlock_take(&shared_spinlock[0]) != SXE_SPINLOCK_STATUS_NOT_TAKEN, "Parent failed to take lock");
        flag = shared[1];

        if (flag == 1) {
            shared[1] = 0;
            i++;
        }

        sxe_spinlock_give(&shared_spinlock[0]);
    }

    time_after = sxe_get_time_in_seconds();
    SXEL13("Just switched back and forth %u times in %f seconds, or %f times/second", TEST_PING_PONGS,
           (time_after - time_before), TEST_PING_PONGS / (time_after - time_before));
#endif

    start_time = sxe_get_time_in_seconds();

    for (instance = 0; instance < TEST_MMAP_INSTANCES; instance++) {
        SXEA10(TEST_WAIT + start_time > sxe_get_time_in_seconds(),                    "Unexpected timeout... is the hardware too slow?");
        SXEA12(cwait(NULL, hell_spawn[instance], WAIT_CHILD) == hell_spawn[instance], "Unexpected return from cwait for process 0x%08x: %s",
               hell_spawn[instance], strerror(errno));
    }

    sxe_mmap_close(&memmap);
    SXEL12("Instance %02d unlinking: %s", instance, unique_memmap_path_and_file);
    unlink(unique_memmap_path_and_file);
    SXEL11("Instance %02d exiting // master", instance);
    return exit_status();
}
