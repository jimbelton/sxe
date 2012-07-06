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

#include <stdbool.h>

#include "ev.h"
#include "sxe-list.h"
#include "sxe-log.h"
#include "sxe-pool-private.h"

static bool     sxe_pool_defer_list_initialized = false;
static SXE_LIST sxe_pool_defer_list;

static void
sxe_pool_defer_notify(EV_P)
{
    SXE_LIST_WALKER  walker;
    SXE_POOL_DEFER * defer;
    SXE_POOL_IMPL  * pool;
    unsigned         id;
    unsigned         last_id = SXE_POOL_NO_INDEX;

    SXEE6("(loop=%p)", loop);
    sxe_list_walker_construct(&walker, &sxe_pool_defer_list);

    SXEL7("For each pool with defer capability enabled check for the deferred state");
    while ((defer = sxe_list_walker_step(&walker)) != NULL) {
        if (defer->loop != loop) {
            SXEL7("Defer %p has loop %p, our loop is %p: skipping", defer, defer->loop, loop);
            continue;    /* Coverage exclusion: multiple ev_loop support: not used yet */
        }

        pool = SXE_POOL_ARRAY_TO_IMPL(defer->array);

        SXEL7("Checking pool name @ %p: %s", defer->array, sxe_pool_get_name(defer->array));
        while ((id = sxe_pool_get_oldest_element_index(defer->array, defer->state)) != SXE_POOL_NO_INDEX) {
            if (id == last_id) {
                SXEL6("Object %u not removed from pool %s deferred state %u by deferred event handler %p",
                      id, sxe_pool_get_name(defer->array), defer->state, SXE_POOL_NODES(pool)[id].event);
                break;
            }

            SXEL6("calling callback: ->event");
            (*SXE_POOL_NODES(pool)[id].event)(defer->array, id);
            last_id = id;
        }
    }

    SXER6("return");
}

/**
 * Is event deferred for an object in a pool?
 *
 * @param array Pointer to the pool's array
 * @param id    Index of the object to defer the event on
 */
bool
sxe_pool_deferred(void * array, unsigned id)
{
    bool            result;
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);

    SXEE6("(pool=%s, id=%u)", pool->name, id);
    SXEA1(pool->defer != NULL, "%s(): Pool %s is not allowed to defer events",         __func__,     pool->name);
    SXEA1(id < pool->number,   "%s(): Index %u is too big for pool %s (max index=%u)", __func__, id, pool->name, pool->number);
    result = pool->defer->state == sxe_pool_index_to_state(array, id);
    SXER6("return %s", result ? "true" : "false");
    return result;
}

/**
 * Defer an event for an object in a pool
 *
 * @param array Pointer to the pool's array
 * @param id    Index of the object to defer the event on
 * @param event Function to call to handle the event
 *
 * @note To make deferred events thread safe, use a locked pool
 * @note Any parameters to the event should be stored in the object
 */
void
sxe_pool_defer_event(void * array, unsigned id, void (*event)(void * array, unsigned id))
{
    SXE_POOL_IMPL * pool  = SXE_POOL_ARRAY_TO_IMPL(array);

    SXEE6("(pool=%s, id=%u, event=%p)", pool->name, id, event);
    SXEA1(pool->defer != NULL, "%s(): Pool %s is not allowed to defer events",         __func__,     pool->name);
    SXEA1(id < pool->number,   "%s(): Index %u is too big for pool %s (max index=%u)", __func__, id, pool->name, pool->number);
    SXE_POOL_NODES(pool)[id].event = event;
    sxe_pool_set_indexed_element_state(array, id, sxe_pool_index_to_state(array, id), pool->defer->state);
    SXER6("return");
}

/**
 * Allow deferring events on a pool
 *
 * @param array     Pointer to the pool's array
 * @param defer     Pointer to the defer object, which must persist for the lifetime of the pool
 * @param loop      Pointer to the event loop to defer events to
 * @param state     State of the pool that will be notified when deferred events are processed
 *
 * @note To make deferred events thread safe, use a locked pool
 */
void
sxe_pool_defer_allow(void * array, SXE_POOL_DEFER * defer, struct ev_loop * loop, unsigned state)
{
    SXE_POOL_IMPL  * pool = SXE_POOL_ARRAY_TO_IMPL(array);
    SXE_LIST_WALKER  walker;
    SXE_POOL_DEFER * defer_old;

    SXEE6("(pool=%s, defer=%p, loop=%p, state=%s)", pool->name, defer, loop, (*pool->state_to_string)(state));
    SXEA1(pool->defer == NULL, "Pool %s is already allowed to defer events", pool->name);

    if (!sxe_pool_defer_list_initialized) {
        SXE_LIST_CONSTRUCT(&sxe_pool_defer_list, 0, SXE_POOL_DEFER, node);
        sxe_pool_defer_list_initialized = true;
    }

    sxe_list_walker_construct(&walker, &sxe_pool_defer_list);

    while ((defer_old = sxe_list_walker_step(&walker)) != NULL) {
        if (defer_old->loop == loop) {
            break;
        }
    }

    if (defer_old == NULL) {    /* Never seen this loop before */
        ev_set_loop_release_cb(loop, &sxe_pool_defer_notify, NULL);    /* NULL for acquire callback */
    }

    pool->defer  = defer;
    defer->array = array;
    defer->loop  = loop;
    defer->state = state;
    sxe_list_push(&sxe_pool_defer_list, defer);
    SXER6("return");
}

/**
 * Allow unit test to reset the state
 */
void
sxe_pool_defer_private_fini(void)
{
    sxe_pool_defer_list_initialized = false;
}

/* vim: set expandtab: */
