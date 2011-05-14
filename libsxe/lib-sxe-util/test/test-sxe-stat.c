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
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sxe-log.h"
//#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

int
main(void)
{
    SXE_STAT status;
    FILE   * file_ptr;
    time_t   current_time;
    char     file_path[PATH_MAX] = "test-sxe-stat-file";
//    unsigned file_path_length;

    plan_tests(7);
    time(&current_time);
    current_time -= 5;      /* Hack for now to allow file system clock to lag by up to 5 seconds: ugh! */

//    sxe_test_get_temp_file_name("test-sxe-stat", file_path, sizeof(file_path), &file_path_length);
//    SXEA11( sizeof(file_path) >= sizeof(file_path), "Temp file name is too big (%u characters)",  sizeof(file_path));
//    file_path[file_path_length] = '\0';

    is(sxe_stat(&status, file_path),              NULL,         "After removal, correctly can't stat '%s'", file_path);
    is(sxe_stat_get_time_modification(NULL),      0,            "Modification time of NULL SXE_STAT is 0");
    SXEA12((file_ptr = fopen(file_path, "w"))  != NULL,         "Can't create '%s': %s", file_path, strerror(errno));
    fclose(file_ptr);
    is(sxe_stat(&status, file_path),              &status,      "After creation, able to stat '%s'", file_path);
    ok(sxe_stat_get_time_modification(&status) >= current_time, "Modification time of file is %lu (program time %lu)",
                                                                (unsigned long)sxe_stat_get_time_modification(&status), (unsigned long)current_time);
    is(sxe_stat_get_file_size(&status), 0,                      "It's an empty file, 0 bytes");
    SXEA12((file_ptr = fopen(file_path, "w"))  != NULL,         "Can't create '%s': %s", file_path, strerror(errno));
    fwrite("foobar", 1, 6, file_ptr);
    fclose(file_ptr);
    is(sxe_stat(&status, file_path),              &status,      "After write, able to stat '%s' again", file_path);
    is(sxe_stat_get_file_size(&status), 6,                      "It's not empty any more, file is 6 bytes now");

    unlink(file_path);
    return exit_status();
}


