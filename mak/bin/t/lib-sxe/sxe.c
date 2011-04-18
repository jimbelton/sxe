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

/* Under Linux, request _GNU_SOURCE extensions to support ucred structure
 */
#ifndef _WIN32
#define _GNU_SOURCE
#include <features.h>
#endif

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/* TODO: implement equivalent to UNIX domain sockets (pipes) and fd passing on Windows (using named pipes?) */

#ifdef _WIN32
#include <io.h>
#include <winsock2.h>

#else /* UNIX */
#include <linux/sockios.h>    /* For SIOCINQ: This appears to be Linux specific; under FreeBSD, use MSG_PEEK */
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/sendfile.h>
#include <sys/un.h>           /* For pipes (AKA UNIX domain sockets) on UNIX                                 */

#define PIPE_PATH_MAX sizeof(((struct sockaddr_un *)NULL)->sun_path)    /* Maximum size of a UNIX pipe path  */

typedef union SXE_CONTROL_MESSAGE_FD {
    struct cmsghdr alignment;
    char           control[CMSG_SPACE(sizeof(int))];
} SXE_CONTROL_MESSAGE_FD;
#endif

#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-pool.h"
#include "sxe-socket.h"
#include "sxe-util.h"

/* TODO: change sxld so that the udp packet is read only once by sxld (and not sxe and sxld) */

#define SXE_WANT_CALLER_READS_UDP 0

typedef enum SXE_STATE {
    SXE_STATE_FREE,
    SXE_STATE_USED,
    SXE_STATE_NUMBER_OF_STATES
} SXE_STATE;

int                     sxe_caller_read_udp_length;      /* If is_caller_reads_udp caller returns length read here */
struct ev_loop        * sxe_private_main_loop = NULL;    /* Private to this package; do not export via sxe.h       */

static unsigned         sxe_extra_size        = 0;
static unsigned         sxe_array_total       = 0;
static SXE            * sxe_array             = NULL;
static unsigned         sxe_stat_total_accept = 0;
static unsigned         sxe_stat_total_read   = 0;
static unsigned         sxe_has_been_inited   = 0;
static int              sxe_listen_backlog    = SOMAXCONN;

static inline bool sxe_is_free(SXE * this) {return sxe_pool_index_to_state(sxe_array, this->id) == SXE_STATE_FREE;}

/**
 * Check the validity of a SXE
 *
 * @param this = the SXE
 *
 * @return true if valid, false if not
 */

bool
sxe_is_valid(SXE * this)
{
    bool valid = false;

    SXEE81("sxe_is_valid(this=%p)", this);
    SXEL93("SXE array %p, total %u and size %u", sxe_array, sxe_array_total, sizeof(SXE) + sxe_extra_size);

    if ((this < sxe_array) || ((char *)this > (char *)sxe_array + (sxe_array_total - 1) * (sizeof(SXE) + sxe_extra_size))) {
        SXEL83("SXE pointer %p is not in the valid range %p to %p", this, sxe_array,
               (char *)sxe_array + (sxe_array_total - 1) * (sizeof(SXE) + sxe_extra_size));
        goto SXE_EARLY_OUT;
    }

    if (((char *)this - (char *)sxe_array) % (sizeof(SXE) + sxe_extra_size) != 0) {
        SXEL83("SXE pointer %p is not a multiple of %u bytes from the base address %p", this, sizeof(SXE) + sxe_extra_size,
               sxe_array);
        goto SXE_EARLY_OUT;
    }

    valid = true;

SXE_EARLY_OUT:
    SXER81("return %s", SXE_BOOL_TO_STR(valid));
    return valid;
}

/**
 * Called by each protocol plugin before initialization.
 */
void
sxe_register(unsigned number_of_connections, unsigned extra_size)
{
    SXEE82("sxe_register(number_of_connections=%u,extra_size=%u)", number_of_connections, extra_size);
    sxe_array_total += number_of_connections;

    SXEA10(sxe_has_been_inited == 0, "SXE has already been init()'d");

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
    SXE_UNUSED_PARAMETER(signum);
    SXE_UNUSED_PARAMETER(act   );
    SXE_UNUSED_PARAMETER(oldact);
    return 0;
}

#endif

/**
 * Initialize SXE objects
 *
 * @return    SXE_RETURN_OK
 *
 * @note      Call at initialization after all plugins have registered
 *
 * @exception Aborts if SXE is already initialized, sxe_register has not been called, or memory cannot be allocated
 */
SXE_RETURN
sxe_init(void)
{
    SXE_RETURN       result = SXE_RETURN_ERROR_INTERNAL;
    struct sigaction sigaction_saved;

    SXEE80("sxe_init()");
    SXEA10(!sxe_has_been_inited, "sxe_init: SXE is already initialized");
    SXEA10(sxe_array_total > 0,  "sxe_init: Error: sxe_register() must be called before sxe_init()");
    sxe_socket_init();

    /* TODO: Check that sxe_array_total is smaller than system ulimit -n */

    SXEL82("Allocating %u SXEs of %u bytes each", sxe_array_total, sizeof(SXE) + sxe_extra_size);
    sxe_array = sxe_pool_new("sxe_pool", sxe_array_total, sizeof(SXE) + sxe_extra_size, SXE_STATE_NUMBER_OF_STATES,
                             SXE_POOL_OPTION_TIMED);

    if (!sxe_private_main_loop) {
        /* Initialize libev, but don't let it take over the SIGCHLD signal.
         */
        SXEV81(sigaction(SIGCHLD, NULL, &sigaction_saved), == 0, "Unable to save SIGCHLD's sigaction: %s", strerror(errno));
        sxe_private_main_loop = ev_default_loop(0);
        SXEV81(sigaction(SIGCHLD, &sigaction_saved, NULL), == 0, "Unable to restore SIGCHLD's sigaction: %s", strerror(errno));
        SXEL82("got main loop = %p, backend=%s", (void*)sxe_private_main_loop,
            ev_backend(sxe_private_main_loop) == EVBACKEND_SELECT  ? "EVBACKEND_SELECT"  :
            ev_backend(sxe_private_main_loop) == EVBACKEND_POLL    ? "EVBACKEND_POLL"    :
            ev_backend(sxe_private_main_loop) == EVBACKEND_EPOLL   ? "EVBACKEND_EPOLL"   :
            ev_backend(sxe_private_main_loop) == EVBACKEND_KQUEUE  ? "EVBACKEND_KQUEUE"  :
            ev_backend(sxe_private_main_loop) == EVBACKEND_DEVPOLL ? "EVBACKEND_DEVPOLL" :
            ev_backend(sxe_private_main_loop) == EVBACKEND_PORT    ? "EVBACKEND_PORT"    : "Unknown backend!" );
    }

    sxe_has_been_inited = 1;
    result = SXE_RETURN_OK;
    SXER81("return %s", sxe_return_to_string(result));
    return result;
}

SXE_RETURN
sxe_fini(void)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    SXEE80("sxe_fini()");

    if (sxe_array == NULL) {
        SXEL80("sxe_fini(): called before successful sxe_init()");
        goto SXE_ERROR_OUT;
    }

    sxe_pool_delete(sxe_array);
    sxe_extra_size        = 0;
    sxe_array_total       = 0;
    sxe_array             = NULL;
    sxe_stat_total_accept = 0;
    sxe_stat_total_read   = 0;
    sxe_has_been_inited   = 0;
    result                = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:

    SXER81("return %d", result);
    return result;
}

static SXE *
sxe_new_internal(SXE                    * this              ,
                 struct sockaddr_in     * local_addr        ,
                 SXE_IN_EVENT_CONNECTED   in_event_connected,
                 SXE_IN_EVENT_READ        in_event_read     ,
                 SXE_IN_EVENT_CLOSE       in_event_close    ,
                 SXE_BOOL                 is_stream            ,
                 const char             * path              )
{
    unsigned id;
    SXE    * that = NULL;

    SXEE87I("sxe_new_internal(local_addr=%08x:%hu,in_event_connected=%p,in_event_read=%p,in_event_close=%p,is_stream=%s,path=%s)",
            local_addr->sin_addr.s_addr, ntohs(local_addr->sin_port), in_event_connected, in_event_read, in_event_close,
            SXE_BOOL_TO_STR(is_stream), path);

    if ((id = sxe_pool_set_oldest_element_state(sxe_array, SXE_STATE_FREE, SXE_STATE_USED)) == SXE_POOL_NO_INDEX) {
        SXEL30I("sxe_new: Warning: ran out of connections; concurrency too high");
        goto SXE_EARLY_OUT;
    }

    that                      = &sxe_array[id];
    that->id                  = id;
    that->path                = path;
    that->flags               = is_stream ? SXE_FLAG_IS_STREAM : 0;
    that->in_event_connected  = in_event_connected;
    that->in_event_read       = in_event_read;
    that->in_event_close      = in_event_close;
    that->socket_as_fd        = -1;
    that->socket              = SXE_SOCKET_INVALID;
    that->next_socket         = SXE_SOCKET_INVALID;
    that->in_total            = 0;
    that->in_consumed         = 0;
    memcpy(&that->local_addr, local_addr, sizeof(that->local_addr));

SXE_EARLY_OR_ERROR_OUT:
    SXER83I((that != NULL ? "%s%p // id=%u" : "%sNULL"), "return that=", that, (that != NULL ? that->id : ~0U));
    return that;
}

static SXE *
sxe_new(SXE                    * this              ,
        const char             * local_ip          ,
        unsigned short           local_port        ,
        SXE_IN_EVENT_CONNECTED   in_event_connected,
        SXE_IN_EVENT_READ        in_event_read     ,
        SXE_IN_EVENT_CLOSE       in_event_close    ,
        SXE_BOOL                 is_stream         ,
        const char             * path              )
{
    SXE                * that = NULL;
    struct sockaddr_in   local_addr;

    SXEE84I("sxe_new(local_ip=%s,local_port=%hu,is_stream=%s,path=%s)", local_ip, local_port, SXE_BOOL_TO_STR(is_stream), path);
    memset(&local_addr, 0, sizeof(local_addr));

#ifndef _WIN32
    if (path) {
        SXEA12I(strlen(path) < PIPE_PATH_MAX, "Unix domain socket path length is %u but must be less than %u characters",
                strlen(path),  PIPE_PATH_MAX);
        SXEL80I("caller wants new sxe using: pipe: unix domain socket path");
    }
    else
#endif
    {
        SXEL80I("caller wants new sxe using: inet: ip & port; i.e. tcp or udp");
        local_addr.sin_family = AF_INET;
        local_addr.sin_port   = htons(local_port);

        if (strcmp(local_ip, "INADDR_ANY") == 0) {
            local_addr.sin_addr.s_addr    = htonl(INADDR_ANY);
        }
        else {
            local_addr.sin_addr.s_addr    = inet_addr(local_ip);
        }
    }

    that = sxe_new_internal(this, &local_addr, in_event_connected, in_event_read, in_event_close, is_stream, path);

SXE_EARLY_OR_ERROR_OUT:
    SXER83I("return that=%p; // id=%u, port=%hu", that, (that == NULL ? ~0U : that->id),
            (that == NULL ? 0 : ntohs(that->local_addr.sin_port)) );
    return that;
}

#ifdef WINDOWS_NT
#else
SXE *
sxe_new_pipe(SXE                    * this              ,
             const char             * path              , /* path @ this pointer must never change! */
             SXE_IN_EVENT_CONNECTED   in_event_connected,
             SXE_IN_EVENT_READ        in_event_read     ,
             SXE_IN_EVENT_CLOSE       in_event_close    )
{
    return sxe_new(this, NULL, 0, in_event_connected, in_event_read, in_event_close, SXE_TRUE, path);
}
#endif

SXE *
sxe_new_tcp(SXE                    * this              ,
            const char             * local_ip          ,
            unsigned short           local_port        ,
            SXE_IN_EVENT_CONNECTED   in_event_connected,
            SXE_IN_EVENT_READ        in_event_read     ,
            SXE_IN_EVENT_CLOSE       in_event_close    )
{
    return sxe_new(this, local_ip, local_port, in_event_connected, in_event_read, in_event_close, SXE_TRUE, NULL);
}

SXE *
sxe_new_udp(SXE               * this         ,
            const char        * local_ip     ,
            unsigned short      local_port   ,
            SXE_IN_EVENT_READ   in_event_read)
{
    return sxe_new(this, local_ip, local_port, NULL, in_event_read, NULL, SXE_FALSE, NULL);
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
    SXEV82I(sxe_socket_set_nonblock(sock, 1), >= 0, "socket=%d: couldn't set non-blocking flag: %s", sock, sxe_socket_get_last_error_as_str());

#ifndef WINDOWS_NT
    /* If this is a pipe, we're done.
     */
    if (this->path != NULL) {
        goto SXE_EARLY_OUT;
    }
#endif

    /* Windows Sockets implementation of SO_REUSEADDR seems to be badly broken.
     */
    if (this->flags & SXE_FLAG_IS_STREAM) {
#ifndef WINDOWS_NT
        flags = 1;
        SXEV83I(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, SXE_WINAPI_CAST_CHAR_STAR &flags, sizeof(flags)),
            >= 0, "socket=%d: couldn't set reuse address flag: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
#endif
        //do we need this? flags = 1;
        //do we need this? SXEV83I(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, SXE_WINAPI_CAST_CHAR_STAR &flags, sizeof(flags)),
        //do we need this?        >= 0, "socket=%d: couldn't set keepalive flag: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

        linger_option.l_onoff  = 0;
        linger_option.l_linger = 0;
        SXEV83I(setsockopt(sock, SOL_SOCKET, SO_LINGER, SXE_WINAPI_CAST_CHAR_STAR &linger_option, sizeof(linger_option)),
               >= 0, "socket=%d: couldn't set linger to 0 secs: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

        flags = 1;
        SXEV83I(setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, SXE_WINAPI_CAST_CHAR_STAR &flags, sizeof(flags)),
               >= 0, "socket=%d: couldn't set TCP no delay flag: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
    }
    else {
        //debug SXEV83I(getsockopt(sock, SOL_SOCKET, SO_RCVBUF, SXE_WINAPI_CAST_CHAR_STAR &so_rcvbuf_size, &so_rcvbuf_size_length), >= 0, "socket=%d: couldn't get SO_RCVBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //debug SXEV83I(getsockopt(sock, SOL_SOCKET, SO_SNDBUF, SXE_WINAPI_CAST_CHAR_STAR &so_sndbuf_size, &so_sndbuf_size_length), >= 0, "socket=%d: couldn't get SO_SNDBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //debug SXEL12I("SO_RCVBUF %d, SO_SNDBUF %d", so_rcvbuf_size, so_sndbuf_size);

#define SXE_UDP_SO_RCVBUF_SIZE (32 * 1024 * 1024)

        so_rcvbuf_size = SXE_UDP_SO_RCVBUF_SIZE;
        SXEV83I(setsockopt(sock, SOL_SOCKET, SO_RCVBUF, SXE_WINAPI_CAST_CHAR_STAR &so_rcvbuf_size, sizeof(so_rcvbuf_size)), >= 0, "socket=%d: couldn't get SO_RCVBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //do we need this? so_sndbuf_size = (128 * 1024 * 1024);
        //do we need this? SXEV83I(setsockopt(sock, SOL_SOCKET, SO_SNDBUF, SXE_WINAPI_CAST_CHAR_STAR &so_sndbuf_size, sizeof(so_sndbuf_size)), >= 0, "socket=%d: couldn't get SO_SNDBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

        so_rcvbuf_size = 0;
        SXEV83I(getsockopt(sock, SOL_SOCKET, SO_RCVBUF, SXE_WINAPI_CAST_CHAR_STAR &so_rcvbuf_size, &so_rcvbuf_size_length), >= 0, "socket=%d: couldn't get SO_RCVBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        if (so_rcvbuf_size < SXE_UDP_SO_RCVBUF_SIZE) {
            SXEL33I("Warning: could not set SO_RCVBUF to %u; instead it is set to %u; consider e.g. sysctl -w net.core.rmem_max=%u; continuing but packet loss will occur at higher loads", SXE_UDP_SO_RCVBUF_SIZE, so_rcvbuf_size, SXE_UDP_SO_RCVBUF_SIZE); /* COVERAGE EXCLUSION */
        }

        //debug SXEV83I(getsockopt(sock, SOL_SOCKET, SO_RCVBUF, SXE_WINAPI_CAST_CHAR_STAR &so_rcvbuf_size, &so_rcvbuf_size_length), >= 0, "socket=%d: couldn't get SO_RCVBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //debug SXEV83I(getsockopt(sock, SOL_SOCKET, SO_SNDBUF, SXE_WINAPI_CAST_CHAR_STAR &so_sndbuf_size, &so_sndbuf_size_length), >= 0, "socket=%d: couldn't get SO_SNDBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //debug SXEL12I("SO_RCVBUF %d, SO_SNDBUF %d", so_rcvbuf_size, so_sndbuf_size);
    }

SXE_EARLY_OR_ERROR_OUT:

    SXER80I("return");
}

#define SXE_IO_CB_READ_MAXIMUM 64

static void
sxe_io_cb_read(EV_P_ ev_io * io, int revents)
{
    SXE                    * this           = (SXE *)io;
    SXE_SOCKLEN_T            peer_addr_size = sizeof(this->peer_addr);
    int                      length = 0; /* keep quiet mingw32-gcc.exe */
    int                      reads_remaining_this_event = SXE_IO_CB_READ_MAXIMUM;
    int                      last_socket_error = 0;
#ifndef _WIN32
    struct msghdr            message_header;
    int                      fd_from_recvmsg;
    SXE_CONTROL_MESSAGE_FD   control_message_buf;
    struct cmsghdr         * control_message_ptr = NULL;
    struct iovec             io_vector[1];                  /* This is intentionally a vector of length 1 */
    int                      input_data_left;
#endif

#if EV_MULTIPLICITY
    SXE_UNUSED_PARAMETER(loop);
#endif

    SXEE83I("sxe_io_cb_read(this=%p, revents=%u) // socket=%d", this, revents, this->socket);

    if (this->socket == SXE_SOCKET_INVALID) {
        SXEL10I("ERROR: Unexpected read event on closed SXE object");
        goto SXE_EARLY_OUT;
    }

    if (revents == EV_READ) {
SXE_TRY_AND_READ_AGAIN:
        if (this->path) {
#ifndef _WIN32
            io_vector[0].iov_base =        this->in_buf  + this->in_total;
            io_vector[0].iov_len  = sizeof(this->in_buf) - this->in_total;
            message_header.msg_control    = &control_message_buf;
            message_header.msg_controllen = CMSG_LEN(sizeof(fd_from_recvmsg));
            message_header.msg_name       = 0;
            message_header.msg_namelen    = 0;
            message_header.msg_iov        = io_vector;
            message_header.msg_iovlen     = sizeof(io_vector) / sizeof(io_vector[0]);    /* == 1 :) */

            length = recvmsg(this->socket, &message_header, 0);

            if (length > 0) {
                if ((control_message_ptr = CMSG_FIRSTHDR(&message_header)) == NULL) {
                    SXEL82I("Read %d bytes from peer via path %s without a control message", length, this->path);
                }
                else if (control_message_ptr->cmsg_len != CMSG_LEN(sizeof(fd_from_recvmsg))) {
                    SXEL22("sxe_io_cb_read(): control message length is %u, expected %u",        /* Coverage Exclusion: TODO */
                           CMSG_LEN(sizeof(fd_from_recvmsg)), control_message_ptr->cmsg_len);    /* Coverage Exclusion: TODO */
                }
                else if (control_message_ptr->cmsg_level != SOL_SOCKET) {
                    SXEL21("sxe_io_cb_read(): control message level %d != SOL_SOCKET", control_message_ptr->cmsg_level);    /* Coverage Exclusion: TODO */
                }
                else if (control_message_ptr->cmsg_type != SCM_RIGHTS) {
                    SXEL21("sxe_io_cb_read(): control message type %d != SCM_RIGHTS", control_message_ptr->cmsg_type);    /* Coverage Exclusion: TODO */
                }
                else {
                    if (this->next_socket >= 0) {
                        SXEL32I("sxe_io_cb_read(): Received fd %d on pipe, and already received %d",    /* Coverage Exclusion: TODO */
                                *(int *)CMSG_DATA(control_message_ptr), this->next_socket);             /* Coverage Exclusion: TODO */
                    }

                    SXEL83I("Read %d bytes from peer via path %s with a control message (fd=%d)", length, this->path, this->next_socket);
                    this->next_socket = *(int *)CMSG_DATA(control_message_ptr);
                }

                if (this->next_socket >= 0) {
                     if (ioctl(this->socket, SIOCINQ, &input_data_left) < 0) {
                         SXEL22("sxe_io_cb_read(): ioctl(socket=%d,SIOCINQ) failed: %s", this->socket,    /* Coverage Exclusion: TODO */
                                sxe_socket_error_as_str(errno));                                          /* Coverage Exclusion: TODO */
                     }
                     else if (input_data_left == 0) {
                         SXEL81I("No more data on pipe: closing pipe socket=%d", this->socket);
                         close(this->socket);
#ifdef EV_MULTIPLICITY
                         ev_io_stop(sxe_private_main_loop, &this->io);
#else
                         ev_io_stop(&this->io);
#endif
                         this->socket       = this->next_socket;
                         this->socket_as_fd = this->next_socket;    /* Same thing, since pipes are UNIX only    */
                         this->next_socket  = -1;                   /* Won't be used again, but clear it anyway */
                         this->path         = NULL;
                         this->flags       |=  SXE_FLAG_IS_STREAM;

                         ev_io_init((struct ev_io*)&this->io, sxe_io_cb_read, this->socket, EV_READ);
                         ev_io_start(sxe_private_main_loop, (struct ev_io*)&this->io);
                         SXEL81I("Set connection to use received TCP socket=%d", this->socket);
                     }
                }
            }
#endif
        }
        else if (this->flags & SXE_FLAG_IS_STREAM) {
            /* Use recv(), not read() for Windows Sockets API compatibility
             */
            length = recv(this->socket, this->in_buf + this->in_total, sizeof(this->in_buf) - this->in_total, 0);
        }
#if SXE_WANT_CALLER_READS_UDP
        else if (this->flags & SXE_FLAG_IS_CALLER_READS) {
            do {
                (*this->in_event_read)(this, 0);
            } while (sxe_caller_read_udp_length > 0);

            length = sxe_caller_read_udp_length;
        }
#endif
        else {
            length = recvfrom(this->socket, this->in_buf + this->in_total, sizeof(this->in_buf) - this->in_total, 0,
                              (struct sockaddr *)&this->peer_addr, &peer_addr_size);
            last_socket_error = sxe_socket_get_last_error();
            SXEA81I(peer_addr_size == sizeof(this->peer_addr), "Peer address is not an IPV4 address (peer_addr_size=%d)",
                    peer_addr_size);
            SXEL83I("Read %d bytes from peer IP %s:%hu", length, inet_ntoa(this->peer_addr.sin_addr),
                    ntohs(this->peer_addr.sin_port));
        }

        if (length > 0) {
            SXED90I(this->in_buf + this->in_total, length);
            this->in_total += length;

            /* If the buffer is full, SXE must wait for the caller to clear it before asking ev for more EVREAD events.
             */
            if (this->in_total == SXE_BUF_SIZE) {
                if (this->in_consumed) {
                    SXEL80I("Shuffling the buffer to make room for more data");
                    memmove(this->in_buf, SXE_BUF(this), SXE_BUF_USED(this));
                    this->in_total -= this->in_consumed;
                    this->in_consumed = 0;
                }
                else {
                    SXEL80I("Buffer is full: stopping read events");
#ifdef EV_MULTIPLICITY
                    ev_io_stop(sxe_private_main_loop, &this->io);
#else
                    ev_io_stop(&this->io);
#endif
                }
            }

            /* for now... quick and dirty statistics */
            sxe_stat_total_read++;

            //debug if ((sxe_stat_total_read % 5000) == 0) {
            //debug     SXEL52I("sxe_stat_total_read=%u, sxe_stat_total_accept=%u", sxe_stat_total_read, sxe_stat_total_accept); /* COVERAGE EXCLUSION: Statistics */
            //debug }

            if (!(this->flags & SXE_FLAG_IS_PAUSED)) {
                SXEL91I("About to pass read event up (%u new bytes in buffer)", length);
                (*this->in_event_read)(this, length);
                reads_remaining_this_event--;
            }

            if (!(this->flags & SXE_FLAG_IS_STREAM) && reads_remaining_this_event) {
                goto SXE_TRY_AND_READ_AGAIN;
            }
        }
        else {
            if (length < 0) {
                switch ((last_socket_error == 0) ? sxe_socket_get_last_error() : last_socket_error) {

                case SXE_SOCKET_ERROR(EWOULDBLOCK): // EAGAIN
                    SXEL81I("socket=%d is not ready", this->socket);
                    goto SXE_EARLY_OUT;

                /* Expected error codes:
                 */
                case SXE_SOCKET_ERROR(ECONNRESET):
                    SXEL83I("Failed to read from socket=%d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
                    break; /* Coverage Exclusion: TODO */

                /* Should never get here.
                 */
                default:
                    SXEL23I("Failed to read from socket=%d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
                }
            }

            /* Under Linux, a received disconnect request may lead to a zero byte read.
             */
            else {
                SXEL81I("Failed to read from socket=%d: (0) zero bytes read - disconnect", this->socket);
            }

            if (this->flags & SXE_FLAG_IS_STREAM) {
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

void
sxe_buf_clear(SXE * this)
{
    SXEE80I("sxe_buf_clear()");

    if (this->in_total == SXE_BUF_SIZE) {
        SXEL80I("Buffer was full: restarting read events");
        ev_io_start(sxe_private_main_loop, (struct ev_io*)&this->io);
    }

    this->in_consumed = 0;
    this->in_total    = 0;
    SXER80I("return");
}

/**
 * Consume part of the data in the SXE input buffer
 *
 * @param this  Pointer to the SXE
 * @param bytes Number of bytes to consume
 *
 * @note Consuming automatically pauses the SXE; further read events must be reenabled with sxe_buf_resume()
 */
void
sxe_buf_consume(SXE * this, unsigned bytes)
{
    SXEE81I("sxe_buf_consume(bytes=%u)", bytes);
    SXEA82I(bytes <= SXE_BUF_SIZE,       "attempt to consume %u bytes, which is more than SXE_BUF_SIZE (%u)", bytes, SXE_BUF_SIZE);
    SXEA82I(bytes <= SXE_BUF_USED(this), "attempt to consume %u bytes, which is more than SXE_BUF_USED (%u)", bytes, SXE_BUF_USED(this));
    this->in_consumed += bytes;
    this->flags       |= SXE_FLAG_IS_PAUSED;

    if (this->in_consumed == this->in_total) {
        SXEL80I("Consumed the whole buffer; clearing");
        sxe_buf_clear(this);
    }

    SXER80I("return");
}

/**
 * Resume read events on a SXE
 *
 * @param this  Pointer to the SXE
 * @param invoke_callback   SXE_BUF_RESUME_IMMEDIATE to invoke the callback
 *                          SXE_BUF_RESUME_WHEN_MORE_DATA to not invoke the callback
 *
 * @note if there is unconsumed data in the buffer, a read event is immediately generated with the 'length' read being the
 *       amount of unconsumed data; be careful to call this function outside of any critical sections
 */
void
sxe_buf_resume(SXE * this, SXE_BUF_RESUME invoke_callback)
{
    SXEE83I("%s(this=%p, invoke_callback=%s)", __func__, this,
            invoke_callback == SXE_BUF_RESUME_IMMEDIATE ? "IMMEDIATE" : "WHEN_MORE_DATA");
    this->flags &= ~SXE_FLAG_IS_PAUSED;

    /* If we've filled the buffer and stopped reading, we need to ensure there
     * is room for more data to read, otherwise we won't ever actually get
     * more data! */
    if (this->in_consumed && this->in_total == SXE_BUF_SIZE) {
        SXEA10I(!ev_is_active((struct ev_io*)&this->io), "IO watcher was already active!");
        SXEL80I("Watcher was paused: making room for more data, and restarting read events");
        memmove(this->in_buf, SXE_BUF(this), SXE_BUF_USED(this));
        this->in_total -= this->in_consumed;
        this->in_consumed = 0;
        ev_io_start(sxe_private_main_loop, (struct ev_io*)&this->io);
    }

    if (invoke_callback == SXE_BUF_RESUME_IMMEDIATE && SXE_BUF_USED(this) != 0) {
        (*this->in_event_read)(this, SXE_BUF_USED(this));
    }

    SXER80I("return");
}

static void
sxe_io_cb_accept(EV_P_ ev_io * io, int revents)
{
    SXE              * this           = (SXE *)io;
    SXE              * that           = NULL;
    SXE_SOCKET         that_socket    = SXE_SOCKET_INVALID;
    struct sockaddr_in peer_addr;
    SXE_SOCKLEN_T      peer_addr_size = sizeof(peer_addr);

#if EV_MULTIPLICITY
    SXE_UNUSED_PARAMETER(loop);
#endif
    SXE_UNUSED_PARAMETER(revents);

    SXEE83I("sxe_io_cb_accept(this=%p, revents=%u) // socket=%d", this, revents, this->socket);
    SXEA80I(revents == EV_READ, "sxe_io_cb_accept: revents is not EV_READ");

    do {
        /* TODO: Track SXE & open socket count and disable / enable accept() event as appropriate */
        SXEL90I("Accept()");

        if ((that_socket = accept(this->socket, (struct sockaddr *)&peer_addr, &peer_addr_size)) == SXE_SOCKET_INVALID)
        {
            if (sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EWOULDBLOCK)) {
                SXEL81I("No more connections to accept on listening socket=%d", this->socket);
                goto SXE_EARLY_OUT;
            }

            SXEL23I("Error accepting on socket=%d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
            goto SXE_ERROR_OUT;
        }

#if SXE_DEBUG
#ifndef _WIN32
        if (this->path) {
            struct ucred credentials;
            unsigned ucred_length = sizeof(struct ucred);

            SXEV80I(getsockopt(that_socket, SOL_SOCKET, SO_PEERCRED, &credentials, &ucred_length), >= 0, "could obtain credentials from unix domain socket");

            SXEL83I("Accepted connection from pid %d, uid %d, gid %d",
                credentials.pid,  /* the process ID            of the process on the other side of the socket */
                credentials.uid,  /* the effective UID         of the process on the other side of the socket */
                credentials.gid); /* the effective primary GID of the process on the other side of the socket */
        }
        else
#endif
        {
            SXEL85I("Accepted connection from ip=%d.%d.%d.%d:%hu",
                    (ntohl(peer_addr.sin_addr.s_addr) >> 24) & 0xff,
                    (ntohl(peer_addr.sin_addr.s_addr) >> 16) & 0xff,
                    (ntohl(peer_addr.sin_addr.s_addr) >> 8 ) & 0xff,
                     ntohl(peer_addr.sin_addr.s_addr)        & 0xff,
                     ntohs(peer_addr.sin_port       )              );
        }
#endif

        sxe_set_socket_options(this, that_socket);

        if (this->flags & SXE_FLAG_IS_ONESHOT) {
            SXEL80I("Closing one-shot listening socket and replacing with accepted socket");
            ev_io_stop(sxe_private_main_loop, &this->io);
            close(this->socket_as_fd);
            this->socket_as_fd = -1;
            CLOSESOCKET(this->socket);
            that = this;
        }
        else {
            SXEL80I("Creating a new SXE");
            that = sxe_new_internal(this, &this->local_addr, this->in_event_connected, this->in_event_read,
                                    this->in_event_close, this->flags & SXE_FLAG_IS_STREAM, this->path);

            if (that == NULL) {
                SXEL31I("Warning: failed to allocate a connection from socket=%d: out of connections", this->socket);
                goto SXE_ERROR_OUT;
            }
        }

        that->socket               = that_socket;
        that->socket_as_fd         = _open_osfhandle(that_socket, 0);
        SXE_USER_DATA_AS_INT(that) = SXE_USER_DATA_AS_INT(this);
        memcpy(&that->peer_addr, &peer_addr, sizeof(that->peer_addr));

        SXEL82I("add accepted connection to watch list, socket==%d, socket_as_fd=%d", that_socket, that->socket_as_fd);
        ev_io_init((struct ev_io*)&that->io, sxe_io_cb_read, that->socket_as_fd, EV_READ);
        ev_io_start(sxe_private_main_loop, (struct ev_io*)&that->io);
        sxe_stat_total_accept++;

        if (that->in_event_connected != NULL) {
            (*that->in_event_connected)(that);
        }
    } while ((this->path == NULL) && (this != that));

    /* TODO: is there a way to greedily accept on unix domain sockets? and do we care? :-) */

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
    ev_io_stop(sxe_private_main_loop, &this->io);
    ev_io_init(&this->io, sxe_io_cb_read, this->socket_as_fd, EV_READ);
    ev_io_start(sxe_private_main_loop, &this->io);

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

/**
 * Get the local address (IP:port) of a SXE
 *
 * @param this SXE to get the local address of
 *
 * @return A pointer to the local address structure
 *
 * @exception Aborts if the SXE is a pipe SXE
 */
struct sockaddr_in *
sxe_get_local_addr(SXE * this)
{
    SXE_SOCKLEN_T address_length = sizeof(this->local_addr);

    SXEA11I(this->path == NULL, "sxe_get_local_addr: SXE is a pipe '%s'", this->path);
    SXEE82I("%s(this=%p)", __func__, this);

    if (this->local_addr.sin_port != 0) {
        goto SXE_EARLY_OUT;
    }

    if (getsockname(this->socket, (struct sockaddr *)&this->local_addr, &address_length) < 0 ) {
        SXEL22I("%s: getsockname() failed: %s\n", __func__, sxe_socket_get_last_error_as_str());    /* COVERAGE EXCLUSION : TODO - when we are able to mock */
    }

SXE_EARLY_OUT:
    SXER81I("return %p", &this->local_addr);
    return &this->local_addr;
}

/**
 * Listen for connections (or packets for UDP) on a SXE
 *
 * @param this  SXE to listen on
 * @param flags 0 or SXE_FLAG_IS_ONESHOT to specify a one-shot listener that turns into the connection on accept
 *
 * @return SXE_RETURN_OK on success, SXE_RETURN_ERROR_ADDRESS_IN_USE if the SXE's address is in use, or
 *         SXE_RETURN_ERROR_INTERNAL
 */
SXE_RETURN
sxe_listen_plus(SXE * this, unsigned flags)
{
    SXE_RETURN         result        = SXE_RETURN_ERROR_INTERNAL;
    SXE_SOCKET         socket_listen = SXE_SOCKET_INVALID;
    struct sockaddr  * address;
    SXE_SOCKLEN_T      address_length;
#ifndef _WIN32
    struct sockaddr_un pipe_address;
#endif

    SXEE82I("sxe_listen(this=%p, flags=%p)", this, flags);
    SXEA10I(this  != NULL,                   "sxe_listen: object pointer is NULL");
    SXEA10I(!sxe_is_free(this),              "sxe_listen: connection has not been allocated (state=FREE)");
    SXEA11I(!(flags & ~SXE_FLAG_IS_ONESHOT), "sxe_listen: a flag other than SXE_FLAG_IS_ONESHOT was given: 0x%08x",
            flags & ~SXE_FLAG_IS_ONESHOT);

    if (this->socket != SXE_SOCKET_INVALID) {
        SXEL21I("Listener is already in use (socket=%d)", this->socket);
        goto SXE_EARLY_OUT;
    }

    socket_listen = socket(this->path ? AF_UNIX : AF_INET, ((this->flags & SXE_FLAG_IS_STREAM) ? SOCK_STREAM : SOCK_DGRAM), 0);

    if (socket_listen == SXE_SOCKET_INVALID) {
        SXEL22I("Error creating listening socket: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        goto SXE_EARLY_OUT;
    }

    SXEL83I("socket=%d == listen_socket = socket(%s, %s, 0)", socket_listen, this->path ? "AF_UNIX" : "AF_INET",
            (this->flags & SXE_FLAG_IS_STREAM) ? "SOCK_STREAM" : "SOCK_DGRAM");

#ifndef _WIN32
    if (this->path) {
        pipe_address.sun_family = AF_UNIX;
        strcpy(pipe_address.sun_path, this->path);
        unlink(this->path);

        address        = (struct sockaddr *)&pipe_address;
        address_length = sizeof(             pipe_address);
    }
    else
#endif
    {
        address        = (struct sockaddr *)&this->local_addr ;
        address_length = sizeof(             this->local_addr);
    }

    sxe_set_socket_options(this, socket_listen);

    if (bind(socket_listen, address, address_length) < 0) {
        int error = sxe_socket_get_last_error();    /* Save due to Windows resetting it in inet_ntoa */

        SXEL25I("Error binding address to listening socket: inet: address=%s:%hu, path=%s: (errno=%d) %s",
                inet_ntoa(this->local_addr.sin_addr), ntohs(this->local_addr.sin_port), this->path, error, sxe_socket_error_as_str(error));
        sxe_socket_set_last_error(error);

        if (error == SXE_SOCKET_ERROR(EADDRINUSE)) {
            result = SXE_RETURN_ERROR_ADDRESS_IN_USE;
        }

        goto SXE_EARLY_OUT;
    }

    if ((this->flags & SXE_FLAG_IS_STREAM) && (listen(socket_listen, sxe_listen_backlog) < 0)) {
        SXEL22I("Error listening on socket: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        goto SXE_EARLY_OUT;
    }

    this->flags       |= flags;            /* Set one-shot, if specified by the caller */
    this->socket       = socket_listen;
    this->socket_as_fd = _open_osfhandle(socket_listen, 0);
    socket_listen      = SXE_SOCKET_INVALID;
    SXEL86I("connection id of new listen socket is %d, socket_as_fd=%d, backlog=%d, address=%s:%hu, path=%s", this->id,
            this->socket_as_fd, sxe_listen_backlog, inet_ntoa(this->local_addr.sin_addr), ntohs(this->local_addr.sin_port), this->path);

    ev_io_init((struct ev_io*)&this->io, ((this->flags & SXE_FLAG_IS_STREAM) ? sxe_io_cb_accept : sxe_io_cb_read), this->socket_as_fd, EV_READ);
    ev_io_start(sxe_private_main_loop, (struct ev_io*)&this->io);

    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    if (socket_listen != SXE_SOCKET_INVALID) {
        SXEL81I("socket=%d is unusable, so must be closed", socket_listen);
        CLOSESOCKET(socket_listen);
    }

    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

void
sxe_set_listen_backlog(int listen_backlog)
{
    SXEE81("sxe_set_listen_backlog(listen_backlog=%d)", listen_backlog);
    SXEL82("backlog was %d; now setting to %d", sxe_listen_backlog, listen_backlog);
    sxe_listen_backlog = listen_backlog;
    SXER80("return");
}

#ifdef WINDOWS_NT
#else
SXE_RETURN
sxe_connect_pipe(SXE * this)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;

    SXEE83I("sxe_connect_pipe(this=%p) // socket=%d, path=%s", this, this->socket, this->path);

    result = sxe_connect(this, NULL, 0);

    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}
#endif

/**
 * Construct a SXE address
 *
 * @param address_out Pointer to address variable, set by function
 * @param ip          Pointer to '\0' terminated IP address in dotted decimal
 * @param port        Port number in host byte order
 *
 * @return address_out
 */

const struct sockaddr_in *
sxe_construct_address(struct sockaddr_in * address_out, const char * ip, unsigned short port)
{
    memset(address_out, 0x00, sizeof(*address_out));
    address_out->sin_family      = AF_INET;
    address_out->sin_port        = htons(port);
    address_out->sin_addr.s_addr = inet_addr(ip);
    return address_out;
}

/**
 * Connect a SXE to an address
 *
 * @param this Pointer to the SXE
 * @param ip   Pointer to '\0' terminated IP address in dotted decimal
 * @param port Port number in host byte order
 *
 * @return SXE_RETURN_ERROR_ALREADY_CONNECTED if already connected, SXE_RETURN_ERROR_INTERNAL on unexpected error
 *         creating/binding/connecting socket, SXE_RETURN_ERROR_ADDRESS_IN_USE if SXE local address is already in use, or
 *         SXE_RETURN_OK on success.
 *
 * @exception Aborts if _this_ is NULL or points to a free connection object
 */

SXE_RETURN
sxe_connect(SXE * this, const char * peer_ip, unsigned short peer_port)
{
    SXE_RETURN         result = SXE_RETURN_ERROR_INTERNAL;
    SXE_SOCKET         that_socket;
    struct sockaddr  * peer_address;
    SXE_SOCKLEN_T      peer_address_length;
#ifndef _WIN32
    struct sockaddr_un pipe_address;
#endif

    SXEE85I("sxe_connect(this=%p, peer_ip==%s, peer_port=%hu) // socket=%d, path=%s", this, peer_ip, peer_port, this->socket,
            this->path);
    SXEA10I(this != NULL,       "sxe_connect: connection pointer is NULL");
    SXEA10I(!sxe_is_free(this), "sxe_connect: connection has not been allocated (state=FREE)");

    if (this->socket != SXE_SOCKET_INVALID) {
        SXEL21I("Connection is already in use (socket=%d)", this->socket);
        result = SXE_RETURN_ERROR_ALREADY_CONNECTED;
        goto SXE_EARLY_OUT;
    }

    SXEL80I("TODO: Add a timeout parameter");

    if ((that_socket = socket(this->path ? AF_UNIX : PF_INET, SOCK_STREAM, this->path ? 0 : IPPROTO_TCP)) == SXE_SOCKET_INVALID) {
        SXEL22I("error creating socket: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        goto SXE_ERROR_OUT;
    }

#ifndef _WIN32
    if (this->path) {

        /* todo: position in code to implement equivalent to unix domain socket / fd passing on winnt */
        pipe_address.sun_family = AF_UNIX;
        strcpy(pipe_address.sun_path, this->path);

        peer_address        = (struct sockaddr *)&pipe_address;
        peer_address_length = sizeof(pipe_address.sun_family) + strlen(pipe_address.sun_path);
    }
    else
#endif
    {
        /* From ip(7) - Linux man page:
         *   When a process wants to receive new incoming packets or connections,
         *   it should bind a socket to a local interface address using bind(2).
         *   Only one IP socket may be bound to any given local (address, port)
         *   pair. When INADDR_ANY is specified in the bind call the socket will
         *   be bound to *ALL* local interfaces. When listen(2) or connect(2) are
         *               ^^^^^
         *   called on an unbound socket, it is automatically bound to a random
         *   free port with the local address set to *INADDR_ANY*.
         *                                           ^^^^^^^^^^^^
         * Therefore: If the local address is not INADDR_ANY:0, bind it to the socket. */
        if (this->local_addr.sin_port == 0) {
            SXEA10I(this->local_addr.sin_addr.s_addr == htonl(INADDR_ANY), "Port must be non-zero if IP is not INADDR_ANY; see ip(7) Linux man page");
            SXEL80I("Connecting using local TCP port: randomly allocated by the stack on IP INADDR_ANY");
        }
        else {
            SXEL81I("Connecting using local TCP port: %hu", ntohs(this->local_addr.sin_port));

            if (bind(that_socket, (struct sockaddr *)&this->local_addr, sizeof(this->local_addr)) == SXE_SOCKET_ERROR_OCCURRED) {
                if (sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EADDRINUSE)) {
                    SXEL33I("Can't connect using local TCP port: %hu (%d) %s", ntohs(this->local_addr.sin_port),
                            sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
                    result = SXE_RETURN_ERROR_ADDRESS_IN_USE;
                }
                else {
                    SXEL22I("Unexpected error calling bind: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
                }

                goto SXE_ERROR_OUT;
            }
        }

        sxe_construct_address(&this->peer_addr, peer_ip, peer_port);
        peer_address        = (struct sockaddr *)&this->peer_addr ;
        peer_address_length = sizeof(             this->peer_addr);
    }

    sxe_set_socket_options(this, that_socket);

    /* Connect non-blocking.
     */

    SXEL82I("add connection to watch list, socket==%d, socket_as_fd=%d", that_socket, _open_osfhandle(that_socket, 0));
    ev_io_init((struct ev_io*)&this->io, sxe_io_cb_connect, _open_osfhandle(that_socket, 0), EV_WRITE);
    ev_io_start(sxe_private_main_loop, &this->io);

    if ((connect(that_socket, peer_address, peer_address_length) == SXE_SOCKET_ERROR_OCCURRED)
            && (sxe_socket_get_last_error() != SXE_SOCKET_ERROR(EINPROGRESS))
            && (sxe_socket_get_last_error() != SXE_SOCKET_ERROR(EWOULDBLOCK))) {
        SXEL22I("connect failed: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        ev_io_stop(sxe_private_main_loop, &this->io);
        goto SXE_ERROR_OUT;
    }

    SXEL84I("Connection to server: ip:port %s:%hu, path %s: %s", peer_ip, peer_port, this->path,
            ((sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EINPROGRESS) || sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EWOULDBLOCK)) ? "in progress" : "completed"));
    this->socket       = that_socket;
    this->socket_as_fd = _open_osfhandle(that_socket, 0);

#if SXE_DEBUG
    /* Display the local socket's bound port and IP; TODO: use sxe_get_local_addr and inet_ntoa */
    if (this->path != NULL) {
        struct sockaddr_in local_addr;
        SXE_SOCKLEN_T      address_length = sizeof(local_addr);

        if (getsockname(that_socket, (struct sockaddr *)&local_addr, &address_length) >= 0 ) {
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

#ifdef WINDOWS_NT
#else
SXE_RETURN
sxe_write_pipe(SXE * this, const void * buf, unsigned size, int fd_to_send)
{
    SXE_RETURN               result = SXE_RETURN_ERROR_INTERNAL;
    SXE_CONTROL_MESSAGE_FD   control_message_buf;
    struct msghdr            message_header;
    struct cmsghdr         * control_message_ptr;
    struct iovec             io_vector[1];           /* This is intentional. It's a vector of length 1 */

    SXEA80I(this != NULL, "sxe_write_pipe(): connection pointer is NULL");
    SXEA80I(this->path  , "sxe_write_pipe(): connection is not a unix domain socket");
    SXEA80I(size != 0,    "sxe_write_pipe(): Must send at least 1 byte of data");
    SXEE84I("sxe_write_pipe(this=%p,size=%u,fd_to_send=%d) // socket=%d", this, size, fd_to_send, this->socket);
    SXED90I(buf, size);

    /* This may be the worst API yet: move over Windows.
     */
    io_vector[0].iov_base                  = (void *)(long)buf;
    io_vector[0].iov_len                   = size;
    message_header.msg_control             = &control_message_buf;
    message_header.msg_controllen          = CMSG_LEN(sizeof(fd_to_send));
    message_header.msg_name                = 0;
    message_header.msg_namelen             = 0;
    message_header.msg_iov                 = io_vector;
    message_header.msg_iovlen              = sizeof(io_vector) / sizeof(io_vector[0]);    /* == 1 :) */
    control_message_ptr                    = CMSG_FIRSTHDR(&message_header);
    control_message_ptr->cmsg_len          = CMSG_LEN(sizeof(fd_to_send));
    control_message_ptr->cmsg_level        = SOL_SOCKET;
    control_message_ptr->cmsg_type         = SCM_RIGHTS;
    *(int *)CMSG_DATA(control_message_ptr) = fd_to_send;

    /* From sendmsg(2) on FreeBSD:
     *
     *     Because sendmsg() does not necessarily block until the data has been transferred, it is possible to transfer an open
     *     file descriptor across an AF_UNIX domain socket (see recv(2)), then close() it before it has actually been sent, the
     *     result being that the receiver gets a closed file descriptor.  It is left to the application to implement an
     *     acknowledgment mechanism to prevent this from happening.
     *
     * We acknowledge a sendmsg by closing the pipe once all data from the sendmsg has been processed by the application.
     */

    if (sendmsg(this->socket, &message_header, 0) < 0) {
        SXEL23I("sxe_write_pipe(): Error writing to socket=%d: (%d) %s", this->socket,    /* Coverage Exclusion: TODO */
                sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());         /* Coverage Exclusion: TODO */
        goto SXE_ERROR_OUT; /* Coverage Exclusion: TODO */
    }

    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}
#endif

SXE_RETURN
sxe_write_to(SXE * this, const void * buf, unsigned size, const struct sockaddr_in * dest_addr)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    int        ret;

    SXEA80I(this != NULL,                        "sxe_write_to(): connection pointer is NULL");
    SXEA80I(!(this->flags & SXE_FLAG_IS_STREAM), "sxe_write_to(): SXE is not a UDP SXE");
    SXEE85I("sxe_write_to(this=%p,size=%u,sockaddr_in=%s:%hu) // socket=%d", this, size, inet_ntoa(dest_addr->sin_addr),
            ntohs(dest_addr->sin_port), this->socket);
    SXED90I(buf, size);

    if ((ret = sendto(this->socket, buf, size, 0, (const struct sockaddr *)dest_addr, sizeof(*dest_addr))) != (int)size) {
        if (ret >= 0) {
            SXEL23I("sxe_write_to(): Only %d of %u bytes written to socket=%d", ret, size, this->socket);   /* COVERAGE EXCLUSION: Logging for UDP truncation on sendto */
        }
        else {
            SXEL23I("sxe_write_to(): Error writing to socket=%d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
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

    SXEA80I(this != NULL,                       "SXE pointer is NULL");
    SXEA80I((this->flags & SXE_FLAG_IS_STREAM), "SXE is not a stream SXE");
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
     *
     * Note: Use SXE_SOCKET_MSG_NOSIGNAL to avoid rare occurrence when connection closed by peer but locally the TCP
     *       stack thinks the connection is still open.  On Windows, this is the normal behaviour.
     */
    if ((ret = send(this->socket, buf, size, SXE_SOCKET_MSG_NOSIGNAL)) != (int)size) {
        /* TODO: Need to support TCP flow control
         */
        if (ret >= 0) {
            SXEL83I("sxe_write(): Only %d of %u bytes written to socket=%d", ret, size, this->socket);
            this->last_write = ret;
            result = SXE_RETURN_WARN_WOULD_BLOCK; /* Coverage Exclusion - todo: win32 coverage */
        }
        else {
            SXEL23I("sxe_write(): Error writing to socket=%d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
            this->last_write = 0;

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

    this->last_write = size;
    SXEL82I("Wrote %u bytes to socket=%d", size, this->socket);
    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

static void
sxe_io_cb_send(EV_P_ ev_io * io, int revents) /* Coverage Exclusion - todo: win32 coverage */
{
    SXE_RETURN result = SXE_RETURN_OK; /* Coverage Exclusion - todo: win32 coverage */
    SXE * this = (SXE *)io; /* Coverage Exclusion - todo: win32 coverage */
    SXE_UNUSED_PARAMETER(revents);

#if EV_MULTIPLICITY
    SXE_UNUSED_PARAMETER(loop);
#endif

    SXEE83I("sxe_io_cb_send(this=%p, revents=%u) // socket=%d", this, revents, this->socket);

    if (this->send_buf_written != this->send_buf_len) { /* Coverage Exclusion - todo: win32 coverage */
        result = sxe_write(this, (this->send_buf + this->send_buf_written), (this->send_buf_len - this->send_buf_written));
        this->send_buf_written += this->last_write;
    }

    if (result != SXE_RETURN_OK) {  /* Coverage Exclusion - todo: win32 coverage */
        if (result == SXE_RETURN_WARN_WOULD_BLOCK) {
           SXEL82("Partial write, written %u bytes of %u", this->send_buf_written, this->send_buf_len);
           goto SXE_EARLY_OUT; /* Coverage Exclusion - todo: win32 coverage */
        }
    }

    SXEA10(this->send_buf_written == this->send_buf_len, "if sxe_write returns SXE_RETURN_OK then all data should be writen"); /* Coverage Exclusion - todo: win32 coverage */

    SXEL80I("Re-enabling read events on this SXE");
    ev_io_stop(sxe_private_main_loop, &this->io); /* Coverage Exclusion - todo: win32 coverage */
    ev_io_init(&this->io, sxe_io_cb_read, this->socket_as_fd, EV_READ);
    ev_io_start(sxe_private_main_loop, &this->io);

    if (this->out_event_written) { /* Coverage Exclusion - todo: win32 coverage */
        (*this->out_event_written)(this, result);
        this->out_event_written = NULL;
    }

SXE_EARLY_OUT:
    SXER80I("return");
} /* Coverage Exclusion - todo: win32 coverage */

SXE_RETURN
sxe_send(SXE * this, const void * buf, unsigned size, SXE_OUT_EVENT_WRITTEN on_complete)
{
    SXE_RETURN result = SXE_RETURN_OK;

    SXEE83I("sxe_send(buf=%p, size=%u, on_complete=%p)", buf, size, on_complete);
    SXEA10I(on_complete != NULL, "sxe_send: on_complete callback function pointer can't be NULL");

    this->send_buf          = (const char *)buf;
    this->send_buf_len      = size;
    this->send_buf_written  = 0;

    result = sxe_write(this, (this->send_buf + this->send_buf_written), (this->send_buf_len - this->send_buf_written));
    this->send_buf_written += this->last_write;

    // Only partials need the EV_WRITE callback, even error cases just return...
    if (result == SXE_RETURN_WARN_WOULD_BLOCK) {
        result = SXE_RETURN_IN_PROGRESS; /* Coverage Exclusion - todo: win32 coverage */
        SXEL82("Initial sxe_write wrote %u bytes of %u", this->send_buf_written, this->send_buf_len);
    } else {
        goto SXE_EARLY_OUT;
    }

    this->out_event_written = on_complete; /* Coverage Exclusion - todo: win32 coverage */
    ev_io_stop(sxe_private_main_loop, &this->io);
    ev_io_init(&this->io, sxe_io_cb_send, this->socket_as_fd, EV_WRITE);
    ev_io_start(sxe_private_main_loop, &this->io);

SXE_EARLY_OUT:
    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

static void
sxe_io_cb_notify_writable(EV_P_ ev_io * io, int revents)
{
    SXE_OUT_EVENT_WRITTEN cb;
    SXE * this = (SXE *)io;
    SXE_UNUSED_PARAMETER(revents);
#if EV_MULTIPLICITY
    SXE_UNUSED_PARAMETER(loop);
#endif
    SXEE83I("sxe_io_cb_notify_writable(this=%p, revents=%u) // socket=%d", this, revents, this->socket);
    SXEL80I("Re-enabling read events on this SXE");
    ev_io_stop(sxe_private_main_loop, &this->io);
    ev_io_init(&this->io, sxe_io_cb_read, this->socket_as_fd, EV_READ);
    ev_io_start(sxe_private_main_loop, &this->io);

    if (this->out_event_written) {
        cb = this->out_event_written;
        this->out_event_written = NULL;
        (*cb)(this, SXE_RETURN_OK);
    }
    SXER80I("return");
}

void
sxe_notify_writable(SXE * this, SXE_OUT_EVENT_WRITTEN writable_cb)
{
    SXEE81I("sxe_notify_writable(on_complete=%p)", writable_cb);
    this->out_event_written = writable_cb;
    ev_io_stop(sxe_private_main_loop, &this->io);
    ev_io_init(&this->io, sxe_io_cb_notify_writable, this->socket_as_fd, EV_WRITE);
    ev_io_start(sxe_private_main_loop, &this->io);
    SXER80I("return");
}

#ifndef WINDOWS_NT
static void
sxe_io_cb_sendfile(EV_P_ ev_io * io, int revents)
{
    SXE * this = (SXE *)io;
    SXE_UNUSED_PARAMETER(loop);
    SXE_UNUSED_PARAMETER(revents);

    SXEE83I("sxe_io_cb_sendfile(this=%p, revents=%u) // socket=%d", this, revents, this->socket);
    sxe_sendfile(this, this->sendfile_in_fd, this->sendfile_bytes, this->out_event_written);
    SXER80("return");
}

SXE_RETURN
sxe_sendfile(SXE * this, int in_fd, unsigned total_bytes, SXE_OUT_EVENT_WRITTEN on_complete)
{
    SXE_RETURN result = SXE_RETURN_ERROR_WRITE_FAILED;
    ssize_t    sent;

    SXEE83I("sxe_sendfile(in_fd=%d, total_bytes=%u, on_complete=%p)", in_fd, total_bytes, on_complete);
    SXEA10I(on_complete != NULL, "sxe_sendfile: on_complete callback function pointer can't be NULL");
    this->sendfile_in_fd    = in_fd;
    this->out_event_written = on_complete;

    sent = sendfile(this->socket, in_fd, NULL, total_bytes);

    if (sent < 0) {
        SXEL22I("sendfile() failed: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

        if (this->out_event_written) {
            (*this->out_event_written)(this, result);
        }

        goto SXE_EARLY_OUT;
    }

    if ((sent > 0) && ((unsigned)sent < total_bytes)) {
        SXEL82I("sendfile() sent %d bytes of %u, filling send buffer", sent, total_bytes);
        result = SXE_RETURN_IN_PROGRESS;
        this->sendfile_bytes = total_bytes - sent;

        ev_io_stop(sxe_private_main_loop, &this->io);
        ev_io_init(&this->io, sxe_io_cb_sendfile, this->socket_as_fd, EV_WRITE);
        ev_io_start(sxe_private_main_loop, &this->io);
        goto SXE_EARLY_OUT;
    }

    if ((unsigned)sent == 0) {
        SXEL80I("sendfile() sent 0 bytes (EOF)");
        result = SXE_RETURN_END_OF_FILE;
    }
    else {
        SXEL82I("sendfile() sent %d bytes of %u", sent, total_bytes);
        result = SXE_RETURN_OK;
    }

    if (this->out_event_written) {
        (*this->out_event_written)(this, result);
    }

    this->sendfile_in_fd    = -1;
    this->sendfile_bytes    = 0;
    this->out_event_written = NULL;

    ev_io_stop(sxe_private_main_loop, &this->io);
    ev_io_init(&this->io, sxe_io_cb_read, this->socket_as_fd, EV_READ);
    ev_io_start(sxe_private_main_loop, &this->io);

SXE_EARLY_OUT:
    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}
#endif

/* TODO: Make useful or delete
 */
void
sxe_dump(SXE * this)
{
    unsigned i;

    SXE_UNUSED_PARAMETER(this);

    SXEE80I("sxe_dump()");
    SXEL80I("Dumping sxes");

    for (i = 0; i < sxe_array_total; i++) {
        SXEL81I("SXE id=%u", i);
    }

    SXER80I("return");
}

/**
 * Close a TCP listener or connection or a UDP port.
 *
 * @param this  The SXE object to close
 *
 * @note This should not be called from a TCP close event callback. If a TCP connection is closed by the other end or the TCP
 *       stack, the SXE will be closed before the TCP close event is called back.
 */
SXE_RETURN
sxe_close(SXE * this)
{
    SXE_RETURN result = SXE_RETURN_OK;

    SXEE81I("sxe_close(this=%p)", this);
    SXEA80I(sxe_array != NULL,                                                "sxe_close: SXE is not initialized");
    SXEA80I((this >= sxe_array) && (this <= &sxe_array[sxe_array_total - 1]), "sxe_close: connection pointer is not valid");

    /* Close is idempotent with a warning.
     */
    if (sxe_pool_index_to_state(sxe_array, this->id) == SXE_STATE_FREE) {
        SXEL31I("sxe_close: ignoring close of a free connection object %u", this->id);
        result = SXE_RETURN_WARN_ALREADY_CLOSED;
        goto SXE_EARLY_OUT;
    }

    /* Close socket if open.
     */
    if (this->socket != SXE_SOCKET_INVALID) {
        /* Call CLOSESOCKET() as well as close() for Windows Sockets API compatibility.
         */
        SXEL82I("About to close fd %d (this=%p)", this->socket_as_fd, this);
        if (this->socket_as_fd >= 0) {
            close(this->socket_as_fd);
            this->socket_as_fd = -1;
        }
        CLOSESOCKET(this->socket);
#ifdef EV_MULTIPLICITY
        ev_io_stop(sxe_private_main_loop, &this->io);
#else
        ev_io_stop(&this->io);
#endif

        this->socket = SXE_SOCKET_INVALID;
    }

    this->in_total    = 0;
    this->in_consumed = 0;
    sxe_pool_set_indexed_element_state(sxe_array, this->id, SXE_STATE_USED, SXE_STATE_FREE);

SXE_EARLY_OUT:
    SXER81I("return %s", sxe_return_to_string(result));
    return result;
}

