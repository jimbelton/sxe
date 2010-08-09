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

#ifndef __SXE_POOL_TCP_H__
#define __SXE_POOL_TCP_H__

#include <stdint.h>
#include "sxe.h"
#include "sxe-pool.h"
#include "sxe-spawn.h"

#define SXE_POOL_TCP_GET_NUMBER_IN_STATE(pool_tcp, state) sxe_pool_get_number_in_state((pool_tcp)->nodes, state)
#define SXE_POOL_TCP_GET_NAME(           pool_tcp)        sxe_pool_get_name((pool_tcp)->nodes)

struct SXE_POOL_TCP;

typedef void (*SXE_POOL_TCP_EVENT)(struct SXE_POOL_TCP * pool, void * called_info);

typedef struct {
    SXE                  * sxe;
    struct SXE_POOL_TCP  * tcp_pool;
    void                 * user_data;
    unsigned               failure_count;
    SXE_SPAWN              previous_spawn;
    SXE_SPAWN              current_spawn;
} SXE_POOL_TCP_NODE;

typedef struct SXE_POOL_TCP {
    SXE_BOOL               is_spawn;
    const char           * command;                  /* Either: Command to spawn       */
    const char           * arg1;                     /*         and arguments          */
    const char           * arg2;
    const char           * peer_ip;                  /* Or:     IP address             */
    int                    peer_port;                /*         and port to connect to */
    SXE_POOL_TCP_NODE    * nodes;
    unsigned               concurrency;
    unsigned               count_ready_to_write_events_queued;
    SXE_POOL_TCP_EVENT     event_ready_to_write;
    SXE_IN_EVENT_CONNECTED event_connected;
    SXE_IN_EVENT_READ      event_read;
    SXE_IN_EVENT_CLOSE     event_close;
    SXE_POOL_TCP_EVENT     event_timeout;
    void                 * caller_info;
    SXE_BOOL               has_initialization;
} SXE_POOL_TCP;

#include "sxe-pool-tcp-proto.h"

#endif /* __SXE_POOL_TCP_H__ */
