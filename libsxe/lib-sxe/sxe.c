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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef WINDOWS_NT
#include <io.h>
#include <winsock2.h>
#else
#include <netdb.h>
#include <netinet/tcp.h>
#endif

#include "ev.h"
#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-socket.h"
#include "sxe-util.h"

/* TODO: change sxld so that the udp packet is read only once by sxld (and not sxe and sxld) */
#define SXE_WANT_CALLER_READS_UDP 0

#define SXE_ID_NEXT_NONE ~0U

int                     sxe_caller_read_udp_length; /* if is_caller_reads_udp caller returns length read here */

static struct ev_loop * sxe_main_loop         = NULL;
static unsigned         sxe_extra_size        = 0;
static unsigned         sxe_array_total       = 0;
static unsigned         sxe_array_used        = 0;
static SXE            * sxe_array             = NULL;
static unsigned         sxe_free_head         = SXE_ID_NEXT_NONE;
static unsigned         sxe_free_tail         = SXE_ID_NEXT_NONE;
static unsigned         sxe_stat_total_accept = 0;
static unsigned         sxe_stat_total_read   = 0;

/**
 * Called by each protocol plugin before initialization.
 */
void
sxe_register(unsigned number_of_connections, unsigned extra_size)
{
    SXEE82("sxe_register(number_of_connections=%u,extra_size=%u)", number_of_connections, extra_size);
    sxe_array_total += number_of_connections;

    /* TODO: sxe_register() should assert if sxe_init() has already been called (because it signifies registration is too late) */

    if (extra_size > sxe_extra_size) {
        sxe_extra_size = extra_size;
    }

    SXER80("return");
}

/* Under Windows, stub out signal handling; libev doesn't grab the SIGCHLD signal in Windows
 * TODO: Make an abstract function for starting libev that hides this.
 */
#ifdef _WIN32

#define SIGCHLD 0

struct sigaction {unsigned dummy;};

static int
sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    return 0;
}

#endif

/**
 * Called at initialization after all plugins have registered.
 */
SXE_RETURN
sxe_init(void)
{
    SXE_RETURN       result = SXE_RETURN_ERROR_INTERNAL;
    unsigned         i;
    struct sigaction sigaction_saved;

    SXEE80("sxe_init()");
    sxe_socket_init();

    /* TODO: Check that sxe_array_total is smaller than system ulimit -n */

    SXEL82("calloc(%d, %d)", sizeof(SXE) + sxe_extra_size, sxe_array_total);
    sxe_array = calloc(sizeof(SXE) + sxe_extra_size, sxe_array_total);

    if (sxe_array == NULL) {
        SXEL12("calloc() failed for %d connections: %s", sxe_array_total, strerror(errno));
        result = SXE_RETURN_ERROR_ALLOC;
        goto SXE_EARLY_OUT;
    }

    for (i = 0; i < sxe_array_total; ++i) {
        sxe_array[i].id      =  i;
        sxe_array[i].id_next = (i + 1);
        sxe_array[i].socket  = SXE_SOCKET_INVALID;
    }

    sxe_free_head                    = 0;
    sxe_free_tail                    = sxe_array_total - 1;
    sxe_array[sxe_free_tail].id_next = SXE_ID_NEXT_NONE;

    if (!sxe_main_loop) {
        /* Initialize libev, but don't let it take over the SIGCHLD signal.
         */
        SXEV81(sigaction(SIGCHLD, NULL, &sigaction_saved), == 0, "Unable to save SIGCHLD's sigaction: %s", strerror(errno));
        sxe_main_loop = ev_default_loop(0);
        SXEV81(sigaction(SIGCHLD, &sigaction_saved, NULL), == 0, "Unable to restore SIGCHLD's sigaction: %s", strerror(errno));
        SXEL82("got main loop = %p, backend=%s", (void*)sxe_main_loop,
            ev_backend(sxe_main_loop) == EVBACKEND_SELECT  ? "EVBACKEND_SELECT"  :
            ev_backend(sxe_main_loop) == EVBACKEND_POLL    ? "EVBACKEND_POLL"    :
            ev_backend(sxe_main_loop) == EVBACKEND_EPOLL   ? "EVBACKEND_EPOLL"   :
            ev_backend(sxe_main_loop) == EVBACKEND_KQUEUE  ? "EVBACKEND_KQUEUE"  :
            ev_backend(sxe_main_loop) == EVBACKEND_DEVPOLL ? "EVBACKEND_DEVPOLL" :
            ev_backend(sxe_main_loop) == EVBACKEND_PORT    ? "EVBACKEND_PORT"    : "Unknown backend!" );
    }

    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:

    SXER81("return %s", sxe_return_to_string(result));
    return result;
}

SXE_RETURN
sxe_fini(void)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    SXEE80("sxe_fini()");

    if (sxe_array == NULL) {
        SXEL80("sxe_fini() called before sxe_init()");
        goto SXE_ERROR_OUT;
    }

    free(sxe_array);

    sxe_extra_size        = 0;
    sxe_array_total       = 0;
    sxe_array_used        = 0;
    sxe_array             = NULL;
    sxe_free_head         = SXE_ID_NEXT_NONE;
    sxe_free_tail         = SXE_ID_NEXT_NONE;
    sxe_stat_total_accept = 0;
    sxe_stat_total_read   = 0;

    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:

    SXER81("return %d", result);
    return result;
}

static SXE *
sxe_new_internal(SXE *                  this,
                 struct sockaddr_in *   local_addr,
                 SXE_IN_EVENT_CONNECTED in_event_connected,
                 SXE_IN_EVENT_READ      in_event_read,
                 SXE_IN_EVENT_CLOSE     in_event_close,
                 SXE_BOOL               is_tcp)
{
    SXE * that = NULL;

    SXEE82I("sxe_new_internal(sockaddr_in=%08x:%hu)", local_addr->sin_addr.s_addr, ntohs(local_addr->sin_port));

    if (sxe_free_head == SXE_ID_NEXT_NONE) {
        SXEL30I("Warning: ran out of connections, concurrency too high");
        goto SXE_EARLY_OUT;
    }

    that                      = &sxe_array[sxe_free_head];
    memcpy(&that->local_addr, local_addr, sizeof(that->local_addr));
    that->is_tcp              = is_tcp;
    that->is_accept_oneshot   = 0;
    that->is_caller_reads_udp = 0;
    that->in_event_connected  = in_event_connected;
    that->in_event_read       = in_event_read;
    that->in_event_close      = in_event_close;
    that->socket_as_fd        = -1;
    sxe_free_head             = that->id_next;
    that->id_next             = SXE_ID_NEXT_NONE;
    sxe_array_used++;
    SXEA10I((sxe_array_used < sxe_array_total) || (sxe_free_head == SXE_ID_NEXT_NONE),
            "All sxes are used, but free head is not none");

    if (sxe_free_head == SXE_ID_NEXT_NONE) {
        SXEA10I(sxe_free_tail == that->id, "Last item in list not pointed to by both head and tail");
        sxe_free_tail = SXE_ID_NEXT_NONE;
    }

SXE_EARLY_OR_ERROR_OUT:
    SXER82I("return id=%d // new connection, used=%u", (that != NULL ? (int)that->id : -1), sxe_array_used);
    return that;
}

static SXE *
sxe_new(SXE *                  this,
        const char *           local_ip,
        unsigned short         local_port,
        SXE_IN_EVENT_CONNECTED in_event_connected,
        SXE_IN_EVENT_READ      in_event_read,
        SXE_IN_EVENT_CLOSE     in_event_close,
        SXE_BOOL               is_tcp)
{
    SXE *               that = NULL;
    struct sockaddr_in  local_addr;

    SXEE83I("sxe_new(local_ip=%s,local_port=%hu,is_tcp=%s)", local_ip, local_port, is_tcp ? "TRUE" : "FALSE");

    memset(&local_addr,   0x00, sizeof(this->local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port   = htons(local_port);

    if (strcmp(local_ip, "INADDR_ANY") == 0) {
        local_addr.sin_addr.s_addr    = htonl(INADDR_ANY);
    }
    else {
        local_addr.sin_addr.s_addr    = inet_addr(local_ip);
    }

    that = sxe_new_internal(this, &local_addr, in_event_connected, in_event_read, in_event_close, is_tcp);

SXE_EARLY_OR_ERROR_OUT:
    SXER83I("return that=%p; // id=%u, port=%hu", that, (that == NULL ? ~0U : that->id),
            (that == NULL ? 0 : ntohs(that->local_addr.sin_port)) );
    return that;
}

SXE *
sxe_new_tcp(SXE *                  this,
            const char *           local_ip,
            unsigned short         local_port,
            SXE_IN_EVENT_CONNECTED in_event_connected,
            SXE_IN_EVENT_READ      in_event_read,
            SXE_IN_EVENT_CLOSE     in_event_close)
{
    return sxe_new(this, local_ip, local_port, in_event_connected, in_event_read, in_event_close, SXE_TRUE);
}

SXE *
sxe_new_udp(SXE * this, const char * local_ip, unsigned short local_port, SXE_IN_EVENT_READ in_event_read)
{
    return sxe_new(this, local_ip, local_port, NULL, in_event_read, NULL, SXE_FALSE);
}

#ifdef WINDOWS_NT
#define SXE_WINAPI_CAST_CHAR_STAR (char *)
#else
#define SXE_WINAPI_CAST_CHAR_STAR (void *)
#endif

static void
sxe_set_socket_options(SXE * this, int sock)
{
    struct linger linger_option;
    int           flags;
    int           so_rcvbuf_size;
    SXE_SOCKLEN_T so_rcvbuf_size_length = sizeof(so_rcvbuf_size);
    //debug int           so_sndbuf_size;
    //debug SXE_SOCKLEN_T so_sndbuf_size_length = sizeof(so_sndbuf_size);

    SXEE81I("sxe_set_socket_options(sock=%d)", sock);

    SXEV82I(sxe_socket_set_nonblock(sock, 1), >= 0, "socket %d: couldn't set non-blocking flag: %s", sock, sxe_socket_get_last_error_as_str());

    /* Windows Sockets implementation of SO_REUSEADDR seems to be badly broken.
     */

    if (this->is_tcp) {

#ifndef WINDOWS_NT
        flags = 1;
        SXEV83I(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, SXE_WINAPI_CAST_CHAR_STAR &flags, sizeof(flags)),
            >= 0, "socket %d: couldn't set reuse address flag: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
#endif
        //do we need this? flags = 1;
        //do we need this? SXEV83I(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, SXE_WINAPI_CAST_CHAR_STAR &flags, sizeof(flags)),
        //do we need this?        >= 0, "socket %d: couldn't set keepalive flag: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

        linger_option.l_onoff  = 0;
        linger_option.l_linger = 0;
        SXEV83I(setsockopt(sock, SOL_SOCKET, SO_LINGER, SXE_WINAPI_CAST_CHAR_STAR &linger_option, sizeof(linger_option)),
               >= 0, "socket %d: couldn't set linger to 0 secs: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

        flags = 1;
        SXEV83I(setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, SXE_WINAPI_CAST_CHAR_STAR &flags, sizeof(flags)),
               >= 0, "socket %d: couldn't set TCP no delay flag: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
    }
    else {
        //debug SXEV83I(getsockopt(sock, SOL_SOCKET, SO_RCVBUF, SXE_WINAPI_CAST_CHAR_STAR &so_rcvbuf_size, &so_rcvbuf_size_length), >= 0, "socket %d: couldn't get SO_RCVBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //debug SXEV83I(getsockopt(sock, SOL_SOCKET, SO_SNDBUF, SXE_WINAPI_CAST_CHAR_STAR &so_sndbuf_size, &so_sndbuf_size_length), >= 0, "socket %d: couldn't get SO_SNDBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //debug SXEL12I("SO_RCVBUF %d, SO_SNDBUF %d", so_rcvbuf_size, so_sndbuf_size);

#define SXE_UDP_SO_RCVBUF_SIZE (32 * 1024 * 1024)

        so_rcvbuf_size = SXE_UDP_SO_RCVBUF_SIZE;
        SXEV83I(setsockopt(sock, SOL_SOCKET, SO_RCVBUF, SXE_WINAPI_CAST_CHAR_STAR &so_rcvbuf_size, sizeof(so_rcvbuf_size)), >= 0, "socket %d: couldn't get SO_RCVBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //do we need this? so_sndbuf_size = (128 * 1024 * 1024);
        //do we need this? SXEV83I(setsockopt(sock, SOL_SOCKET, SO_SNDBUF, SXE_WINAPI_CAST_CHAR_STAR &so_sndbuf_size, sizeof(so_sndbuf_size)), >= 0, "socket %d: couldn't get SO_SNDBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

        so_rcvbuf_size = 0;
        SXEV83I(getsockopt(sock, SOL_SOCKET, SO_RCVBUF, SXE_WINAPI_CAST_CHAR_STAR &so_rcvbuf_size, &so_rcvbuf_size_length), >= 0, "socket %d: couldn't get SO_RCVBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        if (so_rcvbuf_size < SXE_UDP_SO_RCVBUF_SIZE) {
            SXEL33I("Warning: could not set SO_RCVBUF to %u; instead it is set to %u; consider e.g. sysctl -w net.core.rmem_max=%u; continuing but packet loss will occur at higher loads", SXE_UDP_SO_RCVBUF_SIZE, so_rcvbuf_size, SXE_UDP_SO_RCVBUF_SIZE); /* COVERAGE EXCLUSION */
        }

        //debug SXEV83I(getsockopt(sock, SOL_SOCKET, SO_RCVBUF, SXE_WINAPI_CAST_CHAR_STAR &so_rcvbuf_size, &so_rcvbuf_size_length), >= 0, "socket %d: couldn't get SO_RCVBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //debug SXEV83I(getsockopt(sock, SOL_SOCKET, SO_SNDBUF, SXE_WINAPI_CAST_CHAR_STAR &so_sndbuf_size, &so_sndbuf_size_length), >= 0, "socket %d: couldn't get SO_SNDBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //debug SXEL12I("SO_RCVBUF %d, SO_SNDBUF %d", so_rcvbuf_size, so_sndbuf_size);
    }

SXE_EARLY_OR_ERROR_OUT:

    SXER80I("return");
}

#define SXE_IO_CB_READ_MAXIMUM 64

static void
sxe_io_cb_read(EV_P_ ev_io * io, int revents)
{
    SXE         * this           = (SXE *)io;
    SXE_SOCKLEN_T peer_addr_size = sizeof(this->peer_addr);
    int           length;
    int           reads_remaining_this_event = SXE_IO_CB_READ_MAXIMUM;

#if EV_MULTIPLICITY
    SXE_UNUSED_PARAMETER(loop);
#endif

    SXEE83I("sxe_io_cb_read(this=%p, revents=%u) // socket=%d", this, revents, this->socket);

    if (this->socket == SXE_SOCKET_INVALID) {
        SXEL10I("ERROR: Unexpected read event on closed SXE object");
        goto SXE_EARLY_OUT;
    }

    if (revents == EV_READ)
    {
SXE_TRY_AND_READ_AGAIN:
        if (this->is_tcp) {
            /* Use recv(), not read() for WSA compatibility
             */
            length = recv    (this->socket, this->in_buf + this->in_total, sizeof(this->in_buf) - this->in_total, 0);
        }
#if SXE_WANT_CALLER_READS_UDP
        else if (this->is_caller_reads_udp) {
            do {
                (*this->in_event_read)(this, 0);
            } while (sxe_caller_read_udp_length > 0);

            length = sxe_caller_read_udp_length;
        }
#endif
        else {
            length = recvfrom(this->socket, this->in_buf + this->in_total, sizeof(this->in_buf) - this->in_total, 0,
                              (struct sockaddr *)&this->peer_addr, &peer_addr_size);
            SXEA81I(peer_addr_size == sizeof(this->peer_addr), "Peer address is not an IPV4 address (peer_addr_size=%d)",
                    peer_addr_size);
            SXEL83I("Read %d bytes from peer IP %u:%hu", length, inet_ntoa(this->peer_addr.sin_addr),
                    ntohs(this->peer_addr.sin_port));
        }

        if (length > 0)
        {
            SXED90I(this->in_buf + this->in_total, length);

            this->in_total += length;

            /* for now... quick and dirty statistics */
            sxe_stat_total_read++;

            //debug if ((sxe_stat_total_read % 5000) == 0) {
            //debug     SXEL52I("sxe_stat_total_read=%u, sxe_stat_total_accept=%u", sxe_stat_total_read, sxe_stat_total_accept); /* COVERAGE EXCLUSION: Statistics */
            //debug }

            (*this->in_event_read)(this, length);

            reads_remaining_this_event --;

            if (!this->is_tcp && reads_remaining_this_event) {
                goto SXE_TRY_AND_READ_AGAIN;
            }
        }
        else {
            if (length < 0) {
                switch (sxe_socket_get_last_error()) {

                case SXE_SOCKET_ERROR(EWOULDBLOCK): // EAGAIN
                    SXEL81I("Socket %d is not ready", this->socket);
                    goto SXE_EARLY_OUT;

                /* Expected error codes:
                 */
                case SXE_SOCKET_ERROR(ECONNRESET):
                    SXEL83I("Failed to read from socket %d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
                    break;

                /* Should never get here.
                 */
                default:
                    SXEL23I("Failed to read from socket %d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
                }
            }

            /* Under Linux, a received disconnect request may lead to a zero byte read.
             */
            else {
                SXEL81I("Failed to read from socket %d: (0) zero bytes read - disconnect", this->socket);
            }

            if (this->is_tcp) {
                /* Close the SXE object before calling the close callback.  This allows the SXE object to be inspected (e.g. to
                 * determine the value of its user data) and to be immediately reused in the callback.
                 */
                sxe_close(this);

                if (this->in_event_close != NULL) {
                    (*this->in_event_close)(this);
                }
                else {
                    SXEL90I("No close event callback: cannot report");
                }
            }
        }
    }

SXE_EARLY_OR_ERROR_OUT:
    SXER80I("return");
}

static void
sxe_io_cb_accept(EV_P_ ev_io * io, int revents)
{
    SXE              * this           = (SXE *)io;
    SXE              * that           = NULL;
    int                that_socket    = SXE_SOCKET_INVALID;
    struct sockaddr_in peer_addr;
    SXE_SOCKLEN_T      peer_addr_size = sizeof(peer_addr);

#if EV_MULTIPLICITY
    SXE_UNUSED_PARAMETER(loop);
#endif
    SXE_UNUSED_PARAMETER(revents);

    SXEE83I("sxe_io_cb_accept(this=%p, revents=%u) // socket=%d", this, revents, this->socket);
    SXEA80I(revents == EV_READ, "sxe_io_cb_accept: revents is not EV_READ");

    while (this != that) {
        /* TODO: Track SXE & open socket count and disable / enable accept() event as appropriate */
        if ((that_socket = accept(this->socket, (struct sockaddr *)&peer_addr, &peer_addr_size)) < 0)
        {
            if (sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EWOULDBLOCK)) {
                SXEL81I("No more sockets to accept on socket %d", this->socket);
                goto SXE_EARLY_OUT;
            }

            SXEL23I("Error accepting on socket %d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
            goto SXE_ERROR_OUT;
        }

        SXEL85I("Accepted connection from ip=%d.%d.%d.%d:%hu",
                ntohl(peer_addr.sin_addr.s_addr) >> 24,
                (ntohl(peer_addr.sin_addr.s_addr) >> 16) & 0xff,
                (ntohl(peer_addr.sin_addr.s_addr) >> 8) & 0xff,
                ntohl(peer_addr.sin_addr.s_addr) & 0xff,
                ntohs(peer_addr.sin_port));

        sxe_set_socket_options(this, that_socket);

        SXEL80I("Checking whether this is a one-shot socket");

        if (this->is_accept_oneshot) {
            SXEL80I("Close listening socket, and replace with accepted socket");
            ev_io_stop(sxe_main_loop, &this->io);
            CLOSESOCKET(this->socket);
            close(this->socket_as_fd);
            this->socket_as_fd = -1;
            that = this;
        }
        else {
            SXEL80I("Creating new sxe internal object thingy");
            that = sxe_new_internal(this,
                                    &this->local_addr,
                                    this->in_event_connected,
                                    this->in_event_read,
                                    this->in_event_close,
                                    this->is_tcp);

            if (that == NULL) {
                SXEL31I("Warning: failed to allocate a connection from socket %d: out of connections.", this->socket);
                goto SXE_ERROR_OUT;
            }
        }

        that->socket       = that_socket;
        that->socket_as_fd = _open_osfhandle(that_socket, 0);
        memcpy(&that->peer_addr, &peer_addr, sizeof(that->peer_addr));

        SXEL82I("add accepted connection to watch list, socket==%d, socket_as_fd=%d", that_socket, that->socket_as_fd);
        ev_io_init((struct ev_io*)&that->io, sxe_io_cb_read, that->socket_as_fd, EV_READ);
        ev_io_start(sxe_main_loop, (struct ev_io*)&that->io);

        sxe_stat_total_accept ++;

        if (that->in_event_connected != NULL) {
            (*that->in_event_connected)(that);
        }
    } /* while (this != that) */

    goto SXE_EARLY_OUT;

SXE_ERROR_OUT:
    if (that_socket != SXE_SOCKET_INVALID) {
        CLOSESOCKET(that_socket);
    }

SXE_EARLY_OUT:
    SXER80I("return");
}

static void
sxe_io_cb_connect(EV_P_ ev_io * io, int revents)
{
    SXE         * this                    = (SXE *)io;
    int           error_indication;
    SXE_SOCKLEN_T error_indication_length = sizeof(error_indication);

    SXEE86I("sxe_io_cb_connect(this=%p, revents=%u) // socket=%d, EV_READ=0x%x, EV_WRITE=0x%x, EV_ERROR=0x%x", this, revents,
            this->socket, EV_READ, EV_WRITE, EV_ERROR);

#if EV_MULTIPLICITY
    SXE_UNUSED_PARAMETER(loop);
#endif
    SXE_UNUSED_PARAMETER(revents);

    if (getsockopt(this->socket, SOL_SOCKET, SO_ERROR, (void*)&error_indication, &error_indication_length) == SXE_SOCKET_ERROR_OCCURRED) {
        SXEL22I("Failed to get error indication (socket=%d): %s", this->socket, sxe_socket_get_last_error_as_str());
        goto SXE_ERROR_OUT;
    }

    SXEA81I(error_indication_length == sizeof(error_indication), "Unexpected size of result returned by getsockopt(): %d bytes", error_indication_length);

    if (error_indication != 0) {
        SXEL51I("Failed to connect: %s", sxe_socket_error_as_str(error_indication));
        goto SXE_ERROR_OUT;
    }

    SXEL81I("setup read event to call sxe_io_cb_read callback (socket==%d)", this->socket);
    ev_io_stop(sxe_main_loop, &this->io);
    ev_io_init(&this->io, sxe_io_cb_read, this->socket_as_fd, EV_READ);
    ev_io_start(sxe_main_loop, &this->io);

    SXEL80I("connection complete; notify sxe application");

    if (this->in_event_connected != NULL) {
        (*this->in_event_connected)(this);
    }

    goto SXE_EARLY_OUT;

SXE_ERROR_OUT:
    if (this->in_event_close != NULL) {
        (*this->in_event_close)(this);
    }

    sxe_close(this);

SXE_EARLY_OUT:
    SXER80I("return");
}

SXE_RETURN
sxe_listen(SXE * this)
{
    SXE_RETURN    result = SXE_RETURN_ERROR_INTERNAL;
    int           socket_listen = SXE_SOCKET_INVALID;
    SXE_SOCKLEN_T address_len = sizeof(this->local_addr);

    SXEE81I("sxe_listen(this=%p)", this);
    SXEA80I(this != NULL, "object pointer is NULL");

    if (this->socket >= 0) {
        SXEL21I("Listener is already in use (socket=%d)", this->socket);
        goto SXE_EARLY_OUT;
    }

    if ((socket_listen = socket(AF_INET, (this->is_tcp ? SOCK_STREAM : SOCK_DGRAM), 0)) == SXE_SOCKET_INVALID) {
        SXEL22I("Error creating listening socket: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        goto SXE_EARLY_OUT;
    }

    SXEL81I("listen socket is %d", socket_listen);

    sxe_set_socket_options(this, socket_listen);

    if (bind(socket_listen, (struct sockaddr *)&this->local_addr, sizeof(this->local_addr)) < 0) {
        int error = sxe_socket_get_last_error();    /* Save due to Microsoft resetting in inet_ntoa */

        SXEL24I("Error binding address to listening socket address=%s, port is %hu: (errno=%d) %s",
                inet_ntoa(this->local_addr.sin_addr), ntohs(this->local_addr.sin_port), error, sxe_socket_error_as_str(error));
        sxe_socket_set_last_error(error);

        if (error == SXE_SOCKET_ERROR(EADDRINUSE)) {
            result = SXE_RETURN_ERROR_ADDRESS_IN_USE;
        }

        goto SXE_EARLY_OUT;
    }

    if (getsockname(socket_listen, (struct sockaddr *)&this->local_addr, &address_len) < 0 ) {
        SXEL21I("ERROR: getsockname(): %s\n", sxe_socket_get_last_error_as_str()); /* COVERAGE EXCLUSION : TODO - when we are able to mock */
        goto SXE_ERROR_OUT; /* COVERAGE EXCLUSION : TODO - when we are able to mock */
    }

    if (this->is_tcp && (listen(socket_listen, SOMAXCONN) < 0)) {
        SXEL22I("Error listening on socket: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        goto SXE_EARLY_OUT;
    }

    this->socket       = socket_listen;
    this->socket_as_fd = _open_osfhandle(socket_listen, 0);
    socket_listen      = SXE_SOCKET_INVALID;
    SXEL85I("connection id of new listen socket is %d, socket_as_fd=%d, address=%s, port=%hu, SOMAXCONN=%d", this->id,
            this->socket_as_fd, inet_ntoa(this->local_addr.sin_addr), ntohs(this->local_addr.sin_port), SOMAXCONN);

    ev_io_init((struct ev_io*)&this->io, (this->is_tcp ? sxe_io_cb_accept : sxe_io_cb_read), this->socket_as_fd, EV_READ);
    ev_io_start(sxe_main_loop, (struct ev_io*)&this->io);

    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    if (socket_listen != SXE_SOCKET_INVALID) {
        SXEL81I("socket %d is unusable, so must be closed", socket_listen);
        CLOSESOCKET(socket_listen);
    }

    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

SXE_RETURN
sxe_connect(SXE * this, const char * peer_ip, unsigned short peer_port)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    int        that_socket;

    SXEE84I("sxe_connect(this=%p, peer_ip==%s, peer_port=%hu) // socket=%d", this, peer_ip, peer_port, this->socket);
    SXEA80I(this != NULL, "connection pointer is NULL");

    if (this->socket != SXE_SOCKET_INVALID) {
        SXEL21I("Connection is already in use (socket=%d)", this->socket);
        result = SXE_RETURN_ERROR_ALREADY_CONNECTED;
        goto SXE_EARLY_OUT;
    }

    SXEL80I("TODO: Add a timeout parameter");

    if ((that_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == SXE_SOCKET_INVALID) {
        SXEL22I("error creating socket: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        goto SXE_ERROR_OUT;
    }

    sxe_set_socket_options(this, that_socket);

    /* If the local address is not INADDR_ANY:0, bind it to the socket.
     */

    if ((this->local_addr.sin_addr.s_addr != htonl(INADDR_ANY)) || (this->local_addr.sin_port != 0)) {
        SXEL81I("Connecting using local TCP port: %hu", ntohs(this->local_addr.sin_port));

        if (bind(that_socket, (struct sockaddr *)&this->local_addr, sizeof(this->local_addr)) == SXE_SOCKET_ERROR_OCCURRED) {
            if (sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EADDRINUSE)) {
                SXEL33I("Can't connect to local TCP port: %hu (%d) %s", ntohs(this->local_addr.sin_port),
                        sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
                result = SXE_RETURN_ERROR_ADDRESS_IN_USE;
            }
            else {
                SXEL22I("Unexpected error calling bind: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
            }

            goto SXE_ERROR_OUT;
        }
    }
    else {
        SXEL80I("Connecting using local TCP port: randomly allocated by the stack");
    }

    memset(&this->peer_addr,           0x00, sizeof(this->peer_addr));
    this->peer_addr.sin_family       = AF_INET;
    this->peer_addr.sin_port         = htons(peer_port);
    this->peer_addr.sin_addr.s_addr  = inet_addr(peer_ip);

    /* Connect non-blocking.
     */

    SXEL82I("add connection to watch list, socket==%d, socket_as_fd=%d", that_socket, _open_osfhandle(that_socket, 0));
    ev_io_init((struct ev_io*)&this->io, sxe_io_cb_connect, _open_osfhandle(that_socket, 0), EV_WRITE);
    ev_io_start(sxe_main_loop, &this->io);

    if ((connect(that_socket, (struct sockaddr*)&this->peer_addr, sizeof(this->peer_addr)) == SXE_SOCKET_ERROR_OCCURRED)
            && (sxe_socket_get_last_error() != SXE_SOCKET_ERROR(EINPROGRESS))
            && (sxe_socket_get_last_error() != SXE_SOCKET_ERROR(EWOULDBLOCK))) {
        SXEL22I("connect failed: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        ev_io_stop(sxe_main_loop, &this->io);
        goto SXE_ERROR_OUT;
    }

    SXEL83I("Connection to server: %s:%hu: %s", peer_ip, peer_port,
            ((sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EINPROGRESS) || sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EWOULDBLOCK)) ? "in progress" : "completed"));
    this->socket       = that_socket;
    this->socket_as_fd =_open_osfhandle(that_socket, 0);

#if SXE_DEBUG
    /* Display the local socket's bound port and IP */
    {
        struct sockaddr_in local_addr;
        SXE_SOCKLEN_T      address_len = sizeof(local_addr);

        if (getsockname(that_socket, (struct sockaddr *)&local_addr, &address_len) >= 0 ) {
            SXEL87I("Connection id of new connecting socket is %d, socket_as_fd=%d, ip=%d.%d.%d.%d:%hu", this->id,
                     this->socket_as_fd                             ,
                     ntohl(local_addr.sin_addr.s_addr) >> 24        ,
                    (ntohl(local_addr.sin_addr.s_addr) >> 16) & 0xff,
                    (ntohl(local_addr.sin_addr.s_addr) >> 8 ) & 0xff,
                     ntohl(local_addr.sin_addr.s_addr)        & 0xff,
                     ntohs(local_addr.sin_port       )              );
        }
    }
#endif

    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

/* TODO: new sxe_write() parameter to auto close after writing all data */
/* TODO: sxe_write() auto configures write_cb() if didn't write all data */

SXE_RETURN
sxe_write_to(SXE * this, const void * buf, unsigned size, const struct sockaddr_in * dest_addr)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    int        ret;

    SXEA80I(this != NULL,  "sxe_write_to(): connection pointer is NULL");
    SXEA80I(!this->is_tcp, "sxe_write_to(): connection is a udp object");
    SXEE85I("sxe_write_to(this=%p,size=%u,sockaddr_in=%s:%hu) // socket=%d", this, size, inet_ntoa(dest_addr->sin_addr),
            ntohs(dest_addr->sin_port), this->socket);
    SXED90I(buf, size);

    if ((ret = sendto(this->socket, buf, size, 0, (const struct sockaddr *)dest_addr, sizeof(*dest_addr))) != (int)size) {
        if (ret >= 0) {
            SXEL23I("Only %d of %u bytes written to socket %d", ret, size, this->socket);   /* COVERAGE EXCLUSION: Logging for UDP truncation on sendto */
        }
        else {
            SXEL23I("Error writing to socket %d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        }

        goto SXE_ERROR_OUT;
    }

    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

SXE_RETURN
sxe_write(SXE * this, const void * buf, unsigned size)
{
    SXE_RETURN  result = SXE_RETURN_ERROR_INTERNAL;
    int         ret;

    SXEA80I(this != NULL, "connection pointer is NULL");
    SXEA80I(this->is_tcp, "connection pointer is a tcp connection");
    SXEE86I("sxe_write(buf='%c%c%c%c...', size=%u) // socket=%d",
        *((const char *)buf + 0),
        *((const char *)buf + 1),
        *((const char *)buf + 2),
        *((const char *)buf + 3),
        size,
        this->socket);
    SXED90I(buf, size);

    if (this->socket == SXE_SOCKET_INVALID) {
        SXEL20I("Send on a disconnected socket");
        result = SXE_RETURN_ERROR_NO_CONNECTION;
        goto SXE_ERROR_OUT;
    }

    /* Use send(), not write(), for WSA compatibility
     */
    if ((ret = send(this->socket, buf, size, 0)) != (int)size) {
        /* TODO: Need to support TCP flow control
         */
        if (ret >= 0) {
            SXEL23I("Only %d of %u bytes written to socket %d", ret, size, this->socket);   /* COVERAGE EXCLUSION: Logging for TCP partial write */
        }
        else {
            SXEL23I("Error writing to socket %d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

            if ((sxe_socket_get_last_error() == SXE_SOCKET_ERROR(ECONNRESET  ))
             || (sxe_socket_get_last_error() == SXE_SOCKET_ERROR(ECONNREFUSED))
             || (sxe_socket_get_last_error() == SXE_SOCKET_ERROR(ENOTCONN)))        /* Windows */
            {
                result = SXE_RETURN_ERROR_NO_CONNECTION;
            }
            else if (sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EWOULDBLOCK)) {
                result = SXE_RETURN_WARN_WOULD_BLOCK;
            }
        }

        goto SXE_ERROR_OUT;
    }

    SXEL82I("Wrote %u bytes to socket %d", size, this->socket);
    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

void
sxe_dump(SXE * this)
{
    unsigned i;

    SXE_UNUSED_PARAMETER(this);

    SXEE80I("sxe_dump()");
    SXEL80I("Dumping sxes");

    for (i = 0; i < sxe_array_total; i++) {
        SXEL84I("id=%d id_next=%d tail=%d head=%d", sxe_array[i].id, sxe_array[i].id_next, sxe_free_tail, sxe_free_head);
    }

    SXER80I("return");
}

/**
 * @param this    The SXE object to close
 *
 * Close a TCP listener or connection or a UDP port.
 *
 * @note This should not be called from a TCP close event callback. If a TCP connection is closed by the other end or the TCP
 *       stack, the SXE will be closed before the TCP close event is called back.
 */
SXE_RETURN
sxe_close(SXE * this)
{
    SXE_RETURN result = SXE_RETURN_OK;
    SXEA80I(this != NULL, "connection pointer is NULL");
    SXEE82I("sxe_close(this=%p) // this->socket=%d", this, this->socket);

    /* Close socket if open.
     */
    if (this->socket != SXE_SOCKET_INVALID) {
        /* Call CLOSESOCKET() as well as close() for Windows Sockets API compatibility.
         */
        CLOSESOCKET(this->socket);
        SXEL82I("About to close fd %d (this=%p)", this->socket_as_fd, this);

        if (this->socket_as_fd >= 0) {
            close(this->socket_as_fd);
            this->socket_as_fd = -1;
        }

#ifdef EV_MULTIPLICITY
        ev_io_stop(sxe_main_loop, &this->io);
#else
        ev_io_stop(&this->io);
#endif

        this->socket = SXE_SOCKET_INVALID;
    }

    SXEL84I("sxe_free_tail=%d, sxe_free_head=%d, this->id=%d, this->id_next=%d", sxe_free_tail, sxe_free_head, this->id, this->id_next);

    /* Close is idempotent with a warning.
     */
    if ((this->id_next != SXE_ID_NEXT_NONE) || (this->id == sxe_free_tail)) {
        SXEL30I("ignoring close of a free connection object");
        result = SXE_RETURN_WARN_ALREADY_CLOSED;
        goto SXE_EARLY_OUT;
    }

    SXEA80I(sxe_array_used > 0, "closing when no sxes are in use");

    if (sxe_free_head == SXE_ID_NEXT_NONE) {
        SXEL80I("sxe_free_head == SXE_ID_NEXT_NONE");
        sxe_free_head = this->id;
    }
    else {
        SXEA80I(sxe_array[sxe_free_tail].id_next == SXE_ID_NEXT_NONE, "Tail sxe has a next sxe");
        sxe_array[sxe_free_tail].id_next = this->id;
    }

    this->in_total = 0;
    sxe_free_tail  = this->id;
    sxe_array_used--;
    SXEL84I("sxe_free_tail=%d (me), sxe_free_head=%d, this->id=%d, this->id_next=%d (expect -1)", sxe_free_tail, sxe_free_head,
            this->id, this->id_next);

SXE_EARLY_OUT:
    SXER82I("return %s // used=%d", sxe_return_to_string(result), sxe_array_used);
    return result;
}

void
sxe_timer_init(ev_timer* timer, void(*cb)(EV_P_ ev_timer *w, int revents), double after, double repeat)
{
    SXEE84("sxe_timer_init(ev_timer=%p, cb=%p, after=%f, repeat=%p)", timer, cb, after, repeat);
    ev_timer_init(timer, cb, after, repeat);
    SXER80("return");
}

void sxe_timer_start(ev_timer* timer)
{
    SXEE81("sxe_timer_start(ev_timer=%p)", timer);
    ev_timer_start(sxe_main_loop, timer);
    SXER80("return");
}

void sxe_timer_stop(ev_timer* timer)
{
    SXEE81("sxe_timer_stop(ev_timer=%p)", timer);
    ev_timer_stop(sxe_main_loop, timer);
    SXER80("return");
}

void sxe_timer_set(ev_timer* timer, double after, double repeat)
{
    SXEE83("sxe_timer_set(ev_timer=%p, after=%f, repeat=%p)", timer, after, repeat);
    ev_timer_set(timer, after, repeat);
    SXER80("return");
}

void sxe_timer_again(ev_timer* timer)
{
    SXEE81("sxe_timer_again(ev_timer=%p)", timer);
    ev_timer_again(sxe_main_loop, timer);
    SXER80("return");
}
