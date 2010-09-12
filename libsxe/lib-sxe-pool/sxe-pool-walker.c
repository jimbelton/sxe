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

/*
#include "sxe-list.h"
#include "sxe-log.h"
#include "sxe-spinlock.h"
*/
#include "sxe-pool-private.h"
/*
#include "sxe-util.h"
#include "mock.h"
*/

/**
 * Construct a pool state walker (AKA iterator)
 *
 * @param walker Pointer to the walker
 * @param array  Pointer to the pool array
 * @param state  State to walk
 */
void
sxe_pool_walker_construct(SXE_POOL_WALKER * walker, void * array, unsigned state)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);

    SXEE83("sxe_pool_walker_construct(walker=%p,pool->name=%s,state=%u)", walker, pool->name, state);
    sxe_list_walker_construct(&walker->list_walker, &SXE_POOL_QUEUE(pool)[state]);
    walker->pool = pool;
    SXER80("return");
}

/**
 * Step to the next object in a pool state
 *
 * @param walker Pointer to the pool state walker
 *
 * @return Index of the next object, SXE_POOL_NO_INDEX if the end of the state queue has been reached,
 *         or SXE_POOL_LOCK_NOT_TAKEN if this is a locked pool and the lock could not be taken
 */
unsigned
sxe_pool_walker_step(SXE_POOL_WALKER * walker)
{
    SXE_LIST_NODE * node;
    unsigned        result;

    SXEE81("sxe_pool_walker_step(walker=%p)", walker);

    if ((result = sxe_pool_lock(walker->pool)) == SXE_POOL_LOCK_NOT_TAKEN) {
        goto SXE_ERROR_OUT;
    }

    if ((node = sxe_list_walker_step(&walker->list_walker)) == NULL) {
        result = SXE_POOL_NO_INDEX;
        goto SXE_EARLY_OUT;
    }

    result = sxe_pool_index_from_list_node(node) - SXE_POOL_NODES((SXE_POOL_IMPL *)walker->pool);

SXE_EARLY_OUT:
    sxe_pool_unlock(walker->pool);

SXE_ERROR_OUT:
    SXER81("return result=%u", result);
    return result;
}

/**
 * Visit the objects in pool state until a visit returns a non-zero value or every object has been visited
 *
 * @param array        Pointer to the pool array
 * @param state        State to walk
 * @param visit        Function to call on each object
 * @param user_data    Arbitrary value to pass to visit function (e.g value to search for)
 *
 * @return NULL or non-NULL value returned from visit (indicates that the walk was stopped early - e.g. pointer to object found)
 *
 * @note If a lock failure occurs, NULL will be returned and the walk will be stopped early. If you need to be able to detected
 *       this, use a walker object and step it yourself.
 */

void *
sxe_pool_walk_state(void * array, unsigned state, void * (*visit)(void * object, void * user_data), void * user_data)
{
    SXE_POOL_IMPL * pool   = SXE_POOL_ARRAY_TO_IMPL(array);
    void          * result = NULL;
    SXE_POOL_WALKER walker;
    unsigned        id;

    SXEE84("sxe_pool_walk_state(pool->name=%s,state=%u,visit=%p,user_data=%p)", pool->name, state, visit, user_data);
    sxe_pool_walker_construct(&walker, array, state);

    for (id = sxe_pool_walker_step(&walker); id != SXE_POOL_NO_INDEX && id != SXE_POOL_LOCK_NOT_TAKEN; id = sxe_pool_walker_step(&walker)) {
        if ((result = (*visit)((char *)SXE_POOL_IMPL_TO_ARRAY(pool) + pool->size * id, user_data)) != NULL) {
            break;
        }
    }

    SXER81("return result=%p", result);
    return result;
}
