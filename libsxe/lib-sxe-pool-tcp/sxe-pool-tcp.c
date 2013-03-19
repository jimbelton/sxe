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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ev.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-pool.h"
#include "sxe-pool-tcp.h"
#include "sxe-pool-tcp-private.h"
#include "sxe-spawn.h"

#ifdef WIN32
    /* TODO: Implement on Windows once sxe_spawn() is implemented on Windows */
#else

#define SXE_CONCURRENCY_MAX      1000000
#define SXE_WRITE_RAMP           10
#define SXE_CONNECTION_RAMP      16
#define SXE_POOL_TCP_FAILURE_MAX 2

#define SXE_POOL_TCP_ASSERT_CONSISTENT(pool)                                                       \
    SXEA86(SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_UNCONNECTED)                  \
         + SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_CONNECTING)                   \
         + SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_INITIALIZING)                 \
         + SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_READY_TO_SEND)                \
         + SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_IN_USE) == pool->concurrency, \
           "Internal pool counts (%u,%u,%u,%u,%u) don't match concurrency %u",                     \
           SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_UNCONNECTED),                 \
           SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_CONNECTING),                  \
           SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_INITIALIZING),                \
           SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_READY_TO_SEND),               \
           SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_IN_USE), pool->concurrency)

static void sxe_pool_tcp_event_connected(SXE * this);               /* prototyped because of dependency within connect_ramp() */
static void sxe_pool_tcp_event_read(     SXE * this, int length);   /* prototyped because of dependency within connect_ramp() */
static void sxe_pool_tcp_event_close(    SXE * this);               /* prototyped because of dependency within connect_ramp() */

static void
connect_ramp(SXE * this, SXE_POOL_TCP * pool)
{
    int                 i;
    unsigned            connections_in_use;
    unsigned            item;
    SXE_POOL_TCP_NODE * node;
    SXE_RETURN          result;

    SXE_UNUSED_ARGUMENT(this);
    SXEE81I("connect_ramp(pool=%s)", SXE_POOL_TCP_GET_NAME(pool));
    SXE_POOL_TCP_ASSERT_CONSISTENT(pool);
    connections_in_use = pool->concurrency - sxe_pool_get_number_in_state(pool->nodes, SXE_POOL_TCP_STATE_UNCONNECTED);
	SXE_UNUSED_PARAMETER(connections_in_use);
	
    if (SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_UNCONNECTED) == 0) {
        SXEL81I("All %u TCP connections are in use", pool->concurrency);
        goto SXE_EARLY_OUT;
    }

    SXEL83I("Make at most %u TCP connections starting with connection %u of %u", SXE_CONNECTION_RAMP,
            connections_in_use + 1, pool->concurrency);

    for (i = 0; i < SXE_CONNECTION_RAMP; i++) {
        item = sxe_pool_get_oldest_element_index(pool->nodes, SXE_POOL_TCP_STATE_UNCONNECTED);

        if (item == SXE_POOL_NO_INDEX) {
            goto SXE_EARLY_OUT;
        }

        node = &pool->nodes[item];

        if (node->failure_count >= SXE_POOL_TCP_FAILURE_MAX) {
            SXEL82I("TCP pool %s: node %u has failed twice: skipping it", SXE_POOL_TCP_GET_NAME(pool), item);
            sxe_pool_touch_indexed_element(pool->nodes, item);
            continue;
        }

        sxe_pool_set_indexed_element_state(pool->nodes, item, SXE_POOL_TCP_STATE_UNCONNECTED, SXE_POOL_TCP_STATE_CONNECTING);
        node->tcp_pool = pool;

        if (pool->is_spawn) {
            node->previous_spawn = node->current_spawn;
            result               = sxe_spawn(NULL, &node->current_spawn, pool->command, pool->arg1, pool->arg2,
                                             sxe_pool_tcp_event_connected, sxe_pool_tcp_event_read, sxe_pool_tcp_event_close);

            if (result != SXE_RETURN_OK)
            {
                SXEL22I("TCP pool %s: failed to allocate a SXE object to spawn %s", SXE_POOL_TCP_GET_NAME(pool), pool->command);
                sxe_pool_set_indexed_element_state(pool->nodes, item, SXE_POOL_TCP_STATE_CONNECTING,
                                                   SXE_POOL_TCP_STATE_UNCONNECTED);
                goto SXE_ERROR_OUT;
            }

            node->sxe = SXE_SPAWN_GET_SXE(&node->current_spawn);

            if (SXE_SPAWN_GET_PID(&node->previous_spawn) == 0) {
                SXEL14I("TCP pool %s: spawned '%s': pid %d (0x%0x)", SXE_POOL_TCP_GET_NAME(pool), pool->command,
                        SXE_SPAWN_GET_PID(&node->current_spawn), SXE_SPAWN_GET_PID(&node->current_spawn));
            }
        }
        else {
            node->sxe = sxe_new_tcp(NULL, "INADDR_ANY", 0, sxe_pool_tcp_event_connected, sxe_pool_tcp_event_read,
                                    sxe_pool_tcp_event_close);

            if (node->sxe == NULL) {
                SXEL22I("TCP pool %s: failed to allocate a SXE object to connect to %s", SXE_POOL_TCP_GET_NAME(pool),
                        pool->peer_ip);
                sxe_pool_set_indexed_element_state(pool->nodes, item, SXE_POOL_TCP_STATE_CONNECTING,
                                                   SXE_POOL_TCP_STATE_UNCONNECTED);
                goto SXE_ERROR_OUT;
            }

            sxe_connect(node->sxe, pool->peer_ip, pool->peer_port);
        }

        SXEL83I("TCP pool %s: node %u associated with SXE id %u (state=CONNECTING)", SXE_POOL_TCP_GET_NAME(pool), item,
                SXE_ID(node->sxe));
        SXE_USER_DATA(node->sxe) = &pool->nodes[item];
        SXE_POOL_TCP_ASSERT_CONSISTENT(pool);
    }

SXE_EARLY_OR_ERROR_OUT:
    SXER80I("return");
}

static void
sxe_pool_tcp_restart(SXE * this, SXE_POOL_TCP * pool)
{
    SXEE81I("sxe_pool_tcp_restart(pool=%s)", SXE_POOL_TCP_GET_NAME(pool));
    SXE_POOL_TCP_ASSERT_CONSISTENT(pool);
    SXEA80(SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_READY_TO_SEND) > 0,
           "sxe_pool_tcp_restart called, but no connections in ready to send state");

    connect_ramp(this, pool);    /* If there's an unconnected node in the pool, ramp it */

    if (pool->count_ready_to_write_events_queued > 0)
    {
        SXEL80I("Caller ready to write, free connection now available, initiating ready_to_write event");
        --pool->count_ready_to_write_events_queued;
        (*pool->event_ready_to_write)(pool, pool->caller_info);
    }

    SXER80I("return");
}

void
sxe_pool_tcp_initialized(SXE * this)
{
    SXE_POOL_TCP_NODE * node;
    SXE_POOL_TCP      * pool;
    unsigned            item;

    SXEE80I("sxe_pool_tcp_initialized()");

    node = SXE_USER_DATA(this);
    SXEA80I(node != NULL, "Node pointer in SXE user data is NULL");

    pool = node->tcp_pool;
    SXEL82I("node=%p, pool=%p", node, pool);

    item = (node - pool->nodes);
    sxe_pool_set_indexed_element_state(pool->nodes, item, SXE_POOL_TCP_STATE_INITIALIZING, SXE_POOL_TCP_STATE_READY_TO_SEND);
    sxe_pool_tcp_restart(this, pool);

    SXER80I("return");
}

static void
sxe_pool_tcp_event_connected(SXE * this)
{
    SXE_POOL_TCP_NODE * node;
    SXE_POOL_TCP      * pool;
    unsigned            item;
    pid_t               pid_previous;
    pid_t               pid_returned;
    int                 status;

    SXEE80I("sxe_pool_tcp_event_connected()");
    node = SXE_USER_DATA(this);
    SXEA80I(node != NULL, "Node pointer in SXE user data is NULL");
    pool = node->tcp_pool;
    item = (node - pool->nodes);
    SXEL82I("TCP pool %s: Node %u connection established", SXE_POOL_TCP_GET_NAME(pool), item);

    /* Should now be able to check the exit status of the previous instance of the command
     */
    if (pool->is_spawn && ((pid_previous = SXE_SPAWN_GET_PID(&node->previous_spawn)) != 0)) {
        SXEV82I((pid_returned = waitpid(pid_previous, &status, WNOHANG)), >= 0, "waitpid(%d) failed: %s",
                pid_previous, strerror(errno));

        if (pid_returned == 0) {
            SXEL23I("TCP pool %s: Node %u previous process %d did not exit: killing it",
                    SXE_POOL_TCP_GET_NAME(pool), item, pid_previous);
            sxe_spawn_kill(&node->previous_spawn, SIGKILL);
            node->failure_count++;
        }
        else if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 0) {
                SXEL83I("TCP pool %s: Node %u previous process %d restarted gracefully",
                        SXE_POOL_TCP_GET_NAME(pool), item, pid_previous);
            }
            else {
                SXEL24I("TCP pool %s: Node %u previous process %d exited with error %d",
                        SXE_POOL_TCP_GET_NAME(pool), item, pid_previous, WEXITSTATUS(status));
                node->failure_count++;
            }
        }
        else {
            SXEA82I(WIFSIGNALED(status), "previous process %d exited with unexpected status %d", pid_previous, status);
            SXEL24I("TCP pool %s: Node %u previous process %d terminated by signal %d", /* Coverage Exclusion: Todo: fix race condition infrequently causing coverage failure */
                    SXE_POOL_TCP_GET_NAME(pool), item, pid_previous, WTERMSIG(status));
            node->failure_count++;                                                      /* Coverage Exclusion: Todo: fix race condition infrequently causing coverage failure */
        }
    }

    /* If the caller specified an initialization timeout, go into the initialization state.
     */
    if (pool->has_initialization) {
        sxe_pool_set_indexed_element_state(pool->nodes, item, SXE_POOL_TCP_STATE_CONNECTING, SXE_POOL_TCP_STATE_INITIALIZING);
        SXE_POOL_TCP_ASSERT_CONSISTENT(pool);
    }
    else {
        sxe_pool_set_indexed_element_state(pool->nodes, item, SXE_POOL_TCP_STATE_CONNECTING, SXE_POOL_TCP_STATE_READY_TO_SEND);
        sxe_pool_tcp_restart(this, pool);
    }

    if (pool->event_connected != NULL) {
        SXEL80I("Calling caller's connected callback");
        (*pool->event_connected)(this);
        SXEA80I(SXE_USER_DATA(this) == node, "Cannot modify TCP pool connection user data in connected callback");
    }

SXE_EARLY_OR_ERROR_OUT:
    SXER80I("return");
}

static void
sxe_pool_tcp_event_read(SXE * this, int length)
{
    SXE_POOL_TCP_NODE * node;
    SXE_POOL_TCP      * pool;
    unsigned            item;

    SXEE81I("sxe_pool_tcp_event_read(length=%d)", length);
    SXEA80I(length != 0, "sxe_pool_tcp_event_read: length cannot be zero");
    node = SXE_USER_DATA(this);
    SXEA80I(node != NULL, "Node pointer in SXE user data is NULL");
    pool = node->tcp_pool;
    item = (node - pool->nodes);
    SXEL83I("node=%p, pool=%p, item=%u", node, pool, item);

    /* Initialization state - let the caller handle things.
     */
    if (sxe_pool_index_to_state(pool->nodes, item) == SXE_POOL_TCP_STATE_INITIALIZING) {
        SXEL81I("Calling caller's read callback with %u bytes", length);
        (*pool->event_read)(this, length);
        SXEA80I(SXE_USER_DATA(this) == node, "Cannot modify the TCP pool connection user data in the initialization callback");
        goto SXE_EARLY_OUT;
    }

    node->failure_count = 0;
    SXEL81I("Setting users's user data %p for callback", node->user_data);
    SXE_USER_DATA(this) = node->user_data;
    SXEL81I("Calling caller's read callback with %u bytes", length);
    (*pool->event_read)(this, length);
    SXEL81I("Restoring node pointer %p as user data", node);
    SXE_USER_DATA(this) = node;

    /* If the data was not consumed, we're done.
     */
    if (SXE_BUF_USED(this) != 0) {
        goto SXE_EARLY_OUT;    /* Coverage Exclusion: Defragmentation */
    }

    SXEL81I("Caller has consumed the reply - connection %u is now ready to send", item);
    sxe_pool_set_indexed_element_state(pool->nodes, item, SXE_POOL_TCP_STATE_IN_USE, SXE_POOL_TCP_STATE_READY_TO_SEND);

    if (pool->count_ready_to_write_events_queued > 0)
    {
        SXEL80I("Caller ready to write, free connection now available, initiating ready_to_write event");
        --pool->count_ready_to_write_events_queued;
        (*pool->event_ready_to_write)(pool, pool->caller_info);
    }

    SXEA80I(sxe_pool_index_to_state(pool->nodes, item) != SXE_POOL_TCP_STATE_UNCONNECTED, "Item %u is in UNCONNECTED state");

SXE_EARLY_OR_ERROR_OUT:
    SXER80I("return");
}

void
sxe_pool_tcp_queue_ready_to_write_event(SXE_POOL_TCP * pool)
{
    SXEE81("sxe_pool_tcp_queue_ready_to_write_event(pool=%s)", SXE_POOL_TCP_GET_NAME(pool));

    if (SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool, SXE_POOL_TCP_STATE_READY_TO_SEND) == 0) {
        SXEL80("No connections are currently ready to send");
        ++pool->count_ready_to_write_events_queued;
    }
    else {
        SXEL80("Connection ready to send, trigger ready_to_write event");
        (*pool->event_ready_to_write)(pool, pool->caller_info);
    }

    SXER80("return");
}

void
sxe_pool_tcp_unqueue_ready_to_write_event(SXE_POOL_TCP * pool)
{
    SXEE81("sxe_pool_tcp_unqueue_ready_to_write_event(pool=%s)", SXE_POOL_TCP_GET_NAME(pool));

    SXEA10(pool->count_ready_to_write_events_queued > 0, "No connections are outstanding when unqueueing");
    --pool->count_ready_to_write_events_queued;

    SXER80("return");
}

SXE_RETURN
sxe_pool_tcp_write(SXE_POOL_TCP * pool, const void * buf, unsigned size, void * user_data)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    SXE *      that;
    unsigned   item;

    SXEE84("sxe_pool_tcp_write(pool=%s, buf=%p, size=%d, user_data=%p)", SXE_POOL_TCP_GET_NAME(pool), buf, size, user_data);
    item = sxe_pool_set_oldest_element_state(pool->nodes, SXE_POOL_TCP_STATE_READY_TO_SEND, SXE_POOL_TCP_STATE_IN_USE);
    SXEA10(item != SXE_POOL_NO_INDEX, "No connections currently ready to send");

    that = pool->nodes[item].sxe;
    pool->nodes[item].user_data = user_data;
    result = sxe_write(that, buf, size);

SXE_EARLY_OR_ERROR_OUT:
    SXER81("return %s", sxe_return_to_string(result));
    return result;
}

static void
sxe_pool_tcp_event_close(SXE * this)
{
    SXE_POOL_TCP_NODE * node;
    SXE_POOL_TCP      * pool;
    unsigned            item;
    SXE_POOL_TCP_STATE  state;

    SXEE80I("sxe_pool_tcp_event_close()");
    node  = SXE_USER_DATA(this);
    SXEA80I(node != NULL, "Node pointer in SXE user data is NULL");
    pool  = node->tcp_pool;
    item  = (node - pool->nodes);
    SXEL83I("Node=%p, pool=%s, item=%u", node, SXE_POOL_TCP_GET_NAME(pool), item);
    state = sxe_pool_index_to_state(pool->nodes, item);
    SXEA81I(state != SXE_POOL_TCP_STATE_UNCONNECTED, "sxe_pool_tcp_event_close(): node %u is in the UNCONNECTED state", item);

    /* Each time a TCP connection is closed, increment the failure count. Also incremented if a spawned process never connects.
     */
    if (!pool->is_spawn || (state == SXE_POOL_TCP_STATE_CONNECTING)) {
        node->failure_count++;
    }

    if (node->failure_count >= SXE_POOL_TCP_FAILURE_MAX) {
        SXEL23I("TCP pool %s: Connection %u has failed %u times: giving up", SXE_POOL_TCP_GET_NAME(pool), item,
                node->failure_count);
        }
        else {
            SXEL83I("TCP pool %s: Connection %u has failed %u times: retrying",  SXE_POOL_TCP_GET_NAME(pool), item,
                    node->failure_count);
    }

    sxe_pool_set_indexed_element_state(pool->nodes, item, state, SXE_POOL_TCP_STATE_UNCONNECTED);
    SXE_POOL_TCP_ASSERT_CONSISTENT(pool);

    if (pool->event_close != NULL) {
        SXE_USER_DATA(this) = node->user_data;
        SXEL80I("Calling caller's close callback");
        (*pool->event_close)(this);
        SXE_USER_DATA(this) = NULL;
    }

    connect_ramp(this, pool);
    SXER80I("return");
}

static void
sxe_pool_tcp_event_read_timeout(void * array, unsigned array_index, void * caller_info)
{
    SXE_POOL_TCP * pool = caller_info;
    SXE          * this;

    SXEE63("sxe_pool_tcp_event_read_timeout(array=%p,array_index=%u,pool=%s)", array, array_index, SXE_POOL_TCP_GET_NAME(pool));
    SXE_UNUSED_ARGUMENT(array);

    if (pool->event_timeout != NULL) {
        (*pool->event_timeout)(pool, pool->caller_info);
    }

    this = pool->nodes[array_index].sxe;
    SXEL21("Timed out pool response on SXE id %u: closing connection", SXE_ID(this));
    SXEV61(sxe_close(this), == SXE_RETURN_OK, "SXE id %u is already closed", SXE_ID(this));
    sxe_pool_tcp_event_close(this);
    SXER60("return");
}

/* Should be called at registration by each package that will construct a pool in its init() function.
 */
void
sxe_pool_tcp_register(unsigned concurrency)
{
    SXEE81("sxe_pool_tcp_register(concurrency=%u)", concurrency);
    sxe_register(concurrency, 0);
    SXER80("return");
}

static SXE_POOL_TCP *
sxe_pool_tcp_new_internal(unsigned int           concurrency           ,
                          const char           * pool_name             ,
                          SXE_POOL_TCP_EVENT     event_ready_to_write  ,
                          SXE_IN_EVENT_CONNECTED event_connected       ,
                          SXE_IN_EVENT_READ      event_read            ,
                          SXE_IN_EVENT_CLOSE     event_close           ,
                          double                 initialization_timeout,
                          double                 response_timeout      ,
                          SXE_POOL_TCP_EVENT     event_timeout         ,
                          void                 * caller_info           )
{
    SXE_POOL_TCP * pool_tcp;
    double         state_timeouts[] = {0.0, 0.0, initialization_timeout, 0.0, response_timeout};
    void *         nodes;
    unsigned       i;

    SXEE86("sxe_pool_tcp_new_internal(concurrency=%d, pool_name=%s, event_ready_to_write=%p, event_connected=%p, event_read=%p, event_close=%p)",
                                      concurrency,    pool_name,    event_ready_to_write,    event_connected,    event_read,    event_close);
    SXEL84("sxe_pool_tcp_new_internal: initialization_timeout=%.2f, response_timeout=%.2f, event_timeout=%p, caller_info=%p",
                                       initialization_timeout,      response_timeout,      event_timeout,    caller_info);

    SXEA10((sizeof(state_timeouts) / sizeof(*state_timeouts)) == SXE_POOL_TCP_STATE_NUMBER_OF_STATES, "Internal: number of states and state timeouts do not match (should never happen)");
    SXEA10(event_read           != NULL, "event_read must not be NULL");
    pool_tcp = malloc(sizeof(SXE_POOL_TCP));
    SXEA11(pool_tcp != NULL, "Unable to allocate TCP pool '%s'", pool_name);

    nodes = sxe_pool_new_with_timeouts(pool_name, concurrency, sizeof(SXE_POOL_TCP_NODE), SXE_POOL_TCP_STATE_NUMBER_OF_STATES,
                                       state_timeouts, sxe_pool_tcp_event_read_timeout, pool_tcp);
    pool_tcp->nodes                              = nodes;
    pool_tcp->concurrency                        = concurrency;
    pool_tcp->count_ready_to_write_events_queued = 0;
    pool_tcp->event_ready_to_write               = event_ready_to_write;
    pool_tcp->event_connected                    = event_connected;
    pool_tcp->event_read                         = event_read;
    pool_tcp->event_close                        = event_close;
    pool_tcp->event_timeout                      = event_timeout;
    pool_tcp->caller_info                        = caller_info;
    pool_tcp->has_initialization                 = (initialization_timeout != 0.0);

    for (i = 0; i < concurrency; i++) {
        pool_tcp->nodes[i].failure_count = 0;
    }

    SXER81("return pool_tcp=%p", pool_tcp);
    return pool_tcp;
}

SXE_POOL_TCP *
sxe_pool_tcp_new_connect(unsigned int           concurrency           ,
                         const char           * pool_name             ,
                         const char           * peer_ip               ,
                         unsigned short         peer_port             ,
                         SXE_POOL_TCP_EVENT     event_ready_to_write  ,
                         SXE_IN_EVENT_CONNECTED event_connected       ,
                         SXE_IN_EVENT_READ      event_read            ,
                         SXE_IN_EVENT_CLOSE     event_close           ,
                         double                 initialization_timeout,
                         double                 response_timeout      ,
                         SXE_POOL_TCP_EVENT     event_timeout         ,
                         void                 * caller_info           )
{
    SXE_POOL_TCP * pool;

    SXEE88("sxe_pool_tcp_new_connect(concurrency=%d, pool_name=%s, peer_ip=0x%08x, peer_port=%hd, event_ready_to_write=%p, event_connected=%p, event_read=%p, event_close=%p",
           concurrency, pool_name, peer_ip, peer_port, event_ready_to_write, event_connected, event_read, event_close);
    pool = sxe_pool_tcp_new_internal(concurrency, pool_name, event_ready_to_write, event_connected, event_read, event_close,
                                     initialization_timeout, response_timeout, event_timeout, caller_info);
    SXEA11(pool != NULL, "Unable to create TCP pool '%s'", pool_name);

    pool->is_spawn  = SXE_FALSE;
    pool->peer_ip   = peer_ip;
    pool->peer_port = peer_port;

    SXEL83("Connecting via ramp %d sockets to peer %s:%d", concurrency, peer_ip, peer_port);
    connect_ramp(NULL, pool);

    SXER81("return pool=%p", pool);
    return pool;
}

SXE_POOL_TCP *
sxe_pool_tcp_new_spawn(unsigned               concurrency           ,
                       const char           * pool_name             ,
                       const char           * command               ,
                       const char           * arg1                  ,
                       const char           * arg2                  ,
                       SXE_POOL_TCP_EVENT     event_ready_to_write  ,
                       SXE_IN_EVENT_CONNECTED event_connected       ,
                       SXE_IN_EVENT_READ      event_read            ,
                       SXE_IN_EVENT_CLOSE     event_close           ,
                       double                 initialization_timeout,
                       double                 response_timeout      ,
                       SXE_POOL_TCP_EVENT     event_timeout         ,
                       void                 * caller_info           )

{
    SXE_POOL_TCP * pool;
    unsigned       i;

    SXEE88("sxe_pool_tcp_new_spawn(concurrency=%d, pool_name=%s, command='%s', arg1=%p, arg2=%p, event_ready_to_write=%p, event_read=%p, event_close=%p",
           concurrency, pool_name, command, arg1, arg2, event_ready_to_write, event_read, event_close);
    pool = sxe_pool_tcp_new_internal(concurrency, pool_name, event_ready_to_write, event_connected, event_read, event_close,
                                     initialization_timeout, response_timeout, event_timeout, caller_info);
    SXEA11(pool != 0, "Unable to create spawned TCP pool '%s'", pool_name);

    pool->is_spawn = SXE_TRUE;
    pool->command  = command;
    pool->arg1     = arg1;
    pool->arg2     = arg2;

    for (i = 0; i < concurrency; i++) {
        SXE_SPAWN_MAKE(&pool->nodes[i].previous_spawn);
        SXE_SPAWN_MAKE(&pool->nodes[i].current_spawn);
    }

    SXEL82("Connecting via ramp %u sockets to spawned command '%s'", concurrency, command);
    connect_ramp(NULL, pool);

    SXER81("return pool=%p", pool);
    return pool;
}

void
sxe_pool_tcp_delete(SXE * this, SXE_POOL_TCP * pool)
{
    enum SXE_POOL_TCP_STATE state;
    unsigned                item;

    SXEE81I("sxe_pool_tcp_delete(pool=%s)", SXE_POOL_TCP_GET_NAME(pool));
    SXE_UNUSED_ARGUMENT(this);
    SXE_POOL_TCP_ASSERT_CONSISTENT(pool);

    for (state = SXE_POOL_TCP_STATE_CONNECTING; state < SXE_POOL_TCP_STATE_NUMBER_OF_STATES; state++) {
        while ((item = sxe_pool_set_oldest_element_state(pool->nodes, state, SXE_POOL_TCP_STATE_UNCONNECTED)) != SXE_POOL_NO_INDEX) {
            SXE_USER_DATA(pool->nodes[item].sxe) = NULL;
            sxe_close(pool->nodes[item].sxe);
        }
    }

    sxe_pool_delete(pool->nodes);
    free(pool);
    SXER80I("return");
}

#endif
