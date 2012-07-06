/* Copyright (c) 2009 Jim Belton
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

#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "tap.h"
#include "sxe-buffer.h"
#include "sxe-log.h"

#define TEST_BUF_SIZE 16

static void
test_event_on_empty(void * user_data, SXE_BUFFER * buffer)
{
    SXEE6("(user_data=%p,buffer=%p)", user_data, buffer);
    tap_ev_push(__func__, 2, "user_data", user_data, "buffer", buffer);
    SXER6("return");
}

int
main(void)
{
    SXE_BUFFER     buffer;
    char         buf[TEST_BUF_SIZE + 4];
    const char * str;
    time_t       era = 0;
    struct tm    tm;
    tap_ev       ev;

    memcpy(&buf[TEST_BUF_SIZE], "GARD", 4);
    plan_tests(43);

    sxe_buffer_construct(&buffer, buf, 0, TEST_BUF_SIZE);
    is(sxe_buffer_length(&buffer), 0,                                              "sxe_buffer is initially 0 length");
    is(sxe_buffer_get_room(&buffer), TEST_BUF_SIZE,                                "all %u bytes are available", TEST_BUF_SIZE);
    is((unsigned)(str = sxe_buffer_get_str(&buffer))[0], (unsigned)'\0',           "sxe_buffer is '\\0' terminated");
    ok(sxe_buffer_printf(&buffer, "This string is too big") >= TEST_BUF_SIZE,      "Printf string is >= 16 characters");
    is_eq(sxe_buffer_get_str(&buffer), "This string is ",                          "Truncated string is 'This string is '");
    gmtime_r(&era, &tm);
    is(sxe_buffer_ftime(&buffer, "%Y", &tm), 0,                                    "No room for year");
    ok(sxe_buffer_is_overflow(&buffer),                                            "Overflow is recognized");
    sxe_buffer_clear(&buffer);
    ok(!sxe_buffer_is_overflow(&buffer),                                           "Overflow was cleared");
    ok(sxe_buffer_ftime(&buffer, "%Y", &tm),                                       "Year formatted");
    is_eq(sxe_buffer_get_str(&buffer), "1970",                                     "Time began in 1970");
    ok(sxe_buffer_eq(&buffer, SXE_BUFFER_CAST("1970")),                            "buffer eq SXE_BUFFER_CAST(\"1970\")");
    ok(sxe_buffer_eq(&buffer, &buffer),                                            "buffer eq buffer");
    ok(sxe_buffer_ne(SXE_BUFFER_EMPTY, &buffer),                                   "SXE_BUFFER_EMPTY ne buffer");
    ok(sxe_buffer_cmp(SXE_BUFFER_CAST("1234"), SXE_BUFFER_CAST("1235")) < 0,       "'1234' lt '1235'");
    sxe_buffer_cat(&buffer, SXE_BUFFER_CAST("-01-01"));
    is_eq(sxe_buffer_get_str(&buffer), "1970-01-01",                               "Short concatenation works");
    ok(sxe_buffer_eq(&buffer, SXE_BUFFER_CAST("1970-01-01")),                      "Compared buffer contents as a string");
    ok(!sxe_buffer_is_overflow(&buffer),                                           "No overflow on short concatenation");
    is_eq(sxe_buffer_get_str(sxe_buffer_cat(&buffer, &buffer)), "1970-01-011970-", "Long self concatenation works");
    ok(sxe_buffer_is_overflow(&buffer),                                            "Overflow on long self concatenation");
    is(sxe_buffer_length(SXE_BUFFER_CAST("hello")), 5,                             "length of literal CSTR 'hello' is 5");

    tap_set_test_case_name("sxe_buffer_cspn");
    sxe_buffer_clear(&buffer);
    is(sxe_buffer_printf(&buffer, "Short string"), 12,                 "Printf string is 12 characters");
    is(sxe_buffer_cspn(SXE_BUFFER_CAST("Literal string"), 0, " "), 7,  "'Literal string' has a blank after 7");
    is(sxe_buffer_cspn(SXE_BUFFER_CAST("Literal string"), 0, ","), 14, "'Literal string' - no ',' so result is the string length");
    is(sxe_buffer_cspn(SXE_BUFFER_CAST("Literal string"), 0, ", "), 7, "'Literal string' - no ',' so result is the string length");
    is(sxe_buffer_cspn(SXE_BUFFER_CAST("Literal string"), 0, "as"), 5, "'Literal string' - searching for 'a' and 's'");
    is(sxe_buffer_cspn(SXE_BUFFER_CAST("Literal string"), 6, "as"), 2, "'Literal string' - searching for 'a' and 's' again");
    is(sxe_buffer_cspn(SXE_BUFFER_CAST("Literal string"), 9, "as"), 5, "'Literal string' - searching for 'a' and 's' and again");

    /* Make a garbage terminated buffer containing 'Unlital string'
     */
    memcpy(buf, "Unltral stringX", 16);
    sxe_buffer_construct_const(&buffer, buf, 14);
    is(sxe_buffer_cspn(&buffer, 0, " "), 7,                            "'Unltral string' has a blank after 7");
    is(sxe_buffer_cspn(&buffer, 0, ","), 14,                           "'Unltral string' - no ',' so result is the string length");
    is(sxe_buffer_cspn(&buffer, 0, ", "), 7,                           "'Unltral string' - no ',' so result is the string length");
    is(sxe_buffer_cspn(&buffer, 0, "as"), 5,                           "'Unltral string' - searching for 'a' and 's'");
    is(sxe_buffer_cspn(&buffer, 6, "as"), 2,                           "'Unltral string' - searching for 'a' and 's' again");
    is(sxe_buffer_cspn(&buffer, 9, "as"), 5,                           "'Unltral string' - searching for 'a' and 's' and again");

    tap_set_test_case_name("sxe_buffer_consume");
    sxe_buffer_construct_plus(&buffer, buf, 14, sizeof(buf), (void *)0xDEADBEEF, test_event_on_empty);
    sxe_buffer_consume(&buffer, 8);
    is_eq(sxe_buffer_get_data(&buffer), "stringX",                       "Consumed expected part of buffer");
    is_eq(sxe_buffer_get_data(SXE_BUFFER_CAST("boo")), "boo",            "Able to access the data in a literal buffer");
    is(tap_ev_length(), 0,                                               "No on_empty events in queue");
    sxe_buffer_consume(&buffer, 6);
    is_eq(tap_ev_identifier(ev = tap_ev_shift()), "test_event_on_empty", "Got on_empty event after consuming entire buffer");
    is(tap_ev_arg(ev, "user_data"), 0xDEADBEEF,                          "User data is 0xDEADBEEF");
    is(tap_ev_arg(ev, "buffer"),    &buffer,                             "Buffer is the expected buffer");

    tap_set_test_case_name("sxe_buffer_clear empty callback");
    sxe_buffer_clear(&buffer);
    is(tap_ev_length(), 0,                                               "Clear of already consumed buffer generates no event");
    sxe_buffer_cat(&buffer, SXE_BUFFER_CAST("more"));
    sxe_buffer_clear(&buffer);
    is_eq(tap_ev_identifier(ev = tap_ev_shift()), "test_event_on_empty", "Got on_empty event after clearing non-empty buffer");
    is(tap_ev_arg(ev, "user_data"), 0xDEADBEEF,                          "User data is 0xDEADBEEF");
    is(tap_ev_arg(ev, "buffer"),    &buffer,                             "Buffer is the expected buffer");
    return exit_status();
}
