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

#ifndef __SXE_TEST_H__
#define __SXE_TEST_H__

#include <unistd.h>

#include "ev.h"
#include "tap.h"

#include "lib-sxe-test-proto.h"

/*
 * The following functions are actually defined in the lib-sxe/ library, but
 * are declared here for simplicity of testing.
 */

tap_ev test_tap_ev_queue_shift_wait_or_what(tap_ev_queue queue, ev_tstamp seconds, int at_timeout);
tap_ev test_tap_ev_queue_shift_wait_or_timeout(tap_ev_queue queue, ev_tstamp seconds);
tap_ev test_tap_ev_queue_shift_wait_or_assert(tap_ev_queue queue, ev_tstamp seconds);
tap_ev test_tap_ev_queue_shift_wait(tap_ev_queue queue, ev_tstamp seconds);
tap_ev test_tap_ev_shift_wait(ev_tstamp seconds);
tap_ev test_tap_ev_shift_wait_or_timeout(ev_tstamp seconds);

const char * test_tap_ev_identifier_wait(ev_tstamp seconds, tap_ev * ev_ptr);
const char * test_tap_ev_queue_identifier_wait(tap_ev_queue queue, ev_tstamp seconds, tap_ev * ev_ptr);

/**
 * Wait for (possibly fragmented) data on a SXE on a specific TAP queue
 *
 * @param queue           TAP event queue
 * @param seconds         Seconds to wait for
 * @param ev_ptr          Pointer to a tap_ev event
 * @param this            Pointer to a SXE to check as the source of the read events or NULL to allow reads from multiple SXEs
 * @param read_event_name Expected read event identifier (usually "read_event"); event must have "this", "buf" and "used" args
 * @param buffer          Pointer to a buffer to store the data
 * @param expected_length Expect amount of data to be read
 * @param who             Textual description of SXE for diagnostics
 */
void
test_ev_queue_wait_read(tap_ev_queue queue, ev_tstamp seconds, tap_ev * ev_ptr, void * this, const char * read_event_name,
                        char * buffer, unsigned expected_length, const char * who);

/**
 * Wait for (possibly fragmented) data on a SXE on the default TAP queue
 *
 * @param seconds         Seconds to wait for
 * @param ev_ptr          Pointer to a tap_ev event
 * @param this            Pointer to a SXE to check as the source of the read events or NULL to allow reads from multiple SXEs
 * @param read_event_name Expected read event identifier (usually "read_event"); event must have "this", "buf" and "used" args
 * @param buffer          Pointer to a buffer to store the data
 * @param expected_length Expect amount of data to be read
 * @param who             Textual description of SXE for diagnostics
 */
void
test_ev_wait_read(ev_tstamp seconds, tap_ev * ev_ptr, void * this, const char * read_event_name, char * buffer,
                  unsigned expected_length, const char * who);

int test_ev_consume_events(const char * expected, int expected_events);

#endif
