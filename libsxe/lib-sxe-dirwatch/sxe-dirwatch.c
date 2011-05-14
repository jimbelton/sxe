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

#ifdef WIN32
/* TODO: Implement sxe_dirwatch() on Windows */
#elif defined(__APPLE__)
/* TODO: Implement sxe_dirwatch() on Apple */
#elif defined(__FreeBSD__)
/* TODO: Implement sxe_dirwatch() on FreeBSD */
#else

#include <sys/inotify.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "ev.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-list.h"
#include "sxe-util.h"
#include "sxe-dirwatch.h"

extern struct ev_loop * sxe_private_main_loop;

static ev_io            sxe_dirwatch_watcher;
static int              sxe_dirwatch_inotify_fd = -1;
static SXE_LIST         sxe_dirwatch_list;

static void
sxe_dirwatch_event(EV_P_ ev_io * io, int revents)
{
    char                   buffer[8192];
    int                    length;
    unsigned               offset;
    struct inotify_event * event;

    SXE_UNUSED_PARAMETER(revents);
    SXEE62("(fd=%d, revents=%08x)", io->fd, revents);
    SXEA11((length = read(io->fd, buffer, sizeof(buffer))) >= 0, "sxe_dirwatch_event: Failed to read events from inotify: %s",
            strerror(errno));

    for (offset = 0; offset < (unsigned)length;)
    {
        SXE_LIST_WALKER walker;
        SXE_DIRWATCH  * dirwatch;
        int             flags = 0;

        SXEA12(length - offset >= sizeof(struct inotify_event),
               "Odd sized chunk left in buffer %u (expected inotify event of %u bytes)",
               length - offset, sizeof(struct inotify_event));
        event = (struct inotify_event *)&buffer[offset];
        offset += sizeof(struct inotify_event);
        SXEA12(length - offset >= event->len, "Chunk left in buffer %u (expected length %u)", length - offset, event->len);
        offset += event->len;
        SXEL63("dirwatch_event: fd=%d flags=%08x file=%s", event->wd, event->mask, event->name);
        sxe_list_walker_construct(&walker, &sxe_dirwatch_list);

        while ((dirwatch = (SXE_DIRWATCH *)sxe_list_walker_step(&walker)) != NULL) {
            if (dirwatch->fd == event->wd) {
                break;
            }
        }

        SXEA11(dirwatch, "No watched directory found with fd %d", event->wd);

        flags |= (event->mask & IN_CREATE)     ? SXE_DIRWATCH_CREATED  : 0;
        flags |= (event->mask & IN_MOVED_TO)   ? SXE_DIRWATCH_CREATED  : 0;
        flags |= (event->mask & IN_MODIFY)     ? SXE_DIRWATCH_MODIFIED : 0;
        flags |= (event->mask & IN_DELETE)     ? SXE_DIRWATCH_DELETED  : 0;
        flags |= (event->mask & IN_MOVED_FROM) ? SXE_DIRWATCH_DELETED  : 0;

        dirwatch->notify(EV_A_ event->name, flags, dirwatch->user_data);
    }

    SXER60("return");
}

/**
 * Initialize the dirwatch package
 *
 * @exceptions Aborts if already initialized or on failure to initialize inotify
 */
void
sxe_dirwatch_init(void)
{
    SXEE60("()");
    if (sxe_dirwatch_inotify_fd != -1) {
        SXEL60("sxe_dirwatch_init: Already initialized");
        goto SXE_EARLY_OUT;
    }

    SXEA11((sxe_dirwatch_inotify_fd = inotify_init()) != -1, "sxe_dirwatch_init: inotify_init() failed with %s", strerror(errno));
    SXEL61("inotify_init() returned fd: %d", sxe_dirwatch_inotify_fd);
    SXE_LIST_CONSTRUCT(&sxe_dirwatch_list, 0, SXE_DIRWATCH, node);
    ev_io_init(&sxe_dirwatch_watcher, sxe_dirwatch_event, sxe_dirwatch_inotify_fd, EV_READ);

SXE_EARLY_OUT:
    SXER60("return");
}

/**
 * Construct a watched directory and add it to the list of watched directories
 *
 * @param dirwatch  Pointer to the directory watcher
 * @param directory       Name of directory to watch
 * @param flags     Add one or more of: SXE_DIRWATCH_CREATED, SXE_DIRWATCH_MODIFIED, SXE_DIRWATCH_DELETED
 * @param notify    Function to be called when directory changes
 * @param user_data Data to pass to the notify callback
 *
 * @exception Aborts if the dirwatch cannot be added to inotify
 */
void
sxe_dirwatch_add(SXE_DIRWATCH * dirwatch, const char * directory, unsigned flags,
                 void(*notify)(EV_P_ const char * file, int revents,void * user_data), void * user_data)
{
    char   buffer[PATH_MAX];
    char * cwd;
    int    inotify_flags = IN_MASK_ADD | IN_ONLYDIR;

    SXEE65("(dirwatch=%p, directory=%s, flags=0x%x, notify=%p, user_data=%p)", dirwatch, directory, flags, notify, user_data);

    if (flags & SXE_DIRWATCH_CREATED) {
        inotify_flags |= IN_CREATE | IN_MOVED_TO;
    }

    if (flags & SXE_DIRWATCH_MODIFIED) {
        inotify_flags |= IN_MODIFY;
    }

    if (flags & SXE_DIRWATCH_DELETED) {
        inotify_flags |= IN_DELETE | IN_MOVED_FROM;
    }

    dirwatch->notify    = notify;
    dirwatch->user_data = user_data;
    dirwatch->fd        = inotify_add_watch(sxe_dirwatch_watcher.fd, directory, inotify_flags);
    SXEA13(dirwatch->fd != -1, "Error watching directory '%s' (pwd = '%s'): %s", directory,
           (cwd = getcwd(buffer, sizeof(buffer))), strerror(errno));
    sxe_list_push(&sxe_dirwatch_list, dirwatch);
    SXEL62("added directory %s as watch id %d", directory, dirwatch->fd);
    SXER60("return");
}

/**
 * Start watching the list of watched directories
 */
void
sxe_dirwatch_start(void)
{
    SXEE61("sxe_dirwatch_start() // fd=%d", sxe_dirwatch_watcher.fd);
    ev_io_start(sxe_private_main_loop, &sxe_dirwatch_watcher);
    SXER60("return");
}

/**
 * Stop watching the list of watched directories
 */
void
sxe_dirwatch_stop(void)
{
    SXEE60("sxe_stat_stop()");
    ev_io_stop(sxe_private_main_loop, &sxe_dirwatch_watcher);
    SXER60("return");
}

#endif

/* vim: set expandtab list: */
