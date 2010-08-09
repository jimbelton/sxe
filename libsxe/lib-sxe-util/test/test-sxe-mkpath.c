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
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "sxe-log.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_DIR  "test-directory"
#define TEST_PATH TEST_DIR "/test-subdirectory"
int
main(void)
{
    struct stat status;

    plan_tests(2);

    SXEA10((system("rm -rf " TEST_DIR) == 0) || (stat(TEST_DIR, &status) < 0), "Couldn't remove " TEST_DIR);
    is(sxe_mkpath(TEST_PATH), SXE_RETURN_OK, "sxe_mkpath succeeded");
    ok(stat(TEST_PATH, &status) >= 0,        "sxe_mkpath created " TEST_PATH);
    return exit_status();
}


