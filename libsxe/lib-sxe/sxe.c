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
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__FreeBSD__)
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

#if defined(__APPLE__) || defined(__FreeBSD__)
# include <sys/uio.h>          /* For sendfile */
# include <sys/sysctl.h>       /* To find out the maximum socket buffer size */
# undef   EV_ERROR             /* remove duplicate definition from <sys/event.h> - conflicts with ev.h */
# define SIOCINQ FIONREAD
#else
# include <linux/sockios.h>    /* For SIOCINQ: This appears to be Linux specific; under FreeBSD, use MSG_PEEK */
# include <sys/sendfile.h>
#endif

#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/un.h>           /* For pipes (AKA UNIX domain sockets) on UNIX                                 */

#endif

#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-pool.h"
#include "sxe-socket.h"
#include "sxe-util.h"
#include "sxe-test.h"         /* for test_tap_ev_queue_shift_wait_get_deferred_count */

#ifndef _WIN32
#define PIPE_PATH_MAX sizeof(((struct sockaddr_un *)NULL)->sun_path)    /* Maximum size of a UNIX pipe path  */

#pragma GCC diagnostic ignored "-Wstrict-aliasing"    /* beause ev_*init() are broken */

typedef union SXE_CONTROL_MESSAGE_FD {
    struct cmsghdr alignment;
    char           control[CMSG_SPACE(sizeof(int))];
} SXE_CONTROL_MESSAGE_FD;
#endif
/* TODO: change sxld so that the udp packet is read only once by sxld (and not sxe and sxld) */

#define SXE_WANT_CALLER_READS_UDP 0

typedef enum SXE_STATE {
    SXE_STATE_FREE,
    SXE_STATE_USED,
    SXE_STATE_DEFERRED,
    SXE_STATE_NUMBER_OF_STATES
} SXE_STATE;

static const char *
sxe_state_to_string(unsigned state)                         /* coverage exclusion: state to string */
{                                                           /* coverage exclusion: state to string */
    switch (state) {                                        /* coverage exclusion: state to string */
        case SXE_STATE_FREE: return "FREE";                 /* coverage exclusion: state to string */
        case SXE_STATE_USED: return "USED";                 /* coverage exclusion: state to string */
        case SXE_STATE_DEFERRED: return "DEFERRED";         /* coverage exclusion: state to string */
        default: return NULL;                               /* coverage exclusion: state to string */
    }                                                       /* coverage exclusion: state to string */
}                                                           /* coverage exclusion: state to string */

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

static unsigned
get_deferred_count(void)
{
    return sxe_pool_get_number_in_state(sxe_array, SXE_STATE_DEFERRED);
}

static void
deferred_generic_invoke(EV_P)
{
    unsigned counter = 64;
    unsigned id;

    SXE_UNUSED_PARAMETER(loop);
    SXEE6("()");

    for (id = sxe_pool_set_oldest_element_state(sxe_array, SXE_STATE_DEFERRED, SXE_STATE_USED);
         id != SXE_POOL_NO_INDEX && counter-- != 0;
         id = sxe_pool_set_oldest_element_state(sxe_array, SXE_STATE_DEFERRED, SXE_STATE_USED))
    {
        SXE                * this  = &sxe_array[id];
        SXE_DEFERRED_EVENT   event = this->deferred_event;

        SXEA1(event != NULL, "Internal error: deferred a NULL event");
        this->deferred_event = NULL;
        (*event)(this);
    }

    SXER6("return // %u elements still deferred", get_deferred_count());
}

static void
deferred_generic_setup(SXE * this, SXE_DEFERRED_EVENT event)
{
    SXE_STATE state;
    SXEE6I("(event=%p)", event);

    SXEA6I(event != NULL, "%s(): event cannot be NULL", __func__);
    state = sxe_pool_index_to_state(sxe_array, this->id);
    if (state == SXE_STATE_DEFERRED) {
        if (this->deferred_event != event) {
            SXEL3I("Warning: overriding deferred event: old=%p new=%p (hint: how did you do that?!)", this->deferred_event, event); /* coverage exclusion: thought not to be possible */
        }
    }
    else {
        SXEA1I(this->deferred_event == NULL, "Internal error: unexpected non-NULL pointer %p; corruption?", this->deferred_event);
        sxe_pool_set_indexed_element_state(sxe_array, this->id, SXE_STATE_USED, SXE_STATE_DEFERRED);
        this->deferred_event = event;
    }

    SXER6I("return");
}

static void
deferred_resume_invoke(SXE * this)
{
    SXEE6I("()");
    SXEL6I("Invoking read event for %u bytes of cached data", SXE_BUF_USED(this));
    (*this->in_event_read)(this, SXE_BUF_USED(this));
    SXER6I("return");
}

static void
deferred_resume_setup(SXE * this)
{
    deferred_generic_setup(this, &deferred_resume_invoke);
}

#ifndef SXE_DISABLE_OPENSSL
static void
deferred_ssl_restart_reading_invoke(SXE * this)
{
    SXEE6I("()");
    sxe_ssl_private_restart_reading_buffer_drained(this);
    SXER6I("return");
}

static void
deferred_ssl_restart_reading_setup(SXE * this)
{
    deferred_generic_setup(this, &deferred_ssl_restart_reading_invoke);
}
#endif

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

    SXEE6("sxe_is_valid(this=%p)", this);
    SXEL7("SXE array %p, total %u and size %zu", sxe_array, sxe_array_total, sizeof(SXE) + sxe_extra_size);

    if ((this < sxe_array) || ((char *)this > (char *)sxe_array + (sxe_array_total - 1) * (sizeof(SXE) + sxe_extra_size))) {
        SXEL6("SXE pointer %p is not in the valid range %p to %p", this, sxe_array,
               (char *)sxe_array + (sxe_array_total - 1) * (sizeof(SXE) + sxe_extra_size));
        goto SXE_EARLY_OUT;
    }

    if (((char *)this - (char *)sxe_array) % (sizeof(SXE) + sxe_extra_size) != 0) {
        SXEL6("SXE pointer %p is not a multiple of %zu bytes from the base address %p", this, sizeof(SXE) + sxe_extra_size,
               sxe_array);
        goto SXE_EARLY_OUT;
    }

    valid = true;

SXE_EARLY_OUT:
    SXER6("return %s", SXE_BOOL_TO_STR(valid));
    return valid;
}

/**
 * Get the number of available SXEs
 *
 * @return the number of free SXEs
 */

unsigned
sxe_diag_get_free_count(void)
{
    return sxe_pool_get_number_in_state(sxe_array, SXE_STATE_FREE);
}

/**
 * Get the number of SXEs in use
 *
 * @return the number of used SXEs
 */

unsigned
sxe_diag_get_used_count(void)
{
    return sxe_pool_get_number_in_state(sxe_array, SXE_STATE_USED);
}

/**
 * Called by each protocol plugin before initialization.
 */
void
sxe_register(unsigned number_of_connections, unsigned extra_size)
{
    SXEE6("sxe_register(number_of_connections=%u,extra_size=%u)", number_of_connections, extra_size);
    sxe_array_total += number_of_connections;

    SXEA1(sxe_has_been_inited == 0, "SXE has already been init()'d");

    /* TODO: sxe_register() should assert if sxe_init() has already been called (because it signifies registration is too late) */

    if (extra_size > sxe_extra_size) {
        sxe_extra_size = extra_size;
    }

    SXER6("return");
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

    SXEE6("sxe_init()");
    SXEA1(!sxe_has_been_inited, "sxe_init: SXE is already initialized");
    SXEA1(sxe_array_total > 0,  "sxe_init: Error: sxe_register() must be called before sxe_init()");
    sxe_socket_init();

    test_tap_ev_queue_shift_wait_get_deferred_count = &get_deferred_count;

    /* TODO: Check that sxe_array_total is smaller than system ulimit -n */

    SXEL6("Allocating %u SXEs of %zu bytes each", sxe_array_total, sizeof(SXE) + sxe_extra_size);
    sxe_array = sxe_pool_new("sxe_pool", sxe_array_total, sizeof(SXE) + sxe_extra_size, SXE_STATE_NUMBER_OF_STATES,
                             SXE_POOL_OPTION_TIMED);
    sxe_pool_set_state_to_string(sxe_array, sxe_state_to_string);

    if (!sxe_private_main_loop) {
        /* Initialize libev, but don't let it take over the SIGCHLD signal.
         */
        SXEV6(sigaction(SIGCHLD, NULL, &sigaction_saved), == 0, "Unable to save SIGCHLD's sigaction: %s", strerror(errno));
        sxe_private_main_loop = ev_default_loop(0);
        SXEV6(sigaction(SIGCHLD, &sigaction_saved, NULL), == 0, "Unable to restore SIGCHLD's sigaction: %s", strerror(errno));
        SXEL6("got main loop = %p, backend=%s", (void*)sxe_private_main_loop,
            ev_backend(sxe_private_main_loop) == EVBACKEND_SELECT  ? "EVBACKEND_SELECT"  :
            ev_backend(sxe_private_main_loop) == EVBACKEND_POLL    ? "EVBACKEND_POLL"    :
            ev_backend(sxe_private_main_loop) == EVBACKEND_EPOLL   ? "EVBACKEND_EPOLL"   :
            ev_backend(sxe_private_main_loop) == EVBACKEND_KQUEUE  ? "EVBACKEND_KQUEUE"  :
            ev_backend(sxe_private_main_loop) == EVBACKEND_DEVPOLL ? "EVBACKEND_DEVPOLL" :
            ev_backend(sxe_private_main_loop) == EVBACKEND_PORT    ? "EVBACKEND_PORT"    : "Unknown backend!" );
    }

    ev_set_loop_release_cb (sxe_private_main_loop, deferred_generic_invoke, NULL); /* set release callback, NULL for acquire callback */


    sxe_has_been_inited = 1;
    result = SXE_RETURN_OK;
    SXER6("return %s", sxe_return_to_string(result));
    return result;
}

SXE_RETURN
sxe_fini(void)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    SXEE6("sxe_fini()");

    if (sxe_array == NULL) {
        SXEL6("sxe_fini(): called before successful sxe_init()");
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

    SXER6("return %d", result);
    return result;
}

void
sxe_private_set_watch_events(SXE * this, void (*func)(EV_P_ ev_io * io, int revents), int events, int stop)
{
    if (stop) {
        ev_io_stop(sxe_private_main_loop, &this->io);
    }

    ev_io_init(&this->io, func, this->socket_as_fd, events);
    this->io.data = this;
    ev_io_start(sxe_private_main_loop, &this->io);
}

static void
stop_reading_buffer_full(SXE * this)
{
    SXEL6I("Buffer is full: stopping read events");
    ev_io_stop(sxe_private_main_loop, &this->io);

#ifndef SXE_DISABLE_OPENSSL
    if (this->ssl_id != SXE_POOL_NO_INDEX) {
        sxe_ssl_private_stop_reading_buffer_full(this);
    }
#endif
}

static void
restart_reading_buffer_drained(SXE * this)
{
    SXEL6I("Buffer was paused: restarting read events");
    ev_io_start(sxe_private_main_loop, &this->io);

#ifndef SXE_DISABLE_OPENSSL
    if (this->ssl_id != SXE_POOL_NO_INDEX) {
        deferred_ssl_restart_reading_setup(this);
    }
#endif
}

static SXE *
sxe_new_internal(SXE                    * this              ,
                 struct sockaddr_in     * local_addr        ,
                 SXE_IN_EVENT_CONNECTED   in_event_connected,
                 SXE_IN_EVENT_READ        in_event_read     ,
                 SXE_IN_EVENT_CLOSE       in_event_close    ,
                 SXE_BOOL                 is_stream         ,
                 const char             * path              )
{
    unsigned id;
    SXE    * that = NULL;

    SXEE6I("sxe_new_internal(local_addr=%08x:%hu,in_event_connected=%p,in_event_read=%p,in_event_close=%p,is_stream=%s,path=%s)",
            local_addr->sin_addr.s_addr, ntohs(local_addr->sin_port), in_event_connected, in_event_read, in_event_close,
            SXE_BOOL_TO_STR(is_stream), path);

    if ((id = sxe_pool_set_oldest_element_state(sxe_array, SXE_STATE_FREE, SXE_STATE_USED)) == SXE_POOL_NO_INDEX) {
        SXEL3I("sxe_new: Warning: ran out of connections; concurrency too high");
        goto SXE_EARLY_OUT;
    }

    that                      = &sxe_array[id];
    that->id                  = id;
    that->path                = path;
    that->flags               = is_stream ? SXE_FLAG_IS_STREAM : 0;
    that->in_event_connected  = in_event_connected;
    that->in_event_read       = in_event_read;
    that->in_event_close      = in_event_close;
    that->deferred_event      = NULL;
    that->ssl_id              = SXE_POOL_NO_INDEX;
    that->socket_as_fd        = -1;
    that->socket              = SXE_SOCKET_INVALID;
    that->next_socket         = SXE_SOCKET_INVALID;
    that->in_total            = 0;
    that->in_consumed         = 0;
    memcpy(&that->local_addr, local_addr, sizeof(that->local_addr));

SXE_EARLY_OR_ERROR_OUT:
    SXER6I((that != NULL ? "%s%p // id=%u" : "%sNULL"), "return that=", that, (that != NULL ? that->id : ~0U));
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

    SXEE6I("sxe_new(local_ip=%s,local_port=%hu,is_stream=%s,path=%s)", local_ip, local_port, SXE_BOOL_TO_STR(is_stream), path);
    memset(&local_addr, 0, sizeof(local_addr));

#ifndef _WIN32
    if (path) {
        SXEA1I(strlen(path) < PIPE_PATH_MAX, "Unix domain socket path length is %u but must be less than %u characters",
               SXE_CAST(unsigned, strlen(path)), SXE_CAST(unsigned, PIPE_PATH_MAX));
        SXEL6I("caller wants new sxe using: pipe: unix domain socket path");
    }
    else
#endif
    {
        SXEL6I("caller wants new sxe using: inet: ip & port; i.e. tcp or udp");
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
#if SXE_DEBUG
    {
        unsigned that_id   = that == NULL ? ~0U :       that->id                  ;
        short    that_port = that == NULL ?  0  : ntohs(that->local_addr.sin_port);
        SXER6I("return that=%p; // id=%u, port=%hu", that, that_id, that_port);
    }
#endif
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

    SXEE6I("sxe_set_socket_options(sock=%d)", sock);
    SXEV6I(sxe_socket_set_nonblock(sock, 1), >= 0, "socket=%d: couldn't set non-blocking flag: %s", sock, sxe_socket_get_last_error_as_str());

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
        SXEV6I(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, SXE_WINAPI_CAST_CHAR_STAR &flags, sizeof(flags)),
            >= 0, "socket=%d: couldn't set reuse address flag: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
#endif
        //do we need this? flags = 1;
        //do we need this? SXEV6I(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, SXE_WINAPI_CAST_CHAR_STAR &flags, sizeof(flags)),
        //do we need this?        >= 0, "socket=%d: couldn't set keepalive flag: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

        linger_option.l_onoff  = 0;
        linger_option.l_linger = 0;
        SXEV6I(setsockopt(sock, SOL_SOCKET, SO_LINGER, SXE_WINAPI_CAST_CHAR_STAR &linger_option, sizeof(linger_option)),
               >= 0, "socket=%d: couldn't set linger to 0 secs: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

        flags = 1;
        SXEV6I(setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, SXE_WINAPI_CAST_CHAR_STAR &flags, sizeof(flags)),
               >= 0, "socket=%d: couldn't set TCP no delay flag: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

#ifdef __APPLE__
        /* Prevent firing SIGPIPE */
        flags = 1;
        SXEV6I(setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, SXE_WINAPI_CAST_CHAR_STAR &flags, sizeof(flags)),
                >= 0, "socket=%d: couldn't set SO_NOSIGPIPE flag: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
#endif
    }
    else {
        //debug SXEV6I(getsockopt(sock, SOL_SOCKET, SO_RCVBUF, SXE_WINAPI_CAST_CHAR_STAR &so_rcvbuf_size, &so_rcvbuf_size_length), >= 0, "socket=%d: couldn't get SO_RCVBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //debug SXEV6I(getsockopt(sock, SOL_SOCKET, SO_SNDBUF, SXE_WINAPI_CAST_CHAR_STAR &so_sndbuf_size, &so_sndbuf_size_length), >= 0, "socket=%d: couldn't get SO_SNDBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //debug SXEL1I("SO_RCVBUF %d, SO_SNDBUF %d", so_rcvbuf_size, so_sndbuf_size);

#ifdef __APPLE__
# define SXE_UDP_SO_RCVBUF_SIZE (     1024 * 1024)
#elif defined(__FreeBSD__)
# define SXE_UDP_SO_RCVBUF_SIZE (      256 * 1024) /* There *MUST* be a sysctl for this!? */
#else
# define SXE_UDP_SO_RCVBUF_SIZE (32 * 1024 * 1024)
#endif

        so_rcvbuf_size = SXE_UDP_SO_RCVBUF_SIZE;
        SXEV6I(setsockopt(sock, SOL_SOCKET, SO_RCVBUF, SXE_WINAPI_CAST_CHAR_STAR &so_rcvbuf_size, sizeof(so_rcvbuf_size)), >= 0, "socket=%d: couldn't get SO_RCVBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //do we need this? so_sndbuf_size = (128 * 1024 * 1024);
        //do we need this? SXEV6I(setsockopt(sock, SOL_SOCKET, SO_SNDBUF, SXE_WINAPI_CAST_CHAR_STAR &so_sndbuf_size, sizeof(so_sndbuf_size)), >= 0, "socket=%d: couldn't get SO_SNDBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

        so_rcvbuf_size = 0;
        SXEV6I(getsockopt(sock, SOL_SOCKET, SO_RCVBUF, SXE_WINAPI_CAST_CHAR_STAR &so_rcvbuf_size, &so_rcvbuf_size_length), >= 0, "socket=%d: couldn't get SO_RCVBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        if (so_rcvbuf_size < SXE_UDP_SO_RCVBUF_SIZE) {
            SXEL3I("Warning: could not set SO_RCVBUF to %u; instead it is set to %u; consider e.g. sysctl -w net.core.rmem_max=%u; continuing but packet loss will occur at higher loads", SXE_UDP_SO_RCVBUF_SIZE, so_rcvbuf_size, SXE_UDP_SO_RCVBUF_SIZE); /* COVERAGE EXCLUSION */
        }

        //debug SXEV6I(getsockopt(sock, SOL_SOCKET, SO_RCVBUF, SXE_WINAPI_CAST_CHAR_STAR &so_rcvbuf_size, &so_rcvbuf_size_length), >= 0, "socket=%d: couldn't get SO_RCVBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //debug SXEV6I(getsockopt(sock, SOL_SOCKET, SO_SNDBUF, SXE_WINAPI_CAST_CHAR_STAR &so_sndbuf_size, &so_sndbuf_size_length), >= 0, "socket=%d: couldn't get SO_SNDBUF: (%d) %s", sock, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        //debug SXEL1I("SO_RCVBUF %d, SO_SNDBUF %d", so_rcvbuf_size, so_sndbuf_size);
    }

SXE_EARLY_OR_ERROR_OUT:

    SXER6I("return");
}

void
sxe_private_handle_read_data(SXE * this, int length, int *reads_remaining)
{
    SXEE6I("(length=%d)", length);

    SXED7I(this->in_buf + this->in_total, length);
    this->in_total += length;

    /* for now... quick and dirty statistics */
    sxe_stat_total_read++;

    //debug if ((sxe_stat_total_read % 5000) == 0) {
    //debug     SXEL5I("sxe_stat_total_read=%u, sxe_stat_total_accept=%u", sxe_stat_total_read, sxe_stat_total_accept); /* COVERAGE EXCLUSION: Statistics */
    //debug }

    if (!(this->flags & SXE_FLAG_IS_PAUSED)) {
        SXEL7I("About to pass read event up (%u new bytes in buffer)", length);
        (*this->in_event_read)(this, length);
        if (reads_remaining) {
            --*reads_remaining;
        }
    }

    /* If the buffer is full, SXE must wait for the caller to clear it before asking ev for more EVREAD events.
     */
    if (this->in_total == SXE_BUF_SIZE) {
        if (this->in_consumed) {
            SXEL6I("Shuffling the buffer to make room for more data");
            memmove(this->in_buf, SXE_BUF(this), SXE_BUF_USED(this));
            this->in_total -= this->in_consumed;
            this->in_consumed = 0;
        }
        else {
            stop_reading_buffer_full(this);
        }
    }

    SXER6I("return");
}

#define SXE_IO_CB_READ_MAXIMUM 64

static void
sxe_io_cb_read(EV_P_ ev_io * io, int revents)
{
    SXE                    * this           = (SXE *)io->data;
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

    SXEE6I("sxe_io_cb_read(this=%p, revents=%u) // socket=%d", this, revents, this->socket);

    if (this->socket == SXE_SOCKET_INVALID) {
        SXEL1I("ERROR: Unexpected read event on closed SXE object");
        goto SXE_EARLY_OUT;
    }

    if (revents == EV_READ) {
SXE_TRY_AND_READ_AGAIN:
        if (this->path) {
#ifndef _WIN32
            memset(&message_header, 0, sizeof message_header);

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
                    SXEL6I("Read %d bytes from peer via path %s without a control message", length, this->path);
                }
                else if (control_message_ptr->cmsg_len != CMSG_LEN(sizeof(fd_from_recvmsg))) {
                    SXEL2("sxe_io_cb_read(): control message length is %u, expected %u",        /* Coverage Exclusion: TODO */
                           SXE_CAST(unsigned, CMSG_LEN(sizeof(fd_from_recvmsg))),               /* Coverage Exclusion: TODO */
                           SXE_CAST(unsigned, control_message_ptr->cmsg_len));                  /* Coverage Exclusion: TODO */
                }
                else if (control_message_ptr->cmsg_level != SOL_SOCKET) {
                    SXEL2("sxe_io_cb_read(): control message level %d != SOL_SOCKET", control_message_ptr->cmsg_level);    /* Coverage Exclusion: TODO */
                }
                else if (control_message_ptr->cmsg_type != SCM_RIGHTS) {
                    SXEL2("sxe_io_cb_read(): control message type %d != SCM_RIGHTS", control_message_ptr->cmsg_type);    /* Coverage Exclusion: TODO */
                }
                else {
                    if (this->next_socket >= 0) {
                        SXEL3I("sxe_io_cb_read(): Received fd %d on pipe, and already received %d",    /* Coverage Exclusion: TODO */
                                *(int *)(void *)CMSG_DATA(control_message_ptr), this->next_socket);             /* Coverage Exclusion: TODO */
                    }

                    SXEL6I("Read %d bytes from peer via path %s with a control message (fd=%d)", length, this->path, this->next_socket);
                    this->next_socket = *(int *)(void *)CMSG_DATA(control_message_ptr);
                }

                if (this->next_socket >= 0) {
                    getpeername(this->next_socket, (struct sockaddr *)&this->peer_addr, &peer_addr_size);
                    SXEA6I(this->peer_addr.sin_family == AF_UNIX || peer_addr_size == sizeof(this->peer_addr), "Peer address is not an IPv4 address (peer_addr_size=%d)",
                            peer_addr_size);

                    if (ioctl(this->socket, SIOCINQ, &input_data_left) < 0) {
                        SXEL2("sxe_io_cb_read(): ioctl(socket=%d,SIOCINQ) failed: %s", this->socket,    /* Coverage Exclusion: TODO */
                               sxe_socket_error_as_str(errno));                                          /* Coverage Exclusion: TODO */
                    }
                    else if (input_data_left == 0) {
                        SXEL6I("No more data on pipe: closing pipe socket=%d", this->socket);
                        close(this->socket);
                        this->socket       = this->next_socket;
                        this->socket_as_fd = this->next_socket;    /* Same thing, since pipes are UNIX only    */
                        this->next_socket  = -1;                   /* Won't be used again, but clear it anyway */
                        this->path         = NULL;
                        this->flags       |=  SXE_FLAG_IS_STREAM;

                        ev_async_init(&this->async, NULL);
                        sxe_watch_read(this);
                        SXEL6I("Set connection to use received TCP socket=%d", this->socket);
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
                (*this->in_event_read)(this, 0); /* COVERAGE EXCLUSION: TODO */
            } while (sxe_caller_read_udp_length > 0);

            length = sxe_caller_read_udp_length; /* COVERAGE EXCLUSION: TODO */
        }
#endif
        else {
            length = recvfrom(this->socket, this->in_buf + this->in_total, sizeof(this->in_buf) - this->in_total, 0,
                              (struct sockaddr *)&this->peer_addr, &peer_addr_size);
            last_socket_error = sxe_socket_get_last_error();
            SXEA6I(peer_addr_size == sizeof(this->peer_addr), "Peer address is not an IPV4 address (peer_addr_size=%d)",
                    peer_addr_size);
            SXEL6I("Read %d bytes from peer IP %s:%hu", length, inet_ntoa(this->peer_addr.sin_addr),
                    ntohs(this->peer_addr.sin_port));
        }

        if (length > 0) {
            sxe_private_handle_read_data(this, length, &reads_remaining_this_event);

            if (!(this->flags & SXE_FLAG_IS_STREAM) && reads_remaining_this_event) {
                goto SXE_TRY_AND_READ_AGAIN;
            }
        }
        else {
            if (length < 0) {
                switch ((last_socket_error == 0) ? sxe_socket_get_last_error() : last_socket_error) {

                case SXE_SOCKET_ERROR(EWOULDBLOCK): // EAGAIN
                    SXEL6I("socket=%d is not ready", this->socket);
                    goto SXE_EARLY_OUT;

                /* Expected error codes:
                 */
                case SXE_SOCKET_ERROR(ECONNRESET):
                    SXEL6I("Failed to read from socket=%d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
                    break; /* Coverage Exclusion: TODO */

                /* Should never get here.
                 */
                default:
                    SXEL2I("Failed to read from socket=%d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
                }
            }

            /* Under Linux, a received disconnect request may lead to a zero byte read.
             */
            else {
                SXEL6I("Failed to read from socket=%d: (0) zero bytes read - disconnect", this->socket);
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
                    SXEL7I("No close event callback: cannot report");
                }
            }
        }
    }

SXE_EARLY_OR_ERROR_OUT:
    SXER6I("return");
}

void
sxe_buf_clear(SXE * this)
{
    SXEE6I("sxe_buf_clear()");

    this->in_consumed = 0;
    this->in_total    = 0;

    if ((-1 != this->socket_as_fd)
    &&  (!ev_is_active(&this->io)))
    {
        restart_reading_buffer_drained(this);
    }

    SXER6I("return");
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
    SXEE6I("sxe_buf_consume(bytes=%u)", bytes);
    SXEA6I(bytes <= SXE_BUF_SIZE,       "attempt to consume %u bytes, which is more than SXE_BUF_SIZE (%u)", bytes, SXE_BUF_SIZE);
    SXEA6I(bytes <= SXE_BUF_USED(this), "attempt to consume %u bytes, which is more than SXE_BUF_USED (%u)", bytes, SXE_BUF_USED(this));
    this->in_consumed += bytes;
    sxe_pause(this);

    if (this->in_consumed == this->in_total) {
        SXEL6I("Consumed the whole buffer; clearing");
        sxe_buf_clear(this);
    }

    SXER6I("return");
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
    SXEE6I("%s(this=%p, invoke_callback=%s)", __func__, this,
            invoke_callback == SXE_BUF_RESUME_IMMEDIATE ? "IMMEDIATE" : "WHEN_MORE_DATA");
    this->flags &= ~SXE_FLAG_IS_PAUSED;

    /* If we've filled the buffer and stopped reading, we need to ensure there
     * is room for more data to read, otherwise we won't ever actually get
     * more data! */
    if (this->in_consumed && this->in_total == SXE_BUF_SIZE) {
        SXEL6I("Watcher was paused: making room for more data, and restarting read events");
        memmove(this->in_buf, SXE_BUF(this), SXE_BUF_USED(this));
        this->in_total -= this->in_consumed;
        this->in_consumed = 0;
        ev_io_start(sxe_private_main_loop, &this->io);
    }

    if (invoke_callback == SXE_BUF_RESUME_IMMEDIATE && SXE_BUF_USED(this) != 0) {
        deferred_resume_setup(this);
    }
    else {
        SXEL6I("Ignoring SXE_BUF_RESUME_IMMEDIATE: buffer is empty: in_total=%u", this->in_total);
    }

    SXER6I("return");
}

static void
sxe_io_cb_accept(EV_P_ ev_io * io, int revents)
{
    SXE              * this           = (SXE *)io->data;
    SXE              * that           = NULL;
    SXE_SOCKET         that_socket    = SXE_SOCKET_INVALID;
    struct sockaddr_in peer_addr;
    SXE_SOCKLEN_T      peer_addr_size = sizeof(peer_addr);

#if EV_MULTIPLICITY
    SXE_UNUSED_PARAMETER(loop);
#endif
    SXE_UNUSED_PARAMETER(revents);

    SXEE6I("sxe_io_cb_accept(this=%p, revents=%u) // socket=%d", this, revents, this->socket);
    SXEA6I(revents == EV_READ, "sxe_io_cb_accept: revents is not EV_READ");

    do {
        /* TODO: Track SXE & open socket count and disable / enable accept() event as appropriate */
        SXEL7I("Accept()");

        if ((that_socket = accept(this->socket, (struct sockaddr *)&peer_addr, &peer_addr_size)) == SXE_SOCKET_INVALID)
        {
            if (sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EWOULDBLOCK)) {
                SXEL6I("No more connections to accept on listening socket=%d", this->socket);
                goto SXE_EARLY_OUT;
            }

            SXEL2I("Error accepting on socket=%d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
            goto SXE_ERROR_OUT;
        }

#if SXE_DEBUG
#   ifndef _WIN32
        if (this->path) {
#       if defined(__APPLE__) || defined(__FreeBSD__)
            uid_t euid;
            gid_t egid;

            SXEV6(getpeereid(that_socket, &euid, &egid), >= 0, "could not obtain credentials from unix domain socket");

            SXEL6I("Accepted connection from uid %d, gid %d",
                euid,  /* the effective UID         of the process on the other side of the socket */
                egid); /* the effective primary GID of the process on the other side of the socket */
#       else /* neither __APPLE__ nor __FreeBSD__ */
            struct ucred credentials;
            unsigned ucred_length = sizeof(struct ucred);

            SXEV6I(getsockopt(that_socket, SOL_SOCKET, SO_PEERCRED, &credentials, &ucred_length), >= 0, "could obtain credentials from unix domain socket");

            SXEL6I("Accepted connection from pid %d, uid %d, gid %d",
                credentials.pid,  /* the process ID            of the process on the other side of the socket */
                credentials.uid,  /* the effective UID         of the process on the other side of the socket */
                credentials.gid); /* the effective primary GID of the process on the other side of the socket */
#       endif /* neither __APPLE__ nor __FreeBSD__ */
        }
        else
#   endif /* !defined(_WIN32) */
        {
            SXEL6I("Accepted connection from ip=%d.%d.%d.%d:%hu",
                    (ntohl(peer_addr.sin_addr.s_addr) >> 24) & 0xff,
                    (ntohl(peer_addr.sin_addr.s_addr) >> 16) & 0xff,
                    (ntohl(peer_addr.sin_addr.s_addr) >> 8 ) & 0xff,
                     ntohl(peer_addr.sin_addr.s_addr)        & 0xff,
                     ntohs(peer_addr.sin_port       )              );
        }
#endif /* defined(SXE_DEBUG) */

        sxe_set_socket_options(this, that_socket);

        if (this->flags & SXE_FLAG_IS_ONESHOT) {
            SXEL6I("Closing one-shot listening socket and replacing with accepted socket");
            ev_io_stop(sxe_private_main_loop, &this->io);
            close(this->socket_as_fd);
            this->socket_as_fd = -1;
            CLOSESOCKET(this->socket);
            that = this;
        }
        else {
            SXEL6I("Creating a new SXE");
            that = sxe_new_internal(this, &this->local_addr, this->in_event_connected, this->in_event_read,
                                    this->in_event_close, this->flags & SXE_FLAG_IS_STREAM, this->path);

            if (that == NULL) {
                SXEL3I("Warning: failed to allocate a connection from socket=%d: out of connections", this->socket);
                goto SXE_ERROR_OUT;
            }
        }

        that->socket               = that_socket;
        that->socket_as_fd         = _open_osfhandle(that_socket, 0);
        SXE_USER_DATA_AS_INT(that) = SXE_USER_DATA_AS_INT(this);
        memcpy(&that->peer_addr, &peer_addr, sizeof(that->peer_addr));

        SXEL6I("add accepted connection to watch list, socket==%d, socket_as_fd=%d", that_socket, that->socket_as_fd);
        ev_async_init(&that->async, NULL);
        sxe_private_set_watch_events(that, sxe_io_cb_read, EV_READ, 0);
        sxe_stat_total_accept++;

#ifndef SXE_DISABLE_OPENSSL
        if (this->flags & SXE_FLAG_IS_SSL) {
            that->flags |= SXE_FLAG_IS_SSL;
            sxe_ssl_accept(that);
        }
        else
#endif
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
    SXER6I("return");
}

static void
sxe_io_cb_connect(EV_P_ ev_io * io, int revents)
{
    SXE                * this                    = (SXE *)io->data;
    int                  error;
    SXE_SOCKLEN_T        error_length = sizeof(error);

    SXEE6I("sxe_io_cb_connect(this=%p, revents=%u) // socket=%d, EV_READ=0x%x, EV_WRITE=0x%x, EV_ERROR=0x%x", this, revents,
            this->socket, EV_READ, EV_WRITE, EV_ERROR);

#if EV_MULTIPLICITY
    SXE_UNUSED_PARAMETER(loop);
#endif
    SXE_UNUSED_PARAMETER(revents);

    if (getsockopt(this->socket, SOL_SOCKET, SO_ERROR, (void*)&error, &error_length) == SXE_SOCKET_ERROR_OCCURRED) {
        SXEL2I("Failed to get error indication (socket=%d): %s", this->socket, sxe_socket_get_last_error_as_str());
        goto SXE_ERROR_OUT;
    }

    SXEA6I(error_length == sizeof(error), "Unexpected size of result returned by getsockopt(): %d bytes", error_length);

    if (error != 0) {
        SXEL5I("Failed to connect to %s:%hu: %s", inet_ntoa(SXE_PEER_ADDR(this)->sin_addr), SXE_PEER_PORT(this),
               sxe_socket_error_as_str(error));
        goto SXE_ERROR_OUT;
    }

    SXEL6I("setup read event to call sxe_io_cb_read callback (socket==%d)", this->socket);
    sxe_watch_read(this);

#ifndef SXE_DISABLE_OPENSSL
    if (this->flags & SXE_FLAG_IS_SSL) {
        sxe_ssl_connect(this);
        goto SXE_EARLY_OUT;
    }
#endif

    SXEL6I("connection complete; notify sxe application");

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
    SXER6I("return");
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

    SXEA1I(this->path == NULL, "sxe_get_local_addr: SXE is a pipe '%s'", this->path);
    SXEE6I("%s(this=%p)", __func__, this);

    if (this->local_addr.sin_port != 0) {
        goto SXE_EARLY_OUT;
    }

    if (getsockname(this->socket, (struct sockaddr *)&this->local_addr, &address_length) < 0 ) {
        SXEL2I("%s: getsockname() failed: %s\n", __func__, sxe_socket_get_last_error_as_str());    /* COVERAGE EXCLUSION : TODO - when we are able to mock */
    }

SXE_EARLY_OUT:
    SXER6I("return %p", &this->local_addr);
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

    SXEE6I("sxe_listen(this=%p, flags=%u)", this, flags);
    SXEA1I(this  != NULL,                   "sxe_listen: object pointer is NULL");
    SXEA1I(!sxe_is_free(this),              "sxe_listen: connection has not been allocated (state=FREE)");
    SXEA1I(!(flags & ~SXE_FLAG_IS_ONESHOT), "sxe_listen: a flag other than SXE_FLAG_IS_ONESHOT was given: 0x%08x",
            flags & ~SXE_FLAG_IS_ONESHOT);

    if (this->socket != SXE_SOCKET_INVALID) {
        SXEL2I("Listener is already in use (socket=%d)", this->socket);
        goto SXE_EARLY_OUT;
    }

    socket_listen = socket(this->path ? AF_UNIX : AF_INET, ((this->flags & SXE_FLAG_IS_STREAM) ? SOCK_STREAM : SOCK_DGRAM), 0);

    if (socket_listen == SXE_SOCKET_INVALID) {
        SXEL2I("Error creating listening socket: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        goto SXE_EARLY_OUT;
    }

    SXEL6I("socket=%d == listen_socket = socket(%s, %s, 0)", socket_listen, this->path ? "AF_UNIX" : "AF_INET",
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

        SXEL2I("Error binding address to listening socket: inet: address=%s:%hu, path=%s: (errno=%d) %s",
                inet_ntoa(this->local_addr.sin_addr), ntohs(this->local_addr.sin_port), this->path, error, sxe_socket_error_as_str(error));
        sxe_socket_set_last_error(error);

        if (error == SXE_SOCKET_ERROR(EADDRINUSE)) {
            result = SXE_RETURN_ERROR_ADDRESS_IN_USE;
        }

        goto SXE_EARLY_OUT;
    }

    if ((this->flags & SXE_FLAG_IS_STREAM) && (listen(socket_listen, sxe_listen_backlog) < 0)) {
        SXEL2I("Error listening on socket: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        goto SXE_EARLY_OUT;
    }

    this->flags       |= flags;            /* Set one-shot, if specified by the caller */
    this->socket       = socket_listen;
    this->socket_as_fd = _open_osfhandle(socket_listen, 0);
    socket_listen      = SXE_SOCKET_INVALID;
    SXEL6I("connection id of new listen socket is %d, socket_as_fd=%d, backlog=%d, address=%s:%hu, path=%s", this->id,
            this->socket_as_fd, sxe_listen_backlog, inet_ntoa(this->local_addr.sin_addr), ntohs(this->local_addr.sin_port), this->path);

    ev_async_init(&this->async, NULL);
    sxe_private_set_watch_events(this, ((this->flags & SXE_FLAG_IS_STREAM) ? sxe_io_cb_accept : sxe_io_cb_read), EV_READ, 0);

    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    if (socket_listen != SXE_SOCKET_INVALID) {
        SXEL6I("socket=%d is unusable, so must be closed", socket_listen);
        CLOSESOCKET(socket_listen);
    }

    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

void
sxe_set_listen_backlog(int listen_backlog)
{
    SXEE6("sxe_set_listen_backlog(listen_backlog=%d)", listen_backlog);
    SXEL6("backlog was %d; now setting to %d", sxe_listen_backlog, listen_backlog);
    sxe_listen_backlog = listen_backlog;
    SXER6("return");
}

#ifdef WINDOWS_NT
#else
SXE_RETURN
sxe_connect_pipe(SXE * this)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;

    SXEE6I("sxe_connect_pipe(this=%p) // socket=%d, path=%s", this, this->socket, this->path);

    result = sxe_connect(this, NULL, 0);

    SXER6I("return %s", sxe_return_to_string(result));
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

    SXEE6I("sxe_connect(this=%p, peer_ip==%s, peer_port=%hu) // socket=%d, path=%s", this, peer_ip, peer_port, this->socket,
            this->path);
    SXEA1I(this != NULL,       "sxe_connect: connection pointer is NULL");
    SXEA1I(!sxe_is_free(this), "sxe_connect: connection has not been allocated (state=FREE)");

    if (this->socket != SXE_SOCKET_INVALID) {
        SXEL2I("Connection is already in use (socket=%d)", this->socket);
        result = SXE_RETURN_ERROR_ALREADY_CONNECTED;
        goto SXE_EARLY_OUT;
    }

    SXEL6I("TODO: Add a timeout parameter");

    if ((that_socket = socket(this->path ? AF_UNIX : PF_INET, SOCK_STREAM, this->path ? 0 : IPPROTO_TCP)) == SXE_SOCKET_INVALID) {
        SXEL2I("error creating socket: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        goto SXE_ERROR_OUT;
    }

#ifndef _WIN32
    if (this->path) {

        /* todo: position in code to implement equivalent to unix domain socket / fd passing on winnt */
        pipe_address.sun_family = AF_UNIX;
        strcpy(pipe_address.sun_path, this->path);

        peer_address        = (struct sockaddr *)&pipe_address;
        peer_address_length = SUN_LEN(&pipe_address);
    }
    else
#endif
    {
        /* From ip(7) - Linux man page:
         *   When a process wants to receive new incoming packets or connections, it should bind a socket to a local interface address using bind(2).
         *   Only one IP socket may be bound to any given local (address, port) pair. When INADDR_ANY is specified in the bind call the socket will
         *   be bound to *ALL* local interfaces. When listen(2) or connect(2) are called on an unbound socket, it is automatically bound to a random
         *   free port with the local address set to *INADDR_ANY*.
         * Therefore: If the local address is not INADDR_ANY:0, bind it to the socket. */

        /* Update: It is true that if you don't call bind() before connect(), INADDR_ANY:0 will be assumed, but that doesn't mean
         *  you *can't* call bind() with one or both of INADDR_ANY or port 0.
        if (this->local_addr.sin_port == 0) {
            SXEA1I(this->local_addr.sin_addr.s_addr == htonl(INADDR_ANY), "Port must be non-zero if IP is not INADDR_ANY; see ip(7) Linux man page");
            SXEL6I("Connecting using local TCP port: randomly allocated by the stack on IP INADDR_ANY");
        } */

        if (this->local_addr.sin_port == 0 && this->local_addr.sin_addr.s_addr == htonl(INADDR_ANY)) {
            SXEL6I("Connecting using local TCP port: randomly allocated by the stack on IP INADDR_ANY");
        }
        else {
            if (this->local_addr.sin_port == 0) {
                SXEL6I("Connecting using a random TCP port");
            } else {
                SXEL6I("Connecting using local TCP port: %hu", ntohs(this->local_addr.sin_port));
            }

            if (bind(that_socket, (struct sockaddr *)&this->local_addr, sizeof(this->local_addr)) == SXE_SOCKET_ERROR_OCCURRED) {
                if (sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EADDRINUSE)) {
                    SXEL3I("Can't connect using local TCP port: %hu (%d) %s", ntohs(this->local_addr.sin_port),
                            sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
                    result = SXE_RETURN_ERROR_ADDRESS_IN_USE;
                }
                else {
                    SXEL2I("Unexpected error calling bind: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
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

    SXEL6I("add connection to watch list, socket==%d, socket_as_fd=%d", that_socket, _open_osfhandle(that_socket, 0));
    ev_async_init(&this->async, NULL);
    ev_io_init(&this->io, sxe_io_cb_connect, _open_osfhandle(that_socket, 0), EV_WRITE);
    this->io.data = this;
    ev_io_start(sxe_private_main_loop, &this->io);

    if ((connect(that_socket, peer_address, peer_address_length) == SXE_SOCKET_ERROR_OCCURRED)
            && (sxe_socket_get_last_error() != SXE_SOCKET_ERROR(EINPROGRESS))
            && (sxe_socket_get_last_error() != SXE_SOCKET_ERROR(EWOULDBLOCK))) {
        SXEL2I("connect failed: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        ev_io_stop(sxe_private_main_loop, &this->io);
        goto SXE_ERROR_OUT;
    }

    SXEL6I("Connection to server: ip:port %s:%hu, path %s: %s", peer_ip, peer_port, this->path,
            ((sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EINPROGRESS) || sxe_socket_get_last_error() == SXE_SOCKET_ERROR(EWOULDBLOCK)) ? "in progress" : "completed"));
    this->socket       = that_socket;
    this->socket_as_fd = _open_osfhandle(that_socket, 0);

#if SXE_DEBUG
    /* Display the local socket's bound port and IP; TODO: use sxe_get_local_addr and inet_ntoa */
    if (this->path != NULL) {
        struct sockaddr_in local_addr;
        SXE_SOCKLEN_T      address_length = sizeof(local_addr);

        if (getsockname(that_socket, (struct sockaddr *)&local_addr, &address_length) >= 0 ) {
            SXEL6I("Connection id of new connecting socket is %d, socket_as_fd=%d, ip=%d.%d.%d.%d:%hu", this->id,
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
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

#ifdef WINDOWS_NT
#else
SXE_RETURN
sxe_write_pipe(SXE * this, const void * data, unsigned size, int fd_to_send)
{
    SXE_RETURN               result = SXE_RETURN_ERROR_INTERNAL;
    SXE_CONTROL_MESSAGE_FD   control_message_buf;
    struct msghdr            message_header;
    struct cmsghdr         * control_message_ptr;
    struct iovec             io_vector[1];           /* This is intentional. It's a vector of length 1 */

    SXEA6I(this != NULL, "sxe_write_pipe(): connection pointer is NULL");
    SXEA6I(this->path  , "sxe_write_pipe(): connection is not a unix domain socket");
    SXEA6I(size != 0,    "sxe_write_pipe(): Must send at least 1 byte of data");
    SXEE6I("sxe_write_pipe(this=%p,size=%u,fd_to_send=%d) // socket=%d", this, size, fd_to_send, this->socket);
    SXED7I(data, size);

    memset(&message_header, 0, sizeof message_header);

    /* This may be the worst API yet: move over Windows.
     */
    io_vector[0].iov_base                  = SXE_CAST_NOCONST(void *, data);
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
    *(int *)(void *)CMSG_DATA(control_message_ptr) = fd_to_send;

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
        SXEL2I("sxe_write_pipe(): Error writing to socket=%d: (%d) %s", this->socket,    /* Coverage Exclusion: TODO */
                sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());         /* Coverage Exclusion: TODO */
        goto SXE_ERROR_OUT; /* Coverage Exclusion: TODO */
    }

    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}
#endif

SXE_RETURN
sxe_write_to(SXE * this, const void * data, unsigned size, const struct sockaddr_in * dest_addr)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    int        ret;

    SXEA6I(this != NULL,                        "sxe_write_to(): connection pointer is NULL");
    SXEA6I(!(this->flags & SXE_FLAG_IS_STREAM), "sxe_write_to(): SXE is not a UDP SXE");
    SXEE6I("sxe_write_to(this=%p,size=%u,sockaddr_in=%s:%hu) // socket=%d", this, size, inet_ntoa(dest_addr->sin_addr),
            ntohs(dest_addr->sin_port), this->socket);
    SXED7I(data, size);

    if ((ret = sendto(this->socket, data, size, 0, (const struct sockaddr *)dest_addr, sizeof(*dest_addr))) != (int)size) {
        if (ret >= 0) {
            SXEL2I("sxe_write_to(): Only %d of %u bytes written to socket=%d", ret, size, this->socket);   /* COVERAGE EXCLUSION: Logging for UDP truncation on sendto */
        }
        else {
            SXEL2I("sxe_write_to(): Error writing to socket=%d: (%d) %s", this->socket, sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
        }

        goto SXE_ERROR_OUT;
    }

    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

/* TODO: Implement in terms of a helper that takes a pointer to bytes written as a parameter */

SXE_RETURN
sxe_write(SXE * this, const void * data, unsigned size)
{
    SXE_RETURN  result = SXE_RETURN_ERROR_INTERNAL;
    int         ret;
    int         socket_error;

    SXEA6I(this != NULL,                       "SXE pointer is NULL");
    SXEA6I((this->flags & SXE_FLAG_IS_STREAM), "SXE is not a stream SXE");
    SXEA6I(this->ssl_id == SXE_POOL_NO_INDEX,  "sxe_write() on an SSL socket: use sxe_send()");
    SXEE6I("sxe_write(data=%p, size=%u)", data, size);

    if (this->socket == SXE_SOCKET_INVALID) {
        SXEL2I("Send on a disconnected socket");
        result = SXE_RETURN_ERROR_NO_CONNECTION;
        goto SXE_ERROR_OUT;
    }

    /* Use send(), not write(), for WSA (Windows Socket API) compatibility
     *
     * Note: Use SXE_SOCKET_MSG_NOSIGNAL to avoid rare occurrence when connection closed by peer but locally the TCP
     *       stack thinks the connection is still open.  On Windows, this is the normal behaviour.
     */
    if ((ret = send(this->socket, data, size, SXE_SOCKET_MSG_NOSIGNAL)) != (int)size) {
        if (ret >= 0) {
            SXED7I(data, ret);
            SXEL6I("sxe_write(): Only %d of %u bytes written to socket=%d", ret, size, this->socket);
            this->last_write = ret;
            result = SXE_RETURN_WARN_WOULD_BLOCK; /* Coverage Exclusion - todo: win32 coverage */
        }
        else {
            socket_error = sxe_socket_get_last_error();
            SXEL2I("sxe_write(): Error writing to socket=%d: (%d) %s", this->socket, socket_error, sxe_socket_get_last_error_as_str());
            this->last_write = 0;

            if ((socket_error == SXE_SOCKET_ERROR(ECONNRESET  ))
             || (socket_error == SXE_SOCKET_ERROR(ECONNREFUSED))
             || (socket_error == SXE_SOCKET_ERROR(ENOTCONN)))        /* Windows */
            {
                result = SXE_RETURN_ERROR_NO_CONNECTION;
            }
            else if (socket_error == SXE_SOCKET_ERROR(EWOULDBLOCK)) {
                result = SXE_RETURN_WARN_WOULD_BLOCK;
            }
        }
        goto SXE_ERROR_OUT;
    }

    this->last_write = size;
    SXED7I(data, size);
    SXEL6I("Wrote %u bytes to socket=%d", size, this->socket);
    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

/* Shared between sxe_io_cb_send_buffers and sxe_send_buffers
 */
static SXE_RETURN
sxe_send_buffers_again(SXE * this)
{
    SXE_RETURN   result = SXE_RETURN_OK;
    SXE_BUFFER * buffer;

    SXEE6I("sxe_send_buffers_again(this=%p) // socket=%d", this, this->socket);
    buffer = sxe_list_walker_find(&this->send_list_walk);

    while (buffer != NULL) {
        result = sxe_write(this, sxe_buffer_get_data(buffer), sxe_buffer_length(buffer));

        if (result != SXE_RETURN_OK && result != SXE_RETURN_WARN_WOULD_BLOCK) {
            break;
        }

        sxe_buffer_consume(buffer, this->last_write);

        if (sxe_buffer_length(buffer) == 0) {
            buffer = sxe_list_walker_step(&this->send_list_walk);
        }

        if (result == SXE_RETURN_WARN_WOULD_BLOCK) {                                                                               /* COVERAGE EXCLUSION: Debian 8 */
            result = SXE_RETURN_IN_PROGRESS;                                                                                       /* COVERAGE EXCLUSION: Debian 8 */
            SXEL6("sxe_write wrote %u bytes with %u left; scheduling another write", this->last_write, sxe_buffer_length(buffer)); /* COVERAGE EXCLUSION: Debian 8 */
            sxe_watch_write(this);                                                                                                 /* COVERAGE EXCLUSION: Debian 8 */
            break;                                                                                                                 /* COVERAGE EXCLUSION: Debian 8 */
        }                                                                                                                          /* COVERAGE EXCLUSION: Debian 8 */
    }

    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

static void
sxe_io_cb_send_buffers(EV_P_ ev_io * io, int revents)                                                   /* COVERAGE EXCLUSION: Debian 8 */
{                                                                                                       /* COVERAGE EXCLUSION: Debian 8 */
    SXE_RETURN result = SXE_RETURN_OK;                                                                  /* COVERAGE EXCLUSION: Debian 8 */
    SXE * this = (SXE *)io->data;                                                                       /* COVERAGE EXCLUSION: Debian 8 */
    SXE_UNUSED_PARAMETER(revents);                                                                      /* COVERAGE EXCLUSION: Debian 8 */
                                                                                                        /* COVERAGE EXCLUSION: Debian 8 */
#if EV_MULTIPLICITY                                                                                     /* COVERAGE EXCLUSION: Debian 8 */
    SXE_UNUSED_PARAMETER(loop);                                                                         /* COVERAGE EXCLUSION: Debian 8 */
#endif                                                                                                  /* COVERAGE EXCLUSION: Debian 8 */
                                                                                                        /* COVERAGE EXCLUSION: Debian 8 */
    SXEE6I("sxe_io_cb_send_buffers(this=%p, revents=%u) // socket=%d", this, revents, this->socket);    /* COVERAGE EXCLUSION: Debian 8 */
    result = sxe_send_buffers_again(this);                                                              /* COVERAGE EXCLUSION: Debian 8 */
                                                                                                        /* COVERAGE EXCLUSION: Debian 8 */
    if (result != SXE_RETURN_IN_PROGRESS) {                                                             /* COVERAGE EXCLUSION: Debian 8 */
        SXEL6I("Re-enabling read events on this SXE");                                                  /* COVERAGE EXCLUSION: Debian 8 */
        sxe_watch_read(this);                                                                           /* COVERAGE EXCLUSION: Debian 8 */
                                                                                                        /* COVERAGE EXCLUSION: Debian 8 */
        (*this->out_event_written)(this, result);                                                       /* COVERAGE EXCLUSION: Debian 8 */
    }                                                                                                   /* COVERAGE EXCLUSION: Debian 8 */
                                                                                                        /* COVERAGE EXCLUSION: Debian 8 */
    SXER6I("return");                                                                                   /* COVERAGE EXCLUSION: Debian 8 */
}                                                                                                       /* COVERAGE EXCLUSION: Debian 8 */

SXE_RETURN
sxe_send_buffers(SXE * this, SXE_LIST * buffers, SXE_OUT_EVENT_WRITTEN on_complete)
{
    SXE_RETURN result = SXE_RETURN_OK;

    SXEE6I("sxe_send_buffers(buffers=%p,on_complete=%p) // socket=%d", buffers, on_complete, this->socket);
    SXEA1I(buffers != NULL,     "sxe_send_buffers: buffers pointer can't be NULL");
    SXEA1I(on_complete != NULL, "sxe_send_buffers: on_complete callback function pointer can't be NULL");

    SXEL6I("sxe_send_buffers(): preparing to send %u buffers", SXE_LIST_GET_LENGTH(buffers));

    sxe_list_walker_construct(&this->send_list_walk, buffers);
    sxe_list_walker_step(&this->send_list_walk); /* advance to the first entry */
    this->out_event_written = on_complete;

#ifndef SXE_DISABLE_OPENSSL
    if (this->ssl_id != SXE_POOL_NO_INDEX) {
        result = sxe_ssl_send_buffers(this);
        goto SXE_EARLY_OUT;
    }
#endif

    result = sxe_send_buffers_again(this);

#ifndef SXE_DISABLE_OPENSSL
SXE_EARLY_OUT:
#endif
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

/* TODO: new sxe_send() parameter to auto close after writing all data */

SXE_RETURN
sxe_send(SXE * this, const void * data, unsigned size, SXE_OUT_EVENT_WRITTEN on_complete)
{
    SXE_RETURN result;

    SXEE6I("sxe_send(data=%p, size=%u, on_complete=%p)", data, size, on_complete);
    SXEA1I(size                                    != 0,    ": I'm not as stupid as you clearly are, trying to send 0 bytes");
    SXEA1I(on_complete                             != NULL, ": on_complete callback function pointer can't be NULL");
    SXEA1I(sxe_buffer_get_data(&this->send_buffer) != NULL, ": there is already a sxe_send in progress");

    sxe_buffer_list_construct(&this->send_list);
    sxe_buffer_construct_const(&this->send_buffer, data, size);
    sxe_list_push(&this->send_list, &this->send_buffer);
    result = sxe_send_buffers(this, &this->send_list, on_complete);
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

static void
sxe_io_cb_notify_writable(EV_P_ ev_io * io, int revents)
{
    SXE_OUT_EVENT_WRITTEN cb;
    SXE * this = (SXE *)io->data;
    SXE_UNUSED_PARAMETER(revents);
#if EV_MULTIPLICITY
    SXE_UNUSED_PARAMETER(loop);
#endif
    SXEE6I("sxe_io_cb_notify_writable(this=%p, revents=%u) // socket=%d", this, revents, this->socket);
    SXEL6I("Re-enabling read events on this SXE");
    sxe_watch_read(this);

    if (this->out_event_written) {
        cb = this->out_event_written;
        this->out_event_written = NULL;
        (*cb)(this, SXE_RETURN_OK);
    }
    SXER6I("return");
}

void
sxe_notify_writable(SXE * this, SXE_OUT_EVENT_WRITTEN writable_cb)
{
    SXEE6I("sxe_notify_writable(on_complete=%p)", writable_cb);
    this->out_event_written = writable_cb;
    sxe_private_set_watch_events(this, sxe_io_cb_notify_writable, EV_WRITE, 1);
    SXER6I("return");
}

#ifndef WINDOWS_NT
static void
sxe_io_cb_sendfile(EV_P_ ev_io * io, int revents)
{
    SXE * this = (SXE *)io->data;
    SXE_UNUSED_PARAMETER(loop);
    SXE_UNUSED_PARAMETER(revents);

    SXEE6I("sxe_io_cb_sendfile(this=%p, revents=%u) // socket=%d", this, revents, this->socket);
    sxe_sendfile(this, this->sendfile_in_fd, this->sendfile_offset, this->sendfile_bytes, this->out_event_written);
    SXER6("return");
}

SXE_RETURN
sxe_sendfile(SXE * this, int in_fd, off_t * offset, unsigned total_bytes, SXE_OUT_EVENT_WRITTEN on_complete)
{
    SXE_RETURN result = SXE_RETURN_ERROR_WRITE_FAILED;
    ssize_t    sent;

    SXEE6I("sxe_sendfile(in_fd=%d, offset=%lu, total_bytes=%u, on_complete=%p)", in_fd, (unsigned long)*offset, total_bytes, on_complete);

    SXEA6I(this->ssl_id == SXE_POOL_NO_INDEX, "sxe_sendfile(): SSL socket: use sxe_send()");
    SXEA1I(on_complete != NULL,               "sxe_sendfile(): on_complete callback function pointer can't be NULL");
    SXEA1I(offset      != NULL,               "sxe_sendfile(): offset pointer can't be NULL");
    SXEA1I(total_bytes != 0,                  "sxe_sendfile(): total_bytes can't be zero"); /* Apple and FreeBSD: send '0' means send until EOF */

    this->sendfile_in_fd    = in_fd;
    this->sendfile_offset   = offset;
    this->out_event_written = on_complete;

#if defined(__APPLE__) || defined(__FreeBSD__)
    {
        off_t len = (off_t)total_bytes;
        int   res;

#ifdef __APPLE__
        SXEL6I("apple:sendfile(in=%u, s=%u, off=%lu, total_bytes=%u, ...)", in_fd, this->socket, (unsigned long)*offset, total_bytes);
        res = sendfile(in_fd, this->socket, *offset, &len, /* No preamble or postamble data */NULL, /* reserved */0);
#else /* FreeBSD */
        SXEL6I("bsd:sendfile(in=%u, s=%u, off=%u, total_bytes=%u, ...)", in_fd, this->socket, (unsigned)*offset, total_bytes);
        res = sendfile(in_fd, this->socket, *offset, total_bytes, /* No preamble or postamble data*/NULL, &len, /* no flags */0);
#endif

        if (res < 0) {
            if (errno == EAGAIN) {
                *offset += len;
                sent = (ssize_t)len;
                goto SXE_PARTIAL_WRITE;
            }
            else
                sent = -1; /* error; see below */
        }
        else {
            SXEL6I("apple:sendfile() sent={%lu}", (unsigned long)len);
            *offset += len;
            sent = (ssize_t)len;
        }
    }
#else
    sent = sendfile(this->socket, in_fd, offset, total_bytes);
#endif

    if (sent < 0) {
        SXEL2I("sendfile() failed: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());

        if (this->out_event_written) {
            (*this->out_event_written)(this, result);
        }

        goto SXE_CLEANUP_OUT;
    }

    if ((sent > 0) && ((unsigned)sent < total_bytes)) {
#if defined(__APPLE__) || defined(__FreeBSD__)
SXE_PARTIAL_WRITE:
#endif
        SXEL6I("sendfile() sent %zd bytes of %u, filling send buffer", sent, total_bytes);
        result = SXE_RETURN_IN_PROGRESS;
        this->sendfile_bytes = total_bytes - sent;

        sxe_private_set_watch_events(this, sxe_io_cb_sendfile, EV_WRITE, 1);
        goto SXE_EARLY_OUT;
    }

    if (sent == 0) {
        SXEL6I("sendfile() sent 0 bytes (EOF)");
        result = SXE_RETURN_END_OF_FILE;
    }
    else {
        SXEL6I("sendfile() sent %zu bytes of %u", sent, total_bytes);
        result = SXE_RETURN_OK;
    }

    if (this->out_event_written) {
        (*this->out_event_written)(this, result);
    }

    sxe_watch_read(this);

SXE_CLEANUP_OUT:
    this->sendfile_in_fd    = -1;
    this->sendfile_bytes    = 0;
    this->out_event_written = NULL;

SXE_EARLY_OUT:
    SXER6I("return %d // %s", result, sxe_return_to_string(result));
    return result;
}
#endif

void
sxe_watch_read(SXE * this)
{
    SXEE6I("sxe_watch_read()");
    sxe_private_set_watch_events(this, sxe_io_cb_read, EV_READ, 1);
    SXER6I("return");
}

void
sxe_watch_write(SXE * this)                                                     /* COVERAGE EXCLUSION: Debian 8 */
{                                                                               /* COVERAGE EXCLUSION: Debian 8 */
    SXEE6I("sxe_watch_write()");                                                /* COVERAGE EXCLUSION: Debian 8 */
    sxe_private_set_watch_events(this, sxe_io_cb_send_buffers, EV_WRITE, 1);    /* COVERAGE EXCLUSION: Debian 8 */
    SXER6I("return");                                                           /* COVERAGE EXCLUSION: Debian 8 */
}                                                                               /* COVERAGE EXCLUSION: Debian 8 */

/* TODO: Make useful or delete
 */
void
sxe_dump(SXE * this)
{
    unsigned i;

    SXE_UNUSED_PARAMETER(this);

    SXEE6I("sxe_dump()");
    SXEL6I("Dumping sxes");

    for (i = 0; i < sxe_array_total; i++) {
        SXEL6I("SXE id=%u", i);
    }

    SXER6I("return");
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
    SXE_STATE  state  = sxe_pool_index_to_state(sxe_array, this->id);
    const char *state_str;

    SXEE6I("sxe_close(this=%p)", this);
    SXEA6I(sxe_array != NULL,                                                "sxe_close: SXE is not initialized");
    SXEA6I((this >= sxe_array) && (this <= &sxe_array[sxe_array_total - 1]), "sxe_close: connection pointer is not valid");

    /* Close is idempotent with a warning.
     */
    switch (state) {
    case SXE_STATE_FREE:
        SXEL3I("sxe_close: ignoring close of a free connection object %u", this->id);
        result = SXE_RETURN_WARN_ALREADY_CLOSED;
        goto SXE_EARLY_OUT;

    case SXE_STATE_DEFERRED:
    case SXE_STATE_USED:
        break;

    default:                                                                       /* coverage exclusion: can't get here */
        state_str = sxe_state_to_string(state);                                    /* coverage exclusion: can't get here */
        SXEA1I(0, "Internal error: unexpected SXE state %s", state_str ?: "NULL"); /* coverage exclusion: can't get here */
    }

#ifndef SXE_DISABLE_OPENSSL
    /* If we have an open SSL socket, close that first. */
    if (this->ssl_id != SXE_POOL_NO_INDEX) {
        result = sxe_ssl_close(this);
        goto SXE_EARLY_OUT;
    }
#endif

    /* Close socket if open.
     */
    if (this->socket != SXE_SOCKET_INVALID) {
        ev_io_stop(sxe_private_main_loop, &this->io);
        ev_async_stop(sxe_private_main_loop, &this->async);

        /* Call CLOSESOCKET() as well as close() for Windows Sockets API compatibility.
         */
        SXEL6I("About to close fd %d (this=%p)", this->socket_as_fd, this);
        if (this->socket_as_fd >= 0) {
            close(this->socket_as_fd);
            this->socket_as_fd = -1;
        }
        CLOSESOCKET(this->socket);

        this->socket = SXE_SOCKET_INVALID;
    }

    this->in_event_connected = NULL;
    this->in_event_read      = NULL;
    this->out_event_written  = NULL;
    this->in_total    = 0;
    this->in_consumed = 0;
    sxe_pool_set_indexed_element_state(sxe_array, this->id, state, SXE_STATE_FREE);

SXE_EARLY_OUT:
    SXER6I("return %s", sxe_return_to_string(result));
    return result;
}

#ifndef _WIN32
static void
sxe_socketpair_connected_cb(EV_P_ struct ev_async *w, int revents)
{
    SXE               * this = (SXE *)w->data;

    SXE_UNUSED_PARAMETER(revents);

    SXEE6I("sxe_socketpair_connected_cb(w=%p,revents=%d)", w, revents);

    ev_async_stop(EV_A_ &this->async);

    if (this->in_event_connected) {
        (*this->in_event_connected)(this);
    }

    ev_io_start(EV_A_ &this->io);

    SXER6I("return");
}

SXE *
sxe_new_socketpair(SXE                    * this              ,
                   int                    * out_pairedsocket  ,
                   SXE_IN_EVENT_CONNECTED   in_event_connected,
                   SXE_IN_EVENT_READ        in_event_read     ,
                   SXE_IN_EVENT_CLOSE       in_event_close    )
{
    SXE                * keeper = NULL;
    struct sockaddr_in   local_addr;
    int                  pair[2];

    SXEE6I("sxe_new_socketpair(in_event_read=%p,in_event_close=%p)",
            in_event_read, in_event_close);

    SXEA1(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) >= 0, "socketpair() failed: (%d) %s", errno, strerror(errno));

    memset(&local_addr, 0, sizeof local_addr);
    keeper = sxe_new_internal(this, &local_addr, in_event_connected, in_event_read, in_event_close, SXE_TRUE, NULL);

    if (keeper) {
        int flags;

        (void)flags;

        *out_pairedsocket = pair[1];

        keeper->socket = pair[0];
        keeper->socket_as_fd = _open_osfhandle(pair[0], 0);

        sxe_socket_set_nonblock(pair[0], 1);
#ifdef __APPLE__
        /* Prevent firing SIGPIPE */
        flags = 1;
        SXEV6I(setsockopt(pair[0], SOL_SOCKET, SO_NOSIGPIPE, SXE_WINAPI_CAST_CHAR_STAR &flags, sizeof(flags)),
                >= 0, "socket=%d: couldn't set SO_NOSIGPIPE flag: (%d) %s", pair[0], sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
#endif

        /* Arrange to call sxe_socketpair_connected_cb() at the next loop. */
        ev_async_init(&keeper->async, sxe_socketpair_connected_cb);
        keeper->async.data = keeper;
        ev_io_init(&keeper->io, sxe_io_cb_read, keeper->socket_as_fd, EV_READ);
        keeper->io.data = keeper;

        ev_async_start(sxe_private_main_loop, &keeper->async);
        ev_async_send(sxe_private_main_loop, &keeper->async);
    }
    else {                      /* COVERAGE EXCLUSION */
        close(pair[0]);         /* COVERAGE EXCLUSION */
        close(pair[1]);         /* COVERAGE EXCLUSION */
    }                           /* COVERAGE EXCLUSION */

    SXER6I("return keeper=%p // out_pairedsocket=%d", keeper, pair[1]);
    return keeper;
}
#endif

/* vim: set expandtab: */
