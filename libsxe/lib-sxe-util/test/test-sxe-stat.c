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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sxe-log.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_FILE "test-sxe-stat-file"

int
main(void)
{
    SXE_STAT status;
    FILE   * file_ptr;
    time_t   current_time;

    plan_tests(7);
    time(&current_time);
    unlink(TEST_FILE);

    is(sxe_stat(&status, TEST_FILE),              NULL,         "After removal, correctly can't stat '" TEST_FILE "'");
    is(sxe_stat_get_time_modification(NULL),      0,            "Modification time of NULL SXE_STAT is 0");
    SXEA11((file_ptr = fopen(TEST_FILE, "w"))  != NULL,         "Can't create '" TEST_FILE "': %s", strerror(errno));
    fclose(file_ptr);
    is(sxe_stat(&status, TEST_FILE),              &status,      "After creation, able to stat '" TEST_FILE "'");
    ok(sxe_stat_get_time_modification(&status) >= current_time, "Modification time of file is %lu (program time %lu)",
                                                                (unsigned long)sxe_stat_get_time_modification(&status), (unsigned long)current_time);
    is(sxe_stat_get_file_size(&status), 0,                      "It's an empty file, 0 bytes");
    SXEA11((file_ptr = fopen(TEST_FILE, "w"))  != NULL,         "Can't create '" TEST_FILE "': %s", strerror(errno));
    fwrite("foobar", 1, 6, file_ptr);
    fclose(file_ptr);
    is(sxe_stat(&status, TEST_FILE),              &status,      "After write, able to stat '" TEST_FILE "' again");
    is(sxe_stat_get_file_size(&status), 6,                      "It's not empty any more, file is 6 bytes now");

    unlink(TEST_FILE);
    return exit_status();
}


