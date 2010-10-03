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

#include "sxe-pool-private.h"

/**
 * Construct a pool state walker (AKA iterator)
 *
 * @param walker Pointer to the walker
 * @param array  Pointer to the pool array
 * @param state  State to walk
 *
 * @exception If the pool is both locked and timed, it cannot be walked safely
 */
void
sxe_pool_walker_construct(SXE_POOL_WALKER * walker, void * array, unsigned state)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);

    SXEE83("sxe_pool_walker_construct(walker=%p,pool->name=%s,state=%u)", walker, pool->name, state);
    SXEA11(!((pool->options & SXE_POOL_OPTION_LOCKED) && (pool->options & SXE_POOL_OPTION_TIMED)),
           "sxe_pool_walker_construct: Can't walk thread safe timed pool %s safely", pool->name);
    sxe_list_walker_construct(&walker->list_walker, &SXE_POOL_QUEUE(pool)[state]);
    walker->pool  = pool;
    walker->state = state;

    if (pool->options & SXE_POOL_OPTION_TIMED) {
        walker->last.time  = 0.0;
    }
    else {
        walker->last.count = 0;
    }

    SXER80("return");
}

/**
 * Step to the next object in a pool state
 *
 * @param walker Pointer to the pool state walker
 *
 * @return Index of the next object or SXE_POOL_NO_INDEX if the end of the state queue has been reached.
 *
 * @note Thread safety is implemented by verifying that the last node stepped to is still in the same state queue. If it is not,
 *       the state queue is rewalked to find a node with a time or count greater than or equal to the time that the last stepped
 *       to node had when it was stepped to.
 */
unsigned
sxe_pool_walker_step(SXE_POOL_WALKER * walker)
{
    SXE_POOL_NODE * node;
    SXE_POOL_IMPL * pool = walker->pool;
    unsigned        result;

    SXEE81("sxe_pool_walker_step(walker=%p)", walker);

    if ((result = sxe_pool_lock(pool)) == SXE_POOL_LOCK_NOT_TAKEN) {
        goto SXE_ERROR_OUT;
    }

    /* If not at the head of the state queue and the current object has been moved to another state.
     */
    if (((node = sxe_list_walker_find(&walker->list_walker)) != NULL)
     && (SXE_LIST_NODE_GET_ID(&node->list_node) != walker->state))
     /* TODO: Check for touching */
    {
        SXEL83("sxe_pool_walker_step: node %u moved from state %u to state %u by another thread", node - SXE_POOL_NODES(pool),
               walker->state, SXE_LIST_NODE_GET_ID(&node->list_node));

        /* If there is a previous object and it has not been moved, get the new next one.
         */
        if (((node = sxe_list_walker_back(&walker->list_walker)) != NULL)
         && (SXE_LIST_NODE_GET_ID(&node->list_node) == walker->state))
         /* TODO: Check for touching */
        {
            node = sxe_list_walker_step(&walker->list_walker);
        }
        else {
            sxe_list_walker_construct(&walker->list_walker, &SXE_POOL_QUEUE(pool)[walker->state]);

            while ((node = sxe_list_walker_step(&walker->list_walker)) != NULL) {
                if (pool->options & SXE_POOL_OPTION_TIMED) {
                    if (node->last.time >= walker->last.time) {    /* Coverage Exclusion: TODO refactor SXE_POOL_TIME */
                        break;                                     /* Coverage Exclusion: TODO refactor SXE_POOL_TIME */
                    }
                }
                else {
                    if (node->last.count >= walker->last.count) {
                        break;
                    }
                }
            }
        }
    }
    else {
        node = sxe_list_walker_step(&walker->list_walker);
    }

    result = SXE_POOL_NO_INDEX;

    if (node != NULL) {
        result = node - SXE_POOL_NODES(pool);

        if (pool->options & SXE_POOL_OPTION_TIMED) {
            walker->last.time  = node->last.time;
        }
        else {
            walker->last.count = node->last.count;
        }
    }

    sxe_pool_unlock(pool);

SXE_ERROR_OUT:
    SXER81("return %s", sxe_pool_return_to_string(result));
    return result;
}
