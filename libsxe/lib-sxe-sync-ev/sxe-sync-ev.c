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

#include <limits.h>

#include "ev.h"
#include "sxe-pool.h"
#include "sxe-socket.h"
#include "sxe-sync-ev.h"

/* Todo: Should use SXE instead of sockets. Need pools of SXEs. */

static SXE_SYNC_EV *      sxe_sync_ev_pool;
static SXE_SOCKET         sxe_sync_ev_sock = SXE_SOCKET_INVALID;
static struct sockaddr_in sxe_sync_ev_addr;
static struct ev_io       sxe_sync_ev_io;
static void            (* sxe_sync_generic_event)(void *, void * user_data);

static void
sxe_sync_ev_read(EV_P_ ev_io * io, int revents)
{
    SXE_SYNC_EV   * handle; /* todo: don't pass pointers around using udp! */

    SXEE6("sxe_sync_ev_read(io=%p,revents=%d)", io, revents);
#if EV_MULTIPLICITY
    SXE_UNUSED_ARGUMENT(loop);
#endif
    SXE_UNUSED_ARGUMENT(io);
    SXE_UNUSED_ARGUMENT(revents);

    while(recvfrom(sxe_sync_ev_sock, (void *)&handle, sizeof(handle), 0, NULL, NULL) == sizeof(handle)) {
        (*sxe_sync_generic_event)(handle, handle->user_data);
    }

    SXEA1(sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EWOULDBLOCK), "Unexpected error receiving from sync socket: %s",
           sxe_socket_get_last_error_as_str());
    SXER6("return");
}

static int
sxe_sync_ev_socket(void)
{
    SXE_SOCKET sock;

    SXEE6("sxe_sync_ev_socket()");
    SXEA1((sock = socket(AF_INET, SOCK_DGRAM, 0)) != SXE_SOCKET_INVALID, "Error creating sync socket: %s",
           sxe_socket_get_last_error_as_str());
    SXEA1(sxe_socket_set_nonblock(sock, 1) >= 0, "socket %d: couldn't set non-blocking flag: %s", sock,
           sxe_socket_get_last_error_as_str());
    SXER6("return sock=%d", sock);
    return sock;
}

void
sxe_sync_ev_init(unsigned concurrency, void (*send_event)(void * sync, void * user_data))
{
    unsigned short port;

    SXEE6("sxe_sync_ev_init(concurrency=%u,send_event=%p)", concurrency, send_event);
    sxe_sync_ev_pool                 = sxe_pool_new("http_sync_ev", concurrency, sizeof(SXE_SYNC_EV), 2, SXE_POOL_OPTION_UNLOCKED);
    sxe_sync_ev_sock                 = sxe_sync_ev_socket();
    sxe_sync_generic_event           = send_event;
    sxe_sync_ev_addr.sin_family      = AF_INET;
    sxe_sync_ev_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    for (port = 1025; ; port++) {
        sxe_sync_ev_addr.sin_port = htons(port);

        if (bind(sxe_sync_ev_sock, (struct sockaddr *)&sxe_sync_ev_addr, sizeof(sxe_sync_ev_addr)) >= 0) {
            break;
        }

        SXEA1(port < USHRT_MAX, "Not able to bind any port between 1024 and %hu: %s", port,   /* Coverage Exclusion - Assert      */
               sxe_socket_get_last_error_as_str());
    }                                                                                          /* Coverage Exclusion - Not Reached */

    SXEL6("Listening on port %hu", port);
    ev_io_init(&sxe_sync_ev_io, sxe_sync_ev_read, _open_osfhandle(sxe_sync_ev_sock, 0), EV_READ);
    ev_io_start(ev_default_loop(0), &sxe_sync_ev_io);
    SXER6("return");
}

void *
sxe_sync_ev_new(void * user_data)
{
    unsigned id;

    SXEE6("sxe_sync_ev_new(user_data=%p)", user_data);
    SXEA1((id = sxe_pool_set_oldest_element_state(sxe_sync_ev_pool, 0, 1)) != SXE_POOL_NO_INDEX,
           "Could not allocate a sync object");
    sxe_sync_ev_pool[id].sock      = sxe_sync_ev_socket();
    sxe_sync_ev_pool[id].user_data = user_data;

    SXER6("return sync=%p", &sxe_sync_ev_pool[id]);
    return &sxe_sync_ev_pool[id];
}

void
sxe_sync_ev_post(void * sync_point)
{
    SXE_SYNC_EV * sync_ev = (SXE_SYNC_EV *)sync_point;

    SXEE6("sxe_sync_ev_post(sync_point=%p)", sync_point);
    SXEA1(sendto(sync_ev->sock, (MOCK_SOCKET_VOID *)&sync_point, sizeof(sync_point), 0, (struct sockaddr *)&sxe_sync_ev_addr,
                  sizeof(sxe_sync_ev_addr)) == sizeof(sync_point),
           "Can't send to sync_point listener port: %s", sxe_socket_get_last_error_as_str());
    SXER6("return");
}

void *
sxe_sync_ev_delete(void * sync_point)
{
    SXEE6("sxe_sync_ev_delete(sync_point=%p)", sync_point);
    sxe_pool_set_indexed_element_state(sxe_sync_ev_pool, (SXE_SYNC_EV *)sync_point - sxe_sync_ev_pool, 1, 0);
    SXER6("return NULL");
    return NULL;
}

