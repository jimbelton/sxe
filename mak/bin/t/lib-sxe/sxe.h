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

#include <stdbool.h>

#include "ev.h"
#include "sxe-socket.h"
#include "sxe-util.h"

#define SXE_BUF_SIZE    1500
#define SXE_IP_ADDR_ANY "INADDR_ANY"

/* Flags. Currently, only SXE_FLAG_IS_ONESHOT is required in the SXE interface
 */
#define SXE_FLAG_IS_STREAM        0x00000001
#define SXE_FLAG_IS_ONESHOT       0x00000002
#define SXE_FLAG_IS_CALLER_READS  0x00000004
#define SXE_FLAG_IS_PAUSED        0x00000008

typedef enum SXE_BUF_RESUME {
    SXE_BUF_RESUME_IMMEDIATE,
    SXE_BUF_RESUME_WHEN_MORE_DATA
} SXE_BUF_RESUME;

struct SXE; /* Forward Declaration */
typedef void (*SXE_IN_EVENT_READ )(    struct SXE *, int length);
typedef void (*SXE_IN_EVENT_CLOSE)(    struct SXE *            );
typedef void (*SXE_IN_EVENT_CONNECTED)(struct SXE *            );
typedef void (*SXE_OUT_EVENT_WRITTEN )(struct SXE *, SXE_RETURN);

/* SXE object. Used for "Accept Sockets", "Connection Sockets", and UDP ports.
 */
typedef struct SXE {
    struct ev_io           io;
    struct sockaddr_in     local_addr;           /* if ip:port     socket                                                 */
    struct sockaddr_in     peer_addr;            /* if ip:port     socket                                                 */
    const char           * path;                 /* if unix domain socket                                                 */
    unsigned               id_next;              /* to keep free list                                                     */
    unsigned               id;
    unsigned               flags;
    SXE_SOCKET             socket;               /* is handle on Windows, is fd on Linux                                  */
    int                    socket_as_fd;         /* is fd     on Windows, is fd on Linux                                  */
    int                    last_write;           /* number of bytes writen by the last sxe_write() call                   */
    unsigned               in_total;
    unsigned               in_consumed;          /* number of bytes already "consumed" by the callback                    */
    char                   in_buf[SXE_BUF_SIZE];
    SXE_IN_EVENT_CONNECTED in_event_connected;   /* NULL or function to call when peer accepts connection                 */
    SXE_IN_EVENT_READ      in_event_read ;       /*         function to call when peer writes data                        */
    SXE_IN_EVENT_CLOSE     in_event_close;       /* NULL or function to call when peer disconnects                        */
    SXE_OUT_EVENT_WRITTEN  out_event_written;    /*         function to call when long write completes                    */
    int                    sendfile_in_fd;
    unsigned               sendfile_bytes;
    const char           * send_buf;
    unsigned               send_buf_len;
    unsigned               send_buf_written;
    union {
       void            *   as_ptr;
       int                 as_int;
    }                      user_data;            /* Not used by sxe                                                       */
    int                    next_socket;          /* Socket to switch to from pipe when all data has been read and cleared */
} SXE;

#define SXE_BUF_STRNSTR(this,str)        sxe_strnstr    (SXE_BUF(this), str, SXE_BUF_USED(this))
#define SXE_BUF_STRNCASESTR(this,str)    sxe_strncasestr(SXE_BUF(this), str, SXE_BUF_USED(this))
#define SXE_BUF(this)                    (     &(this)->in_buf[0] + (this)->in_consumed)
#define SXE_BUF_USED(this)               (      (this)->in_total - (this)->in_consumed)
#define SXE_PEER_ADDR(this)              (     &(this)->peer_addr)
#define SXE_PEER_PORT(this)              (ntohs((this)->peer_addr.sin_port))
#define SXE_LOCAL_PORT(this)             (ntohs(sxe_get_local_addr(this)->sin_port))
#define SXE_USER_DATA(this)              (      (this)->user_data.as_ptr)
#define SXE_USER_DATA_AS_INT(this)       (      (this)->user_data.as_int)
#define SXE_LAST_WRITE_LENGTH(this)      (      (this)->last_write)
#define SXE_EVENT_READ(this)             (      (this)->in_event_read)
#define SXE_EVENT_CLOSE(this)            (      (this)->in_event_close)
#define SXE_ID(this)                     (      (this)->id)
#define SXE_WRITE_LITERAL(this, literal) sxe_write(this, literal, SXE_LITERAL_LENGTH(literal))

#define SXE_BUF_CLEAR(this)              sxe_buf_clear(this)         /* For backward compatibility only - this macro is deprecated */
#define SXE_LOCAL_ADDR(this)             sxe_get_local_addr(this)    /* For backward compatibility only - this macro is deprecated */

#include "lib-sxe-proto.h"

static inline SXE_RETURN sxe_listen(SXE * this)         {return sxe_listen_plus(this, 0);                   }
static inline SXE_RETURN sxe_listen_oneshot(SXE * this) {return sxe_listen_plus(this, SXE_FLAG_IS_ONESHOT); }

#endif /* __SXE_H__ */
