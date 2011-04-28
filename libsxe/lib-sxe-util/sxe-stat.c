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

#include "sxe-util.h"

SXE_STAT *
sxe_stat(SXE_STAT * status, const char * file)
{
    SXEE83("%s(status=%p,file=%s", __func__, status, file);

    if (stat(file, status) < 0) {
        SXEL43("%s: warning: can't stat file '%s': %s", __func__, file, strerror(errno));
        status = NULL;
    }

    SXER81("return status=%p // 0 on error", status);
    return status;
}

time_t
sxe_stat_get_time_modification(SXE_STAT * status)
{
    return status != NULL ? status->st_mtime : 0;
}

off_t
sxe_stat_get_file_size(SXE_STAT * status)
{
    return status != NULL ? status->st_size : 0;
}
