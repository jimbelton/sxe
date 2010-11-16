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

#ifndef __SXE_POOL_H__
#define __SXE_POOL_H__

#include "sxe-list.h"
#include "sxe-time.h"
#include "sxe-util.h"

#define SXE_POOL_LOCK_TAKEN            0        /* Only used by sxe_pool_set_indexed_element_state() */
#define SXE_POOL_NO_INDEX             -1U       /* All elements are in use or state is empty         */
#define SXE_POOL_INCORRECT_STATE      -2U       /* Element is not in the expected state              */
#define SXE_POOL_LOCK_NOT_TAKEN       -3U       /* We gave up trying to acquire lock                 */
#define SXE_POOL_NAME_MAXIMUM_LENGTH   31

#define SXE_POOL_OPTION_UNLOCKED       0
#define SXE_POOL_OPTION_LOCKED         SXE_BIT_OPTION(0)
#define SXE_POOL_OPTION_TIMED          SXE_BIT_OPTION(1)

typedef void (*SXE_POOL_EVENT_TIMEOUT)(    void * array, unsigned array_index, void * caller_info);

typedef struct SXE_POOL_WALKER {
    SXE_LIST_WALKER list_walker;
    void       * pool;
    unsigned     state;
    union {
        SXE_TIME time;
        uint64_t count;
    } last;
} SXE_POOL_WALKER;

#include "lib-sxe-pool-proto.h"
#endif
