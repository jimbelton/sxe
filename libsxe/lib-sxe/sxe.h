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

#ifndef __SXE_H__
#define __SXE_H__

#include "ev.h"
#include "sxe-log.h"        /* For SXE_BOOL */
#include "sxe-socket.h"

#define SXE_BUF_SIZE    1500
#define SXE_IP_ADDR_ANY "INADDR_ANY"

struct SXE;

typedef void (*SXE_IN_EVENT_READ )(    struct SXE *, int length);
typedef void (*SXE_IN_EVENT_CLOSE)(    struct SXE *            );
typedef void (*SXE_IN_EVENT_CONNECTED)(struct SXE *            );

/* SXE object. Used for "Accept Sockets", "Connection Sockets", and UDP ports.
 */
typedef struct SXE {
    struct ev_io           io;
    struct sockaddr_in     local_addr;           /* if ip:port     socket                                                 */
    struct sockaddr_in     peer_addr;            /* if ip:port     socket                                                 */
    const char           * path;                 /* if unix domain socket                                                 */
    unsigned               id_next;              /* to keep free list                                                     */
    unsigned               id;
    SXE_BOOL               is_tcp;
    SXE_BOOL               is_unix;
    SXE_BOOL               is_accept_oneshot;
    SXE_BOOL               is_caller_reads_udp;  /* is caller to read UDP packets? default is no                          */
    int                    socket;               /* is handle on Windows, is fd on Linux                                  */
    int                    socket_as_fd;         /* is fd     on Windows, is fd on Linux                                  */
    unsigned               in_total;
    char                   in_buf[SXE_BUF_SIZE];
    SXE_IN_EVENT_CONNECTED in_event_connected;   /* NULL or function to call when peer accepts connection                 */
    SXE_IN_EVENT_READ      in_event_read ;       /*         function to call when peer writes data                        */
    SXE_IN_EVENT_CLOSE     in_event_close;       /* NULL or function to call when peer disconnects                        */
    union {
       void            *   as_ptr;
       int                 as_int;
    }                      user_data;            /* Not used by sxe                                                       */
    int                    next_socket;          /* Socket to switch to from pipe when all data has been read and cleared */
} SXE;

#define SXE_BUF_STRNSTR(this,str)      sxe_strnstr    (SXE_BUF(this), str, SXE_BUF_USED(this))
#define SXE_BUF_STRNCASESTR(this,str)  sxe_strncasestr(SXE_BUF(this), str, SXE_BUF_USED(this))
#define SXE_BUF_CLEAR(this)            sxe_buf_clear(this)      /* For backward compatibility only - this macro is deprecated */
#define SXE_BUF(this)                  (     &(this)->in_buf[0])
#define SXE_BUF_USED(this)             (      (this)->in_total)
#define SXE_PEER_ADDR(this)            (     &(this)->peer_addr)
#define SXE_PEER_PORT(this)            (ntohs((this)->peer_addr.sin_port))
#define SXE_LOCAL_ADDR(this)           (     &(this)->local_addr)
#define SXE_LOCAL_PORT(this)           (ntohs((this)->local_addr.sin_port))
#define SXE_USER_DATA(this)            (      (this)->user_data.as_ptr)
#define SXE_USER_DATA_AS_INT(this)     (      (this)->user_data.as_int)
#define SXE_EVENT_READ(this)           (      (this)->in_event_read)
#define SXE_EVENT_CLOSE(this)          (      (this)->in_event_close)
#define SXE_ID(this)                   (      (this)->id)

#include "sxe-proto.h"
#include "sxe-timer-proto.h"

#endif /* __SXE_H__ */
