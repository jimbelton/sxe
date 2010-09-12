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

#include "sxe-list.h"
#include "sxe-log.h"
#include "sxe-spinlock.h"
#include "sxe-util.h"
#include "mock.h"

#include "sxe-pool-private.h"

#define SXE_POOL_ASSERT_LOCKS_DISABLED(pool) SXEA11(pool->flags == SXE_POOL_LOCKS_DISABLED,                            \
                                                    "Function called on pool %s which has locks disabled", pool->name)

static SXE_LIST sxe_pool_timeout_list;
static unsigned sxe_pool_timeout_count = 0;

static inline double
sxe_pool_local_get_time_in_seconds(void)
{
    struct timeval tv;

    SXEA11(gettimeofday(&tv, NULL) >= 0, "gettimeofday failed: (%d)", errno);
    return (double)tv.tv_sec + 1.e-6 * (double)tv.tv_usec;
}

const char *
sxe_pool_get_name(void * array)
{
    /* No diagnostics, thanks, since this is only used for diagnostics */

    return SXE_POOL_ARRAY_TO_IMPL(array)->name;
}

unsigned
sxe_pool_get_number_in_state(void * array, unsigned state)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);
    unsigned        count;

    SXEE82("sxe_pool_get_number_in_state(array=%p,state=%u)", array, state);
    count = SXE_LIST_GET_LENGTH(&SXE_POOL_QUEUE(pool)[state]);
    SXER81("return %u", count);
    return count;
}

unsigned
sxe_pool_index_to_state(void * array, unsigned id)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);
    unsigned        state;

    SXEE82("sxe_pool_index_to_state(array=%p,id=%u)", array, id);
    state = SXE_LIST_NODE_GET_ID(&SXE_POOL_NODES(pool)[id].list_node);
    SXER81("return %u", state);
    return state;
}

/**
 * Determine the size in bytes of a new pool of <number> objects of size <size> with <states> states
 *
 * @return The size of the pool
 */
size_t
sxe_pool_size(unsigned number, unsigned size, unsigned states)
{
    size_t pool_size;

    SXEE83("sxe_pool_size(number=%u,size=%u,states=%u)", number, size, states);
    pool_size =      1 * sizeof(SXE_POOL_IMPL)    /* pool impl structure */
              + number * size                     /* user objects        */
              + states * sizeof(SXE_LIST)         /* state queue heads   */
              + number * sizeof(SXE_POOL_NODE);   /* internal nodes      */

    SXER81("return pool_size=%u", pool_size);
    return pool_size;
}

/**
 * Construct a new pool of <number> objects of size <size> with <states> states
 *
 * @return A pointer to the array of objects
 *
 * @note   The base pointer must point at a region of memory big enough to hold the pool size (see sxe_pool_size())
 */
void *
sxe_pool_construct(void * base, const char * name, unsigned number, unsigned size, unsigned states, unsigned flags)
{
    SXE_POOL_IMPL * pool;
    unsigned        i;
    unsigned        sxe_log_level_saved;
    double          current_time;

    SXEE86("sxe_pool_construct(base=%p,name=%s,number=%u,size=%u,states=%u,flags=%u)", base, name, number, size, states, flags);

    pool         = (SXE_POOL_IMPL *)base;
    pool->queue  = (SXE_LIST      *)(sizeof(SXE_POOL_IMPL) + number * size);
    pool->nodes  = (SXE_POOL_NODE *)(sizeof(SXE_POOL_IMPL) + number * size + states * sizeof(SXE_LIST));
    pool->number = number;
    pool->size   = size;
    pool->states = states;
    pool->flags  = flags;

    if (flags & SXE_POOL_LOCKS_ENABLED) {
        sxe_spinlock_construct(&pool->spinlock);
    }

    strncpy(pool->name, name, sizeof(pool->name));
    pool->name[sizeof(pool->name) - 1] = '\0';
    pool->event_timeout = NULL;

#ifdef DEBUG
    memset(SXE_POOL_QUEUE(pool), 0xBE, sizeof(SXE_POOL_NODE) * states);
#endif

    for (i = states; i-- > 0; ) {
        SXE_LIST_CONSTRUCT(&SXE_POOL_QUEUE(pool)[i], i, SXE_POOL_NODE, list_node);
    }

    SXEL80("Construct the free list");

    sxe_log_level_saved = sxe_log_level; /* Shut up logging on every node here - gross */
    sxe_log_level = 5;
    current_time  = sxe_pool_local_get_time_in_seconds();

    for (i = number; i-- > 0; ) {
        SXE_POOL_NODES(pool)[i].last_time = current_time;
        sxe_list_push(&SXE_POOL_QUEUE(pool)[0], &SXE_POOL_NODES(pool)[i].list_node);
    }

    sxe_log_level = sxe_log_level_saved;
    SXER84("return array=%p // pool=%p, pool->nodes=%p, pool->name=%s", pool + 1, pool, SXE_POOL_NODES(pool), pool->name);
    return pool + 1;
}

/**
 * Get the a pointer to a pool from a base pointer; this can be used to get at the array in a memory mapped pool
 *
 * @return A pointer to the array of objects
 */
void *
sxe_pool_from_base(void * base)
{
    void * array;

    SXEE81("sxe_pool_from_base(base=%p)", base);
    array = SXE_POOL_IMPL_TO_ARRAY(base);
    SXER81("return array=%p", array);
    return array;
}

/**
 * Get the base pointer from a pool array pointer; this is used (for example) by sxe-hash to get at a hash array's base pointer
 *
 * @return A pointer to the base of the pool
 */
void *
sxe_pool_to_base(void * array)
{
    void * base;

    SXEE81("sxe_pool_to_base(array=%p)", array);
    base = SXE_POOL_ARRAY_TO_IMPL(array);
    SXER81("return base=%p", base);
    return base;
}

/**
 * Allocate and construct a new pool of <number> objects of size <size> with <states> states
 *
 * @param flags = SXE_POOL_LOCKS_ENABLED or SXE_POOL_LOCKS_DISABLED
 *
 * @return A pointer to the array of objects
 */
void *
sxe_pool_new(const char * name, unsigned number, unsigned size, unsigned states, unsigned flags)
{
    void          * base;
    void          * array;
    SXE_POOL_IMPL * pool;

    SXEE85("sxe_pool_new(name=%s,number=%u,size=%u,states=%u,flags=%s)", name, number, size, states,
           flags & SXE_POOL_LOCKS_ENABLED ? "SXE_POOL_LOCKS_ENABLED" : "SXE_POOL_LOCKS_DISABLED");
    SXEL82("Allocating pool: %16s: %10u byte pool structure", name, sizeof(SXE_POOL_IMPL));
    SXEL84("Allocating pool: %16s: %10u bytes = %10u * %10u byte objects", name, size * number, number, size);
    SXEL84("Allocating pool: %16s: %10u bytes = %10u * %10u byte state queue heads",
            name, states * sizeof(SXE_LIST), states, sizeof(SXE_LIST));
    SXEL84("Allocating pool: %16s: %10u bytes = %10u * %10u byte internal nodes",
           name, sizeof(SXE_POOL_NODE)* number, number, sizeof(SXE_POOL_NODE));
    SXEA11((base = malloc(sxe_pool_size(number, size, states))) != NULL, "Error allocating SXE pool %s", name);

    array = sxe_pool_construct(base, name, number, size, states, flags);
    pool  = SXE_POOL_ARRAY_TO_IMPL(array);
    SXER84("return array=%p // pool=%p, pool->nodes=%p, pool->name=%s", array, pool, SXE_POOL_NODES(pool), pool->name);
    return array;
}

/**
 *  @note   Pools with timeouts are not currently relocatable or thread safe.
 */
void *
sxe_pool_new_with_timeouts(
    const char             * name,
    unsigned                 number,
    unsigned                 size,
    unsigned                 states,
    double                 * timeouts,
    SXE_POOL_EVENT_TIMEOUT   callback,
    void                   * caller_info)
{
    char          * array;
    SXE_POOL_IMPL * pool;
    unsigned        i;

    SXEE87("sxe_pool_new_with_timeouts(name=%s,number=%u,size=%u,states=%u,timeouts=%p,callback=%p,caller_info=%p)",
           name, number, size, states, timeouts, callback, caller_info);
    SXEA10(callback != NULL, "Internal: timeout callback must be a real address of a function");

    /* If it feels like the first time...
     */
    if (sxe_pool_timeout_count == 0) {
        SXE_LIST_CONSTRUCT(&sxe_pool_timeout_list, 0, SXE_POOL_IMPL, timeout_node);
    }

    array                = sxe_pool_new(name, number, size, states, SXE_POOL_LOCKS_DISABLED);
    pool                 = SXE_POOL_ARRAY_TO_IMPL(array);
    pool->event_timeout  = callback;
    pool->caller_info    = caller_info;
    pool->state_timeouts = malloc(states * sizeof(*timeouts));

    SXEA11(pool->state_timeouts != NULL, "Error allocating SXE pool %s; state timeout array", name);
    SXEL82("allocated %u bytes to hold %u state timeouts", states * sizeof(*timeouts), states);

    for (i = 0; i < states; i++) {
        SXEL82("state %u has timeout %f", i, timeouts[i]);
        pool->state_timeouts[i] = timeouts[i];
    }

    sxe_list_push(&sxe_pool_timeout_list, pool);
    sxe_pool_timeout_count++;

    /* TODO: need sxe_pool_construct_with_timeouts() for pools with timeouts using spinlocks */

    SXER81("return array=%p", array);
    return array;
}

static void *
sxe_pool_visit_timeouts(void * pool_void, void * user_data)
{
    SXE_POOL_IMPL * pool;
    double          time_now;
    unsigned        state;
    double          timeout_for_this_state;
    double          time_oldest_for_this_state;
    unsigned        index_oldest_for_this_state;
    unsigned        index_oldest_for_this_state_last;

    pool     = (SXE_POOL_IMPL *)pool_void;
    time_now = *(double *)user_data;
    SXEE82("sxe_pool_visit_timeouts(pool->name=%s,time_now=%.3f)", pool->name, time_now);

    for (state = 0; state < pool->states; state ++) {
        timeout_for_this_state = pool->state_timeouts[state];

        if (0.0 == timeout_for_this_state) {
            SXEL81("state %d timeout is infinite; ignoring", state);
            continue;
        }

        index_oldest_for_this_state_last = SXE_POOL_NO_INDEX;

        for (;;) {
            index_oldest_for_this_state = sxe_pool_get_oldest_element_index(SXE_POOL_IMPL_TO_ARRAY(pool), state);

            if (SXE_POOL_NO_INDEX == index_oldest_for_this_state) {
                SXEL82("state %d timeout %f has no elements", state, timeout_for_this_state);
                break;
            }

            time_oldest_for_this_state  = sxe_pool_get_oldest_element_time(SXE_POOL_IMPL_TO_ARRAY(pool), state);
            SXEA10(index_oldest_for_this_state_last != index_oldest_for_this_state,
                   "Internal: callback failed to update state on timed out pool element");

            if ((time_now - time_oldest_for_this_state) < timeout_for_this_state) {
                SXEL83("state %d timeout %f has not been reached for oldest index %u", state, timeout_for_this_state,
                       index_oldest_for_this_state);
                break;
            }

            SXEL83("state %d timeout %f has been reached for oldest index %u", state, timeout_for_this_state,
                   index_oldest_for_this_state);
            (*pool->event_timeout)(SXE_POOL_IMPL_TO_ARRAY(pool), index_oldest_for_this_state, pool->caller_info);
            index_oldest_for_this_state_last = index_oldest_for_this_state;
        }
    }

    SXER80("return NULL");
    return NULL;
}

void
sxe_pool_check_timeouts(void)
{
    double time_now;

    SXEE80("sxe_pool_check_timeouts()");
    time_now = sxe_pool_local_get_time_in_seconds();
    SXEA10(sxe_list_walk(&sxe_pool_timeout_list, sxe_pool_visit_timeouts, &time_now) == NULL, "Did not visit all timeout pools");
    SXER80("return");
}

/**
 * Internal lockless function to move a specific object from one state queue to the tail of another
 */
static void
sxe_pool_set_indexed_element_state_unlocked(void * array, unsigned id, unsigned old_state, unsigned new_state)
{
    SXE_POOL_IMPL * pool   = SXE_POOL_ARRAY_TO_IMPL(array);
    SXE_POOL_NODE * node;

    SXE_UNUSED_PARAMETER(old_state);   /* Used to verify sanity in debug build only */
    SXEE84("sxe_pool_set_indexed_element_state_unlocked(pool->name=%s,id=%u,old_state=%u,new_state=%u)",
           pool->name, id, old_state, new_state);

    node = &SXE_POOL_NODES(pool)[id];
    SXEA85(SXE_LIST_NODE_GET_ID(&node->list_node) == old_state,
           "sxe_pool_set_indexed_element_state_unlocked(pool->name=%s,id=%u,old_state=%u,new_state=%u): Object is in state %u",
           pool->name, node - SXE_POOL_NODES(pool),
           old_state, new_state, SXE_LIST_NODE_GET_ID(&node->list_node));
    sxe_list_remove(&SXE_POOL_QUEUE(pool)[old_state], node);
    node->last_time = sxe_pool_local_get_time_in_seconds();
    sxe_list_push(&SXE_POOL_QUEUE(pool)[new_state], &node->list_node);

    SXER80("return");
}

/**
 * Move a specific object from one state queue to the tail of another
 */
unsigned
sxe_pool_set_indexed_element_state(void * array, unsigned id, unsigned old_state, unsigned new_state)
{
    SXE_POOL_IMPL * pool   = SXE_POOL_ARRAY_TO_IMPL(array);
    unsigned        result = SXE_POOL_LOCK_TAKEN;

    SXE_UNUSED_PARAMETER(old_state);   /* Used to verify sanity in debug build only */
    SXEE84("sxe_pool_set_indexed_element_state(pool->name=%s,id=%u,old_state=%u,new_state=%u)",
           pool->name, id, old_state, new_state);
    SXEA83(id < pool->number, "sxe_pool_set_indexed_element_state(pool->name=%s,id=%u): Index is too big (number=%u)",
           pool->name, id, pool->number);
    SXEA83(old_state <= pool->states, "state %u is greater than maximum state %u for pool %s", old_state, pool->states, pool->name);
    SXEA83(new_state <= pool->states, "state %u is greater than maximum state %u for pool %s", new_state, pool->states, pool->name);

    if ((result = sxe_pool_lock(pool)) == SXE_POOL_LOCK_NOT_TAKEN) {
        goto SXE_ERROR_OUT;
    }

    sxe_pool_set_indexed_element_state_unlocked(array, id, old_state, new_state);
    sxe_pool_unlock(pool);

SXE_ERROR_OUT:
    SXER82("return %u // %s", result,
        result == SXE_POOL_NO_INDEX       ? "SXE_POOL_NO_INDEX"       :
        result == SXE_POOL_LOCK_NOT_TAKEN ? "SXE_POOL_LOCK_NOT_TAKEN" :
        result == SXE_POOL_LOCK_TAKEN     ? "SXE_POOL_LOCK_TAKEN"     : "unexpected return value!" );
    return result;
}

/**
 * Move an object from a the head of one state queue to the tail of another
 *
 * @return Object's index or SXE_POOL_NO_INDEX if there are no objects in the first state
 */
unsigned
sxe_pool_set_oldest_element_state(void * array, unsigned old_state, unsigned new_state)
{
    SXE_POOL_IMPL * pool  = SXE_POOL_ARRAY_TO_IMPL(array);
    SXE_POOL_NODE * node;
    unsigned        result = SXE_POOL_LOCK_TAKEN;

    SXEE84("sxe_pool_set_oldest_element_state(array=%p, old_state=%u, new_state=%u) // pool=%s", array, old_state, new_state,
           pool->name);
    SXEA83(old_state <= pool->states, "state %u is greater than maximum state %u for pool %s", old_state, pool->states,
           pool->name);

    if ((result = sxe_pool_lock(pool)) == SXE_POOL_LOCK_NOT_TAKEN) {
        goto SXE_ERROR_OUT;
    }

    if ((node = sxe_list_peek_head(&SXE_POOL_QUEUE(pool)[old_state])) == NULL) {
        SXEL82("sxe_pool_set_oldest_element_state(pool->name=%s): No objects in state %u; returning SXE_POOL_NO_INDEX",
               pool->name, old_state);
        result = SXE_POOL_NO_INDEX;
        goto SXE_EARLY_OUT;
    }

    result = node - SXE_POOL_NODES(pool);
    sxe_pool_set_indexed_element_state_unlocked(array, result, old_state, new_state);

SXE_EARLY_OUT:
    sxe_pool_unlock(pool);

SXE_ERROR_OUT:
    SXER82("return %u // %s", result,
        result == SXE_POOL_NO_INDEX       ? "SXE_POOL_NO_INDEX"       :
        result == SXE_POOL_LOCK_NOT_TAKEN ? "SXE_POOL_LOCK_NOT_TAKEN" :
        result == SXE_POOL_LOCK_TAKEN     ? "SXE_POOL_LOCK_TAKEN"     : "id" );
    return result;
}

/**
 * Update an object's time of use and move it to the back its current state queue
 */
unsigned
sxe_pool_touch_indexed_element(void * array, unsigned id)
{
    SXE_POOL_IMPL * pool   = SXE_POOL_ARRAY_TO_IMPL(array);
    unsigned        result = SXE_POOL_LOCK_TAKEN;
    SXE_POOL_NODE * node;
    unsigned        state;

    SXEE82("sxe_pool_touch_indexed_element(pool->name=%s,id=%u)", pool->name, id);

    if ((result = sxe_pool_lock(pool)) == SXE_POOL_LOCK_NOT_TAKEN) {
        goto SXE_ERROR_OUT;
    }

    node  = &SXE_POOL_NODES(pool)[id];
    state = SXE_LIST_NODE_GET_ID(&node->list_node);
    sxe_pool_set_indexed_element_state_unlocked(array, id, state, state);
    sxe_pool_unlock(pool);

SXE_ERROR_OUT:
    SXER82("return %u // %s", result,
        result == SXE_POOL_NO_INDEX       ? "SXE_POOL_NO_INDEX"       :
        result == SXE_POOL_LOCK_NOT_TAKEN ? "SXE_POOL_LOCK_NOT_TAKEN" :
        result == SXE_POOL_LOCK_TAKEN     ? "SXE_POOL_LOCK_TAKEN"     : "unexpected return value!" );
    return result;
}

/**
 * Get the index of the oldest object in a given state (or SXE_POOL_NO_INDEX if none)
 */
unsigned
sxe_pool_get_oldest_element_index(void * array, unsigned state)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);
    SXE_POOL_NODE * node;
    unsigned        id = SXE_POOL_NO_INDEX;

    SXEE82("sxe_pool_get_oldest_element_index(pool->name=%s, state=%u)", pool->name, state);
    SXEA83(state <= pool->states, "state %u is greater than maximum state %u for pool %s", state, pool->states, pool->name);

    if ((node = sxe_list_peek_head(&SXE_POOL_QUEUE(pool)[state])) == NULL) {
        SXEL82("No objects in state %u, returning SXE_POOL_NO_INDEX", pool->name, state);
        goto SXE_EARLY_OUT;
    }

    id = node - SXE_POOL_NODES(pool);

SXE_EARLY_OR_ERROR_OUT:
    SXER81("return %u", id);
    return id;
}

/**
 * Get the use time of the oldest object in a given state (or 0.0 if none)
 */
double
sxe_pool_get_oldest_element_time(void * array, unsigned state)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);
    SXE_POOL_NODE * node;
    double          last_time = 0.0;

    SXEE82("sxe_pool_get_oldest_element_time(pool->name=%s, state=%u)", pool->name, state);
    SXEA83(state <= pool->states, "state %u is greater than maximum state %u for pool %s", state, pool->states, pool->name);

    if ((node = sxe_list_peek_head(&SXE_POOL_QUEUE(pool)[state])) == NULL) {
        SXEL52("sxe_pool_get_oldest_element_time(pool->name=%s): No objects in state %u", pool->name, state);
        goto SXE_EARLY_OUT;
    }

    last_time = node->last_time;

SXE_EARLY_OR_ERROR_OUT:
    SXER81("return %f", last_time);
    return last_time;
}

void
sxe_pool_delete(void * array)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);

    SXEE82("sxe_pool_delete(array=%p) pool->name='%s'", array, pool->name);
    SXE_POOL_ASSERT_LOCKS_DISABLED(pool);

    if (pool->event_timeout != NULL) {
        SXEA10(sxe_list_remove(&sxe_pool_timeout_list, pool) == pool, "Remove always returns the object removed");
    }

    free(pool);
    SXER80("return");
}

/**
 * Reset the lock on a pool
 *
 * @note Don't call this functions unless you really know what you're doing!
 */
void
sxe_pool_override_locked(void * array)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);

    SXEE81("sxe_pool_override_locked(pool->name=%s)", pool->name);

    if (pool->flags & SXE_POOL_LOCKS_ENABLED) {
        sxe_spinlock_force(&pool->spinlock, 0);
    }

    SXER80("return");
}
