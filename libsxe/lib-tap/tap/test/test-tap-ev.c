#include <string.h>
#include "tap.h"

#define EV_READ 0

typedef struct ev_io ev_io;

static ev_io * io1;
static ev_io * io2;

static void
test_ev_io(ev_io * io, int event)
{
    tap_ev_push("ev_io", 2, "io", io, "event", event);
}

static int  fd    = 1;
static char buf[] = "hello";

int
main(void)
{
    tap_ev ev;

    plan_tests(21);

    is(tap_ev_length(),                          0,          "No events in queue");
    ok((ev = tap_ev_shift())                  == NULL,       "No event to shift");

    tap_ev_push("ev_read", 3, "fd", fd, "buf", tap_dup(buf, sizeof(buf)), "buf_size", sizeof(buf));
    memcpy(buf, "olleh", sizeof(buf));
    is(tap_ev_length(),                          1,          "One event in queue");
    ok((ev = tap_ev_shift())                  != NULL,       "First event shifted");
    is(tap_ev_length(),                          0,          "No events in queue after shift");

    is_eq(tap_ev_identifier(ev),                 "ev_read",  "Identifier is 'ev_read'");
    is(   tap_ev_arg_count(ev),                  3,          "Event has 3 arguments");
    is(   tap_ev_arg(ev, "fd"),                  fd,         "First argument is fd");
    is_eq(tap_ev_arg(ev, "buf"),                 "hello",    "Second argument is 'hello'");
    is(   tap_ev_arg(ev, "buf_size"),            6,          "Third argument is sizeof('hello')");
    tap_ev_free(ev);

    test_ev_io(io1, EV_READ);
    tap_ev_push("ev_read", 3, "fd", fd, "buf", tap_dup(buf, sizeof(buf)), "buf_size", sizeof(buf));
    memcpy(buf, "bye..", sizeof(buf));
    test_ev_io(io2, EV_READ);

    is(tap_ev_length(),                          3,          "Three events in queue");
    is(tap_ev_count("ev_io"),                    2,          "Two 'ev_io' events in queue");
    is(tap_ev_count("ev_read"),                  1,          "One 'ev_read' event in queue");

    ok(  (ev = tap_ev_shift())                != NULL,       "Second event shifted");
    is_eq(tap_ev_identifier(ev),                 "ev_io",    "Identifier is &test_ev_io");
    tap_ev_free(ev);

    ok(  (ev = tap_ev_shift())                != NULL,       "Third event shifted");
    is_eq(tap_ev_identifier(ev),                 "ev_read",  "Identifier is 'ev_read'");
    is(   tap_ev_arg_count(ev),                  3,          "Event has 3 arguments");
    is(   tap_ev_arg(ev, "fd"),                  fd,         "First argument is fd");
    is_eq(tap_ev_arg(ev, "buf"),                 "olleh",    "Second argument is 'olleh'");
    is(   tap_ev_arg(ev, "buf_size"),            6,          "Third argument is sizeof('olleh')");

    return exit_status();
}
