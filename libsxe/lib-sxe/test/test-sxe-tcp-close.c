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

#include <string.h>
#include <stdlib.h>

#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-socket.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

static void
test_event_connected(SXE * this)
{
    SXEE60I("test_event_connected()");
    tap_ev_push(__func__, 1, "this", this);
    SXER60I("return");
}

static void
test_event_read(SXE * this, int length)
{
    SXEE61I("test_event_read(length=%d)", length);
    tap_ev_push(__func__, 2, "this", this, "length", length);
    if(SXE_BUF_STRNCASESTR(this, "CLOSE")) {
        SXEL10("Closing         - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -");
        sxe_close(this);
    }
    SXER60I("return");
}

static void
test_event_close(SXE * this)
{
    SXEE60I("test_event_close()");
    tap_ev_push(__func__, 1, "this", this);
    SXER60I("return");
}

#define CONCURRENCY 7

int
main(int argc, char *argv[])
{
    SXE_RETURN result;
    int        port;
    SXE *      listener;
    SXE *      connectees[CONCURRENCY];
    SXE *      connectors[CONCURRENCY];
    tap_ev     event;
    int        i, j;

    plan_tests(150);
    SXE_UNUSED_PARAMETER(argc);
    SXE_UNUSED_PARAMETER(argv);

    port = 3030;
    sxe_register(CONCURRENCY*2+1, 0);
    ok(sxe_init() == SXE_RETURN_OK, "init succeeded");

    /* connect_test
     */

    listener  = sxe_new_tcp(NULL, "INADDR_ANY", port, test_event_connected, test_event_read, test_event_close);
    is(sxe_listen(listener), SXE_RETURN_OK, "creation of listener succeeded");

    for (i=0; i < CONCURRENCY; i++) {
        connectors[i] = sxe_new_tcp(NULL, "INADDR_ANY", 0,    test_event_connected, test_event_read, test_event_close);
        is(sxe_connect(connectors[i], "127.0.0.1", port),  SXE_RETURN_OK, "connection to listener initiated");
        is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_connected", "got 1st connected event; e.g. server");
        is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_connected", "got 2nd connected event; e.g. client");
    }

    for (i=0; i < CONCURRENCY; i++) {
        SXEL11("Write round #%2d ---------------------------------------------------------------------", i);
        for (j=i; j < CONCURRENCY; j++) {
            char   message_close[] = "CLOSE";
            char   message_hello[] = "HELLO";
            char * message = ( (i == j) ? message_close : message_hello );
            SXEL11("Write text  #%2d - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -", j);
            is(result = sxe_write(connectors[j], message, 5), SXE_RETURN_OK,                    "wrote message to connector");
            is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_read",      "got a read event");
            connectees[j] = SXE_CAST_NOCONST(SXE *, tap_ev_arg(event, "this"));
            is(tap_ev_arg(event, "length")            , 5,                                      "read expected bytes");
            is_strncmp(SXE_BUF(connectees[j]), message, 5,                                      "message in buffer");
            SXE_BUF_CLEAR(connectees[j]);

            if(i == j) {
                is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_close", "got a close event");
                is(tap_ev_arg(event, "this"), connectors[i],                                    "on the connector");
            }
        }

    }

    is(sxe_fini(), SXE_RETURN_OK, "finished with sxe");
    return exit_status();
}
