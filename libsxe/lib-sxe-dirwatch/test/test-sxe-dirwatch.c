#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "sxe.h"
#include "sxe-dirwatch.h"
#include "sxe-log.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_WAIT 5

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__FreeBSD__)
#define TEST_DIRWATCH 1
#else
#define TEST_DIRWATCH 0
#endif

#if TEST_DIRWATCH
static void
test_dirwatch_event(EV_P_ const char *chfile, int chflags, void * user_data)
{
    SXE_UNUSED_PARAMETER(loop);
    SXEE63("test_dirwatch_event(chfile=%s, chflags=%08x, user_data=%p)", chfile, chflags, user_data);
    tap_ev_push(__func__, 3,
                "chfile", tap_dup(chfile, strlen(chfile)),
                "chflags", chflags,
                "user_data", user_data);
    SXER60("return");
}
#endif

int
main(void)
{

#if !TEST_DIRWATCH
/* TODO: Implement sxe_dirwatch() on Windows */
/* TODO: Implement sxe_dirwatch() on Apple */
/* TODO: Implement sxe_dirwatch() on FreeBSD */
#else

    char tempdir1[] = "tmp-XXXXXX";
    char tempdir2[] = "tmp-XXXXXX";
    char tempdir3[] = "tmp-XXXXXX";
    SXE_DIRWATCH dirwatch1;
    SXE_DIRWATCH dirwatch2;
    SXE_DIRWATCH dirwatch3;
    char fname[PATH_MAX];
    tap_ev ev;

    plan_tests(28);

    sxe_register(1, 0);
    sxe_init();

    /* We need to make a new temporary directory, because *everything* appears
     * to change willy-nilly during the build. The current directory has a
     * file that always changes. The /tmp directory always seems to have
     * spurious changes in it. */
    SXEA11(mkdtemp(tempdir1), "Failed to create tempdir: %s", strerror(errno));
    SXEA11(mkdtemp(tempdir2), "Failed to create tempdir: %s", strerror(errno));
    SXEA11(mkdtemp(tempdir3), "Failed to create tempdir: %s", strerror(errno));

    sxe_dirwatch_init();
    sxe_dirwatch_init(); /* for coverage */
    sxe_dirwatch_add(&dirwatch1, tempdir1, SXE_DIRWATCH_CREATED|SXE_DIRWATCH_MODIFIED|SXE_DIRWATCH_DELETED, test_dirwatch_event, (void *)1);
    sxe_dirwatch_add(&dirwatch2, tempdir2, SXE_DIRWATCH_MODIFIED, test_dirwatch_event, (void *)2);
    sxe_dirwatch_add(&dirwatch3, tempdir3, SXE_DIRWATCH_DELETED, test_dirwatch_event, (void *)3);
    sxe_dirwatch_start();

    /* tempdir1: create, modify, delete */
    {
        char rname[PATH_MAX];
        int fd;

        snprintf(rname, sizeof rname, "%s/renamed", tempdir1);
        snprintf(fname, sizeof fname, "%s/created", tempdir1);

        fd = open(fname, O_CREAT|O_RDWR, S_IRWXU);
        write(fd, "Hello", 5);
        rename(fname, rname);
        unlink(rname);

        is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_dirwatch_event",  "Got a dirwatch event");
        is_eq(tap_ev_arg(ev, "chfile"), "created",                                 "Got filename %s", fname);
        is(tap_ev_arg(ev, "chflags"), SXE_DIRWATCH_CREATED,                        "Got flags=SXE_DIRWATCH_CREATED");
        is(tap_ev_arg(ev, "user_data"), 1,                                         "Got user_data=1 (%s)", tempdir1);

        is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_dirwatch_event",  "Got a dirwatch event");
        is_eq(tap_ev_arg(ev, "chfile"), "created",                                 "Got filename %s", fname);
        is(tap_ev_arg(ev, "chflags"), SXE_DIRWATCH_MODIFIED,                       "Got flags=SXE_DIRWATCH_MODIFIED");
        is(tap_ev_arg(ev, "user_data"), 1,                                         "Got user_data=1 (%s)", tempdir1);

        is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_dirwatch_event",  "Got a dirwatch event");
        is_eq(tap_ev_arg(ev, "chfile"), "created",                                 "Got filename %s", fname);
        is(tap_ev_arg(ev, "chflags"), SXE_DIRWATCH_DELETED,                        "Got flags=SXE_DIRWATCH_DELETED");
        is(tap_ev_arg(ev, "user_data"), 1,                                         "Got user_data=1 (%s)", tempdir1);

        is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_dirwatch_event",  "Got a dirwatch event");
        is_eq(tap_ev_arg(ev, "chfile"), "renamed",                                 "Got filename %s", rname);
        is(tap_ev_arg(ev, "chflags"), SXE_DIRWATCH_CREATED,                        "Got flags=SXE_DIRWATCH_CREATED");
        is(tap_ev_arg(ev, "user_data"), 1,                                         "Got user_data=1 (%s)", tempdir1);

        is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_dirwatch_event",  "Got a dirwatch event");
        is_eq(tap_ev_arg(ev, "chfile"), "renamed",                                 "Got filename %s", rname);
        is(tap_ev_arg(ev, "chflags"), SXE_DIRWATCH_DELETED,                        "Got flags=SXE_DIRWATCH_DELETED");
        is(tap_ev_arg(ev, "user_data"), 1,                                         "Got user_data=1 (%s)", tempdir1);
    }

    /* tempdir2: modified */
    {
        const char *rel;
        int fd;

        snprintf(fname, sizeof fname, "%s/file.XXXXXX", tempdir2);
        SXEA11((rel = strchr(fname, '/')), "Didn't find '/' in %s", fname);
        rel++;

        fd = mkstemp(fname);
        write(fd, "Hello", 5);
        unlink(fname);

        is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_dirwatch_event",  "Got a dirwatch event");
        is_eq(tap_ev_arg(ev, "chfile"), rel,                                       "Got filename %s", fname);
        is(tap_ev_arg(ev, "chflags"), SXE_DIRWATCH_MODIFIED,                       "Got flags=SXE_DIRWATCH_MODIFIED");
        is(tap_ev_arg(ev, "user_data"), 2,                                         "Got user_data=2 (%s)", tempdir2);
    }

    /* tempdir3: deleted */
    {
        const char *rel;
        int fd;

        snprintf(fname, sizeof fname, "%s/file.XXXXXX", tempdir3);
        SXEA11((rel = strchr(fname, '/')), "Didn't find '/' in %s", fname);
        rel++;

        fd = mkstemp(fname);
        unlink(fname);

        is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_dirwatch_event",  "Got a dirwatch event");
        is_eq(tap_ev_arg(ev, "chfile"), rel,                                       "Got filename %s", fname);
        is(tap_ev_arg(ev, "chflags"), SXE_DIRWATCH_DELETED,                        "Got flags=SXE_DIRWATCH_DELETED");
        is(tap_ev_arg(ev, "user_data"), 3,                                         "Got user_data=3 (%s)", tempdir3);
    }

    rmdir(tempdir1);
    rmdir(tempdir2);
    rmdir(tempdir3);

    sxe_dirwatch_stop();

#endif
    return exit_status();
}

/* vim: set expandtab list: */
