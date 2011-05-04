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

#include "ev.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-list.h"
#include "sxe-util.h"
#include "sxe-dirwatch.h"

extern struct ev_loop * sxe_private_main_loop;

struct wlist_entry {
    int wfd;
    void (*cb)(EV_P_ const char * file, int revents, void * user_data);
    void *user_data;
    SXE_LIST_NODE node;
};

static struct {
    ev_io    watcher;
    SXE_LIST used;
    SXE_LIST free;
    struct wlist_entry *entries;
} wlist;

static void
dirwatch_event(EV_P_ ev_io * io, int revents)
{
    char buf[8192];
    struct inotify_event *ev = (struct inotify_event *)buf;
    int ofs;
    int len = read(io->fd, buf, sizeof(buf));

    SXE_UNUSED_PARAMETER(revents);

    SXEE82("dirwatch_event(fd=%d revents=%08x)", io->fd, revents);

    for (ofs = 0; ofs < len; ofs += sizeof (struct inotify_event) + ev->len)
    {
        SXE_LIST_WALKER walker;
        struct wlist_entry *ent;
        int flags = 0;

        ev = (struct inotify_event *)&buf[ofs];
        SXEL63("dirwatch_event: wfd=%d flags=%08x file=%s", ev->wd, ev->mask, ev->name);

        sxe_list_walker_construct(&walker, &wlist.used);

        while ((ent = (struct wlist_entry *)sxe_list_walker_step(&walker)) != NULL) {
            if (ent->wfd == ev->wd) {
                break;
            }
        }

        SXEA11(ent, "No watched directory found with wfd %d", ev->wd);

        flags |= (ev->mask & IN_CREATE)     ? SXE_DIRWATCH_CREATED  : 0;
        flags |= (ev->mask & IN_MOVED_TO)   ? SXE_DIRWATCH_CREATED  : 0;
        flags |= (ev->mask & IN_MODIFY)     ? SXE_DIRWATCH_MODIFIED : 0;
        flags |= (ev->mask & IN_DELETE)     ? SXE_DIRWATCH_DELETED  : 0;
        flags |= (ev->mask & IN_MOVED_FROM) ? SXE_DIRWATCH_DELETED  : 0;

        ent->cb(EV_A_ ev->name, flags, ent->user_data);
    }

    SXER80("return");
}

void
sxe_dirwatch_init(int maxdirs)
{
    int infy_fd;
    int i;

    SXEE81("sxe_dirwatch_init(maxdirs=%d)", maxdirs);

    SXEA10(wlist.entries == NULL, "wlist.entries is NULL");

    SXEA10((wlist.entries = malloc(sizeof(struct wlist_entry) * maxdirs)) != NULL, "Couldn't allocate memory for the watch list");
    memset(wlist.entries, '\0', sizeof(struct wlist_entry) * maxdirs);

    SXE_LIST_CONSTRUCT(&wlist.free, maxdirs, struct wlist_entry, node);
    SXE_LIST_CONSTRUCT(&wlist.used, maxdirs, struct wlist_entry, node);

    /* Add all entries to free list */
    for (i = 0; i < maxdirs; i++) {
        wlist.entries[i].wfd = -1;
        sxe_list_push(&wlist.free, &wlist.entries[i]);
    }

    SXEA11((infy_fd = inotify_init()) != -1, "inotify_init() failed with %d", errno);
    SXEL91("inotify_init() returned fd:%d", infy_fd);
    ev_io_init(&wlist.watcher, dirwatch_event, infy_fd, EV_READ);

    SXER80("return");
}

void
sxe_dirwatch_add(const char * dir, unsigned flags, void(*cb)(EV_P_ const char * file, int revents, void * user_data), void * user_data)
{
    int dwflags = IN_MASK_ADD | IN_ONLYDIR;
    int wfd;
    struct wlist_entry *ent;

    SXEE83("sxe_dirwatch_add(dir=%s, cb=%p, user_data=%p)", dir, cb, user_data);

    SXEA10(! SXE_LIST_IS_EMPTY(&wlist.free), "No free entries");
    ent = sxe_list_pop(&wlist.free);

    if (flags & SXE_DIRWATCH_CREATED)
        dwflags |= IN_CREATE | IN_MOVED_TO;
    if (flags & SXE_DIRWATCH_MODIFIED)
        dwflags |= IN_MODIFY;
    if (flags & SXE_DIRWATCH_DELETED)
        dwflags |= IN_DELETE | IN_MOVED_FROM;

    SXEA11((wfd = inotify_add_watch(wlist.watcher.fd, dir, dwflags)) != -1, "Error watching directory: %d", errno);

    ent->wfd = wfd;
    ent->cb = cb;
    ent->user_data = user_data;
    sxe_list_push(&wlist.used, ent);
    SXEL64("added directory %s as watch id %d -- now used:%d free:%d", dir, wfd, SXE_LIST_GET_LENGTH(&wlist.used), SXE_LIST_GET_LENGTH(&wlist.free));

    SXER80("return");
}

void
sxe_dirwatch_start(void)
{
    SXEE81("sxe_dirwatch_start() // fd=%d", wlist.watcher.fd);
    ev_io_start(sxe_private_main_loop, &wlist.watcher);
    SXER80("return");
}

void
sxe_dirwatch_stop(void)
{
    SXEE80("sxe_stat_stop()");
    ev_io_stop(sxe_private_main_loop, &wlist.watcher);
    SXER80("return");
}

#endif

/* vim: set expandtab list: */
