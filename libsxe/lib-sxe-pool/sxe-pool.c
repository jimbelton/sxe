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
#include <stdbool.h>
#include <string.h>

#include "sxe-list.h"
#include "sxe-log.h"
#include "sxe-spinlock.h"
#include "sxe-util.h"
#include "mock.h"

#include "sxe-pool-private.h"

#define SXE_POOL_ON_INCORRECT_STATE_RETURN_FALSE 0
#define SXE_POOL_ON_INCORRECT_STATE_ABORT        1
#define SXE_POOL_ASSERT_ARRAY_INITIALIZED(array) SXEA81((array) != NULL, "%s(array=NULL): Uninitialized pool?", __func__)

static SXE_LIST sxe_pool_timeout_list;
static unsigned sxe_pool_timeout_count = 0;

/* Diagnostic function to convert state to string if none is supplied by the user
 */
static const char *
sxe_pool_state_to_string(unsigned state)                                /* Coverage Exclusion: Only called in debug mode */
{                                                                       /* Coverage Exclusion: Only called in debug mode */
    static char     string[2][12];
    static unsigned which = 0;
    char *          result;

    result = string[which];
    which  = (which + 1) % (sizeof(string) / sizeof(string[which]));    /* Coverage Exclusion: Only called in debug mode */
    snprintf(result, sizeof(string[which]), "%u", state);               /* Coverage Exclusion: Only called in debug mode */
    return result;                                                      /* Coverage Exclusion: Only called in debug mode */
}

const char *
sxe_pool_get_name(void * array)
{
    /* No diagnostics, thanks, since this is only used for diagnostics */
    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    return SXE_POOL_ARRAY_TO_IMPL(array)->name;
}

unsigned
sxe_pool_get_number_in_state(void * array, unsigned state)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);
    unsigned        count;

    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXEE82("sxe_pool_get_number_in_state(pool=%s,state=%s)", pool->name, (*pool->state_to_string)(state));
    count = SXE_LIST_GET_LENGTH(&SXE_POOL_QUEUE(pool)[state]);
    SXER81("return %u", count);
    return count;
}

unsigned
sxe_pool_index_to_state(void * array, unsigned id)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);
    unsigned        state;

    SXEE82("sxe_pool_index_to_state(name=%s,id=%u)", pool->name, id);
    SXEA13(id < pool->number, "sxe_pool_index_to_state(pool=%s,id=%u): Index is too big for pool (max index=%u)",
           pool->name, id, pool->number);
    state = SXE_LIST_NODE_GET_ID(&SXE_POOL_NODES(pool)[id].list_node);
    SXER81("return %s", (*pool->state_to_string)(state));
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
 * @param options = SXE_POOL_OPTION_LOCKED for thread safety
 *                  SXE_POOL_OPTION_TIMED  to keep the time of last insertion for each node
 *
 * @return A pointer to the array of objects
 *
 * @note   The base pointer must point at a region of memory big enough to hold the pool size (see sxe_pool_size())
 */
void *
sxe_pool_construct(void * base, const char * name, unsigned number, unsigned size, unsigned states, unsigned options)
{
    SXE_POOL_IMPL * pool;
    unsigned        i;
    SXE_LOG_LEVEL   log_level_saved;
    SXE_TIME        current_time = 0;    /* Initialize to shut the compiler up */

    SXEE86("sxe_pool_construct(base=%p,name=%s,number=%u,size=%u,states=%u,options=%u)", base, name, number, size, states, options);

    pool                  = (SXE_POOL_IMPL *)base;
    pool->queue           = (SXE_LIST      *)(sizeof(SXE_POOL_IMPL) + number * size);
    pool->nodes           = (SXE_POOL_NODE *)(sizeof(SXE_POOL_IMPL) + number * size + states * sizeof(SXE_LIST));
    pool->number          = number;
    pool->size            = size;
    pool->states          = states;
    pool->options         = options;
    pool->state_to_string = &sxe_pool_state_to_string;    /* Default to just printing the number */

    if (options & SXE_POOL_OPTION_LOCKED) {
        sxe_spinlock_construct(&pool->spinlock);
    }

    strncpy(pool->name, name, sizeof(pool->name));
    pool->name[sizeof(pool->name) - 1] = '\0';
    pool->event_timeout                = NULL;

#ifdef DEBUG
    memset(SXE_POOL_QUEUE(pool), 0xBE, sizeof(SXE_POOL_NODE) * states);
#endif

    for (i = states; i-- > 0; ) {
        SXE_LIST_CONSTRUCT(&SXE_POOL_QUEUE(pool)[i], i, SXE_POOL_NODE, list_node);
    }

    SXEL80("Construct the free list");

    log_level_saved = sxe_log_decrease_level(SXE_LOG_LEVEL_DEBUG);    /* Shut up logging on every node */

    if (options & SXE_POOL_OPTION_TIMED) {
        current_time  = sxe_time_get();
    }
    else {
        pool->next_count = 0;
    }

    for (i = number; i-- > 0; ) {
        if (options & SXE_POOL_OPTION_TIMED) {
            SXE_POOL_NODES(pool)[i].last.time  = current_time;
        }
        else {
            SXE_POOL_NODES(pool)[i].last.count = ++pool->next_count;
        }

        sxe_list_push(&SXE_POOL_QUEUE(pool)[0], &SXE_POOL_NODES(pool)[i].list_node);
    }

    sxe_log_set_level(log_level_saved);
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
 * @param name    Name of pool; pointer to '\0' terminated string
 * @param number  Number of elements in the pool
 * @param size    Size of each element in the pool
 * @param states  Number of states each element can be in
 * @param options SXE_POOL_OPTION_UNLOCKED for speed or SXE_POOL_OPTION_LOCKED for thread safety, and SXE_POOL_OPTION_TIMED to
 *                support timed operations; bit mask, combined with | operator
 *
 * @return A pointer to the array of objects
 *
 * @exception Aborts on failure to allocate memory
 */
void *
sxe_pool_new(const char * name, unsigned number, unsigned size, unsigned states, unsigned options)
{
    void          * base;
    void          * array;
    SXE_POOL_IMPL * pool;

    SXEE86("sxe_pool_new(name=%s,number=%u,size=%u,states=%u,options=%sSXE_POOL_OPTION_LOCKED|%sSXE_POOL_OPTION_TIMED)", name,
           number, size, states, options & SXE_POOL_OPTION_LOCKED ? "" : "!", options & SXE_POOL_OPTION_TIMED ? "" : "!");
    SXEL82("Allocating pool: %16s: %10u byte pool structure", name, sizeof(SXE_POOL_IMPL));
    SXEL84("Allocating pool: %16s: %10u bytes = %10u * %10u byte objects", name, size * number, number, size);
    SXEL84("Allocating pool: %16s: %10u bytes = %10u * %10u byte state queue heads",
            name, states * sizeof(SXE_LIST), states, sizeof(SXE_LIST));
    SXEL84("Allocating pool: %16s: %10u bytes = %10u * %10u byte internal nodes",
           name, sizeof(SXE_POOL_NODE)* number, number, sizeof(SXE_POOL_NODE));
    SXEA11((base = malloc(sxe_pool_size(number, size, states))) != NULL, "Error allocating SXE pool %s", name);

    array = sxe_pool_construct(base, name, number, size, states, options);
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

    array                = sxe_pool_new(name, number, size, states, SXE_POOL_OPTION_TIMED);
    pool                 = SXE_POOL_ARRAY_TO_IMPL(array);
    pool->event_timeout  = callback;
    pool->caller_info    = caller_info;
    pool->state_timeouts = malloc(states * sizeof(SXE_TIME));

    SXEA11(pool->state_timeouts != NULL, "Error allocating SXE pool %s; state timeout array", name);
    SXEL82("allocated %u bytes to hold %u state timeouts", states * sizeof(*timeouts), states);

    for (i = 0; i < states; i++) {
        SXEL82("state %u has timeout %f", i, timeouts[i]);
        pool->state_timeouts[i] = sxe_time_from_double_seconds(timeouts[i]);
    }

    sxe_list_push(&sxe_pool_timeout_list, pool);
    sxe_pool_timeout_count++;

    /* TODO: need sxe_pool_construct_with_timeouts() for pools with timeouts using spinlocks */

    SXER81("return array=%p", array);
    return array;
}

void
sxe_pool_set_state_to_string(void * array, const char * (*state_to_string)(unsigned state))
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);

    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXEE82("sxe_pool_set_state_to_string(pool=%s,state_to_string=%p)", pool->name, state_to_string);
    pool->state_to_string = state_to_string;
    SXER80("return");
}

void
sxe_pool_check_timeouts(void)
{
    SXE_LIST_WALKER walker;
    SXE_TIME        time_now;
    SXE_POOL_IMPL * pool;
    unsigned        state;
    SXE_TIME        timeout_for_this_state;
    SXE_TIME        time_oldest_for_this_state;
    SXE_TIME        time_oldest_for_this_state_last;
    unsigned        index_oldest_for_this_state;
    unsigned        index_oldest_for_this_state_last;

    SXEE80("sxe_pool_check_timeouts()");
    sxe_list_walker_construct(&walker, &sxe_pool_timeout_list);
    time_now = sxe_time_get();

    while ((pool = (SXE_POOL_IMPL *)sxe_list_walker_step(&walker)) != NULL) {
        for (state = 0; state < pool->states; state++) {
            timeout_for_this_state = pool->state_timeouts[state];

            if (timeout_for_this_state == 0) {
                SXEL81("state %s timeout is infinite; ignoring", (*pool->state_to_string)(state));
                continue;
            }

            index_oldest_for_this_state_last = SXE_POOL_NO_INDEX;
            time_oldest_for_this_state_last  = 0;

            for (;;) {
                index_oldest_for_this_state = sxe_pool_get_oldest_element_index(SXE_POOL_IMPL_TO_ARRAY(pool), state);

                if (SXE_POOL_NO_INDEX == index_oldest_for_this_state) {
                    SXEL82("state %s timeout %f has no elements", (*pool->state_to_string)(state), timeout_for_this_state);
                    break;
                }

                time_oldest_for_this_state = sxe_pool_get_oldest_element_time(SXE_POOL_IMPL_TO_ARRAY(pool), state);

                SXEA10(   (index_oldest_for_this_state_last != index_oldest_for_this_state)
                       || (time_oldest_for_this_state       != time_oldest_for_this_state_last),
                       "Internal: callback failed to update state on pool element with timed out");

                if ((time_now - time_oldest_for_this_state) < timeout_for_this_state) {
                    SXEL83("state %s timeout %f has not been reached for oldest index %u", (*pool->state_to_string)(state),
                           timeout_for_this_state, index_oldest_for_this_state);
                    break;
                }

                SXEL83("state %s timeout %f has been reached for oldest index %u", (*pool->state_to_string)(state),
                       timeout_for_this_state, index_oldest_for_this_state);
                (*pool->event_timeout)(SXE_POOL_IMPL_TO_ARRAY(pool), index_oldest_for_this_state, pool->caller_info);
                index_oldest_for_this_state_last = index_oldest_for_this_state;
                time_oldest_for_this_state_last  = time_oldest_for_this_state;
            }
        }
    }

    SXER80("return");
}

/**
 * Internal lockless function to move a specific object from one state queue to the tail of another
 */
static bool
sxe_pool_set_indexed_element_state_unlocked(void * array, unsigned id, unsigned old_state, unsigned new_state,
                                            unsigned on_incorrect_state)
{
    bool            success = false;
    SXE_POOL_IMPL * pool    = SXE_POOL_ARRAY_TO_IMPL(array);
    SXE_POOL_NODE * node;

    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXEE85("(pool=%s,id=%u,old_state=%s,new_state=%s,on_incorrect_state=%s)",
           pool->name, id, (*pool->state_to_string)(old_state), (*pool->state_to_string)(new_state),
           on_incorrect_state == SXE_POOL_ON_INCORRECT_STATE_ABORT ? "ABORT" : "RETURN_FALSE");

    node = &SXE_POOL_NODES(pool)[id];

    if (SXE_LIST_NODE_GET_ID(&node->list_node) != old_state) {
        SXEA15(on_incorrect_state != SXE_POOL_ON_INCORRECT_STATE_ABORT,
               "sxe_pool_set_indexed_element_state_unlocked(pool=%s,id=%u,old_state=%s,new_state=%s): Object is in state %s",
               pool->name, node - SXE_POOL_NODES(pool), (*pool->state_to_string)(old_state),
               (*pool->state_to_string)(new_state), (*pool->state_to_string)(SXE_LIST_NODE_GET_ID(&node->list_node)));
        goto SXE_EARLY_OUT;
    }

    sxe_list_remove(&SXE_POOL_QUEUE(pool)[old_state], node);

    if (pool->options & SXE_POOL_OPTION_TIMED) {
        node->last.time  = sxe_time_get();
    }
    else {
        node->last.count = ++pool->next_count;
    }

    sxe_list_push(&SXE_POOL_QUEUE(pool)[new_state], &node->list_node);
    success = true;

SXE_EARLY_OUT:
    SXER81("return %s", success ? "true // success" : "false // failure");
    return success;
}

/**
 * Move a specific object from one state queue to the tail of another
 */
unsigned
sxe_pool_set_indexed_element_state(void * array, unsigned id, unsigned old_state, unsigned new_state)
{
    SXE_POOL_IMPL * pool   = SXE_POOL_ARRAY_TO_IMPL(array);
    unsigned        result = SXE_POOL_LOCK_TAKEN;

    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXE_UNUSED_PARAMETER(old_state);   /* Used to verify sanity in debug build only */
    SXEE84("(pool=%s,id=%u,old_state=%s,new_state=%s)",
           pool->name, id, (*pool->state_to_string)(old_state), (*pool->state_to_string)(new_state));
    SXEA83(id < pool->number, "sxe_pool_set_indexed_element_state(pool=%s,id=%u): Index is too big (number=%u)",
           pool->name, id, pool->number);
    SXEA83(old_state <= pool->states, "state %u is greater than maximum state %u for pool %s", old_state, pool->states, pool->name);
    SXEA83(new_state <= pool->states, "state %u is greater than maximum state %u for pool %s", new_state, pool->states, pool->name);

    if ((result = sxe_pool_lock(pool)) == SXE_POOL_LOCK_NOT_TAKEN) {
        goto SXE_ERROR_OUT;
    }

    SXEA10(sxe_pool_set_indexed_element_state_unlocked(array, id, old_state, new_state, SXE_POOL_ON_INCORRECT_STATE_ABORT),
           "sxe_pool_set_indexed_element_state_unlocked failed: internal fatal error");
    sxe_pool_unlock(pool);

SXE_ERROR_OUT:
    SXER81("return %s", sxe_pool_return_to_string(result));
    return result;
}

/**
 * Try to move a specific object from one state queue to the tail of another
 *
 * @param array           Pointer to the pool array
 * @param id              Index of the element to move
 * @param old_state       State to move the element from
 * @param new_state_inout Pointer to a state; on input, state to move the element from; on output, state the element is in
 *
 * @return id if the move occured (in this case, *new_state_inout will be unchanged); SXE_POOL_INCORRECT_STATE if the element
 *         was not in old_state (in this case, *new_state_inout will be set to the state the element was in); or
 *         SXE_POOL_LOCK_NOT_TAKEN if the pool lock could not be taken
 */
unsigned
sxe_pool_try_to_set_indexed_element_state(void * array, unsigned id, unsigned old_state, unsigned * new_state_inout)
{
    SXE_POOL_IMPL * pool   = SXE_POOL_ARRAY_TO_IMPL(array);
    unsigned        result = SXE_POOL_INCORRECT_STATE;

    SXE_UNUSED_PARAMETER(old_state);   /* Used to verify sanity in debug build only */
    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXEE84("sxe_pool_set_indexed_element_state(pool=%s,id=%u,old_state=%s,new_state=%s)",
           pool->name, id, (*pool->state_to_string)(old_state), (*pool->state_to_string)(*new_state_inout));
    SXEA83(id < pool->number, "sxe_pool_set_indexed_element_state(pool=%s,id=%u): Index is too big (number=%u)",
           pool->name, id, pool->number);
    SXEA83(old_state <= pool->states, "state %u is greater than maximum state %u for pool %s", old_state, pool->states, pool->name);
    SXEA83(*new_state_inout <= pool->states, "state %u is greater than maximum state %u for pool %s", *new_state_inout, pool->states, pool->name);

    if (sxe_pool_lock(pool) == SXE_POOL_LOCK_NOT_TAKEN) {
        result = SXE_POOL_LOCK_NOT_TAKEN;    /* Coverage exclusion: Add tests before using in multiprocess code */
        goto SXE_ERROR_OUT;                  /* Coverage exclusion: Add tests before using in multiprocess code */
    }

    if (sxe_pool_set_indexed_element_state_unlocked(array, id, old_state, *new_state_inout, SXE_POOL_ON_INCORRECT_STATE_RETURN_FALSE)) {
        result = id;
    }
    else {
        *new_state_inout = sxe_pool_index_to_state(array, id);
    }

    sxe_pool_unlock(pool);

SXE_ERROR_OUT:
    SXER81((result == SXE_POOL_LOCK_NOT_TAKEN || result == SXE_POOL_INCORRECT_STATE) ? "return %s" : "return %u",
           result == SXE_POOL_LOCK_NOT_TAKEN  ? "LOCK_NOT_TAKEN"  :
           result == SXE_POOL_INCORRECT_STATE ? "INCORRECT_STATE" :
           SXE_CAST(void *, result));
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

    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXEE83("sxe_pool_set_oldest_element_state(pool=%s, old_state=%s, new_state=%s)",
           pool->name, (*pool->state_to_string)(old_state), (*pool->state_to_string)(new_state));
    SXEA83(old_state <= pool->states, "old state %u is greater than maximum state %u for pool %s", old_state, pool->states,
           pool->name);
    SXEA83(new_state <= pool->states, "new state %u is greater than maximum state %u for pool %s", new_state, pool->states,
           pool->name);

    if ((result = sxe_pool_lock(pool)) == SXE_POOL_LOCK_NOT_TAKEN) {
        goto SXE_ERROR_OUT;
    }

    if ((node = sxe_list_peek_head(&SXE_POOL_QUEUE(pool)[old_state])) == NULL) {
        SXEL82("sxe_pool_set_oldest_element_state(pool=%s): No objects in state %s; returning SXE_POOL_NO_INDEX",
               pool->name, (*pool->state_to_string)(old_state));
        result = SXE_POOL_NO_INDEX;
        goto SXE_EARLY_OUT;
    }

    result = node - SXE_POOL_NODES(pool);
    SXEA10(sxe_pool_set_indexed_element_state_unlocked(array, result, old_state, new_state, SXE_POOL_ON_INCORRECT_STATE_ABORT),
           "sxe_pool_set_indexed_element_state_unlocked failed: internal fatal error");

SXE_EARLY_OUT:
    sxe_pool_unlock(pool);

SXE_ERROR_OUT:
    SXER81("return %s", sxe_pool_return_to_string(result));
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

    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXEE82("sxe_pool_touch_indexed_element(pool=%s,id=%u)", pool->name, id);

    if ((result = sxe_pool_lock(pool)) == SXE_POOL_LOCK_NOT_TAKEN) {
        goto SXE_ERROR_OUT;
    }

    node  = &SXE_POOL_NODES(pool)[id];
    state = SXE_LIST_NODE_GET_ID(&node->list_node);
    SXEA10(sxe_pool_set_indexed_element_state_unlocked(array, id, state, state, SXE_POOL_ON_INCORRECT_STATE_ABORT),
           "sxe_pool_set_indexed_element_state_unlocked failed: internal fatal error");
    sxe_pool_unlock(pool);

SXE_ERROR_OUT:
    SXER81("return %s", sxe_pool_return_to_string(result));
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

    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXEE82("sxe_pool_get_oldest_element_index(pool=%s, state=%s)", pool->name, (*pool->state_to_string)(state));
    SXEA83(state <= pool->states, "state %u is greater than maximum state %u for pool %s", state, pool->states, pool->name);

    if (sxe_pool_lock(pool) == SXE_POOL_LOCK_NOT_TAKEN) {
        goto SXE_ERROR_OUT;  /* Coverage exclusion: Add tests before using in multiprocess code */
    }

    if ((node = sxe_list_peek_head(&SXE_POOL_QUEUE(pool)[state])) == NULL) {
        SXEL81("No objects in state %s, returning SXE_POOL_NO_INDEX", (*pool->state_to_string)(state));
        goto SXE_EARLY_OUT;
    }

    id = node - SXE_POOL_NODES(pool);

SXE_EARLY_OUT:
    sxe_pool_unlock(pool);

SXE_ERROR_OUT:
    SXER81("return %s", sxe_pool_return_to_string(id));
    return id;
}

/*
 * Get the time or count of the oldest object in a given state (or 0 if none)
 *
 * @param array = Pointer to the pool array
 * @param state = State to check
 */
static SXE_TIME
sxe_pool_impl_get_oldest_element_time_or_count(SXE_POOL_IMPL * pool, unsigned state)
{
    SXE_POOL_NODE * node;
    SXE_TIME        last_time = 0;

    SXEE83("sxe_pool_get_oldest_element_%s(pool=%s, state=%s)", pool->options & SXE_POOL_OPTION_TIMED ? "time" : "count",
           pool->name, (*pool->state_to_string)(state));

    if (sxe_pool_lock(pool) == SXE_POOL_LOCK_NOT_TAKEN) {
        goto SXE_ERROR_OUT;  /* Coverage exclusion: Add tests before using in multiprocess code */
    }

    if ((node = sxe_list_peek_head(&SXE_POOL_QUEUE(pool)[state])) == NULL) {
        SXEL81("No objects in state %s", (*pool->state_to_string)(state));
        goto SXE_EARLY_OUT;
    }

    last_time = node->last.time;

SXE_EARLY_OUT:
    sxe_pool_unlock(pool);

SXE_ERROR_OUT:
    SXER81("return %llu", (unsigned long long)last_time);
    return last_time;
}

/**
 * Get the last use time of the oldest object in a given state (or 0 if none)
 *
 * @param array = Pointer to the pool array
 * @param state = State to check
 *
 * @exception Aborts if the pool is not a timed pool
 */
SXE_TIME
sxe_pool_get_oldest_element_time(void * array, unsigned state)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);

    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXEA12(pool->options & SXE_POOL_OPTION_TIMED, "%s: pool %s is a timed pool", __func__, pool->name);
    return sxe_pool_impl_get_oldest_element_time_or_count(pool, state);
}

/**
 * Get the insertion counter of the oldest object in a given state (or 0 if none)
 *
 * @param array = Pointer to the pool array
 * @param state = State to check
 *
 * @exception Aborts if the pool is a timed pool
 */
uint64_t
sxe_pool_get_oldest_element_count(void * array, unsigned state)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);

    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXEA12(!(pool->options & SXE_POOL_OPTION_TIMED), "%s: pool %s is a timed pool", __func__, pool->name);
    return (uint64_t)sxe_pool_impl_get_oldest_element_time_or_count(pool, state);
}

/**
 * Get the time of a given object has been in it's current state,  by index
 *
 * @param array = Pointer to the pool array
 * @param state = State to check
 *
 * @exception Release mode assertion: the pool must be a timed pool
 */
SXE_TIME
sxe_pool_get_element_time_by_index(void * array, unsigned element)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);
    SXE_POOL_NODE * node;

    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXEE82("sxe_pool_get_element_time_by_index(pool=%s, index=%u)", pool->name, element);
    SXEA11(pool->options & SXE_POOL_OPTION_TIMED, "sxe_pool_get_element_time_by_index: pool %s is not a timed pool", pool->name);
    SXEA83(element < pool->number, "index %u is greater than maximum index %u for pool %s", element, pool->number, pool->name);

    node = &SXE_POOL_NODES(pool)[element];

SXE_EARLY_OR_ERROR_OUT:
    SXER81("return %llu", node->last.time);
    return node->last.time;
}

void
sxe_pool_delete(void * array)
{
    SXE_POOL_IMPL * pool = SXE_POOL_ARRAY_TO_IMPL(array);

    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXEE81("sxe_pool_delete(pool=%s)", pool->name);

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

    SXE_POOL_ASSERT_ARRAY_INITIALIZED(array);
    SXEE81("sxe_pool_override_locked(pool=%s)", pool->name);

    if (pool->options & SXE_POOL_OPTION_LOCKED) {
        sxe_spinlock_force(&pool->spinlock, 0);
    }

    SXER80("return");
}
