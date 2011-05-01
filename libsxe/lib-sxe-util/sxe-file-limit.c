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

#ifdef WIN32
#include <stdio.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "sxe-util.h"

SXE_RETURN
sxe_set_file_limit(const unsigned limit)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;

#ifndef _WIN32
    struct rlimit rt;
    rt.rlim_max = limit;
    rt.rlim_cur = limit;
#endif

    if
#ifdef _WIN32
       (_setmaxstdio(limit) == -1)
#else
       (setrlimit(RLIMIT_NOFILE, &rt) == -1)
#endif
    {
        SXEL91("Failed to set file limit to '%u'", limit);
        goto SXE_ERROR_OUT; /* COVERAGE EXCLUSION - TODO: WIN32 COVERAGE */
    }

    result = SXE_RETURN_OK;

SXE_ERROR_OUT:
    return result;
}

