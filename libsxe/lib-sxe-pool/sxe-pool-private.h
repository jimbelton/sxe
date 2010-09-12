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
#define SXE_POOL_ACQUIRE_LOCK(pool)   sxe_spinlock_take(&pool->spinlock)
#define SXE_POOL_FREE_LOCK(pool)      sxe_spinlock_give(&pool->spinlock)

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
sxe_pool_index_from_list_node(SXE_LIST_NODE * list_node)
{
    return (SXE_POOL_NODE *)((char *)list_node - offsetof(SXE_POOL_NODE, list_node));
}

static inline unsigned
sxe_pool_lock(SXE_POOL_IMPL * pool)
{
    unsigned result = SXE_POOL_LOCK_TAKEN;

    if (!(pool->flags & SXE_POOL_LOCKS_ENABLED)) {    /* Not locked - take it and go! */
        return SXE_POOL_LOCK_TAKEN;
    }

    SXEE81("sxe_pool_lock(pool->name=%s)", pool->name);

    if (SXE_POOL_ACQUIRE_LOCK(pool) != SXE_SPINLOCK_STATUS_TAKEN) {
        result = SXE_POOL_LOCK_NOT_TAKEN;
    }

SXE_EARLY_OR_ERROR_OUT:
    SXER82("return %u // %s", result,
        result == SXE_POOL_LOCK_NOT_TAKEN ? "SXE_POOL_LOCK_NOT_TAKEN" :
        result == SXE_POOL_LOCK_TAKEN       ? "SXE_POOL_LOCK_TAKEN"       : "unexpected return value!" );
    return result;
}

static inline void
sxe_pool_unlock(SXE_POOL_IMPL * pool)
{
    if (!(pool->flags & SXE_POOL_LOCKS_ENABLED)) {    /* Not locked - GTFO! */
        return;
    }

    SXEE81("sxe_pool_unlock(pool->name=%s)", pool->name);
    SXE_POOL_FREE_LOCK(pool);
    SXER80("return");
}
