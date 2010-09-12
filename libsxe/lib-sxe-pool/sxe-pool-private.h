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

/**
 * This file includes details of the pool implementation that are shared between its modules only.
 */

#include "sxe-pool.h"
#include "sxe-spinlock.h"
#include "sxe-util.h"

#define SXE_POOL_ARRAY_TO_IMPL(array) ((SXE_POOL_IMPL *)(array) - 1)
#define SXE_POOL_IMPL_TO_ARRAY(impl)  ((void *)((SXE_POOL_IMPL *)(impl) + 1))
#define SXE_POOL_NODES(impl)          SXE_PTR_FIX(impl, SXE_POOL_NODE *, (impl)->nodes)
#define SXE_POOL_QUEUE(impl)          SXE_PTR_FIX(impl, SXE_LIST *,      (impl)->queue)

typedef struct SXE_POOL_NODE {
    SXE_LIST_NODE list_node;
    double        last_time;
} SXE_POOL_NODE;

typedef struct SXE_POOL_IMPL {
    SXE_SPINLOCK           spinlock;
    char                   name[SXE_POOL_NAME_MAXIMUM_LENGTH + 1];
    unsigned               flags;
    unsigned               number;
    size_t                 size;
    unsigned               states;
    SXE_POOL_NODE        * nodes;
    SXE_LIST             * queue;
    SXE_POOL_EVENT_TIMEOUT event_timeout;
    void                 * caller_info;
    double               * state_timeouts;
    SXE_LIST_NODE          timeout_node;
} SXE_POOL_IMPL;

static inline SXE_POOL_NODE *
sxe_pool_node_from_list_node(SXE_LIST_NODE * list_node)
{
    return (SXE_POOL_NODE *)((char *)list_node - offsetof(SXE_POOL_NODE, list_node));
}
