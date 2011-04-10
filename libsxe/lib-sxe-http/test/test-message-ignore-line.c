/* Copyright 2010 Sophos Limited. All rights reserved. Sophos is a registered
 * trademark of Sophos Limited.
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

#include "sxe-http.h"
#include "sxe-test.h"  // for _snprintf
#include "tap.h"

#define STRIGN_LEN_100 "0123456789" \
                       "0123456789" \
                       "0123456789" \
                       "0123456789" \
                       "0123456789" \
                       "0123456789" \
                       "0123456789" \
                       "0123456789" \
                       "0123456789" \
                       "0123456789"

#define HEADER(x)      "HEADER-" #x
#define LONG_HEADER(x) "LONG-HEADER-" #x "-" STRIGN_LEN_100

#define VALUE(x)      "VALUE_" #x
#define LONG_VALUE(x) "LONG_VALUE_" #x "_" STRIGN_LEN_100

static char       message_headers[8096]; /* big enough for all the tests */
static unsigned   max_buffer_size;

static void test_long_name(void)
{
    SXE_HTTP_MESSAGE message;
    unsigned         i;
    unsigned         line_len;

    tap_test_case_name("Long Header Name");
    max_buffer_size = 30;

    memset(message_headers, 0, sizeof(message_headers));

    snprintf(message_headers, sizeof(message_headers), "\r\n%s: %s\r\n\r\n", LONG_HEADER(1), VALUE(1));
    sxe_http_message_construct(&message, message_headers, max_buffer_size);
    line_len = strlen(message_headers);
    diag("Long Header Name: The header is %u bytes, the max buffer size is %u bytes", line_len, max_buffer_size);

    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_END_OF_FILE,
                                                                                      "Parsed message request line");

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK,     "Get long header");
    is(sxe_http_message_consume_parsed_headers(&message),  2,                         "Consuming the request line");

    /* Simulate sxe_buf_consume() */
    memmove(message_headers, &message_headers[2], sizeof(message_headers) - 2);
    diag("Long Header Name: Consumed %u bytes", 2);
    /* Buffer: "LONG-HEADER-1-0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789: VALUE_1\r\n" */
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK,     "Get long header");
    is(sxe_http_message_consume_parsed_headers(&message),  0,                         "Buffer is full");

    /* Start ignoring current line */
    /* Simulate sxe_buf_clear(), Igore 1st buffer */
    memmove(message_headers, &message_headers[max_buffer_size], sizeof(message_headers) - max_buffer_size);
    diag("Long Header Name: Consumed %u bytes", max_buffer_size);
    /* Buffer: "678901234567890123456789012345678901234567890123456789012345678901234567890123456789: VALUE_1\r\n\r\n" */
    message.ignore_line = 1;
    sxe_http_message_increase_buffer_length(&message, max_buffer_size);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK,     "Get long header");
    is(message.ignore_line, 1, "Ignore buffer flag is set");
    is(message.ignore_length, max_buffer_size, "Ignore length is right");

    for (i = 2; i < 4; i++) { //two more buffers to ignore
        memmove(message_headers, &message_headers[max_buffer_size], sizeof(message_headers) - max_buffer_size);
        diag("Long Header Name: Consumed %u bytes", max_buffer_size);
        sxe_http_message_increase_buffer_length(&message, max_buffer_size);
        is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK,     "Get long header");
        is(message.ignore_line, 1, "Ignore buffer flag is set");
        is(message.ignore_length, max_buffer_size, "Ignore length is right");
    }
    /* Buffer: "678901234567890123456789: VALUE_1\r\n\r\n" */
    memmove(message_headers, &message_headers[max_buffer_size], sizeof(message_headers) - max_buffer_size);
    diag("Long Header Name: Consumed %u bytes", max_buffer_size);
    /* Buffer: "E_1\r\n\r\n" */

    sxe_http_message_increase_buffer_length(&message, 7);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_END_OF_FILE, "Get end of line");
    is(message.ignore_line, 0, "Ignore buffer flag is clear");
    is(message.ignore_length, 7, "Ignore length is right");
    is(sxe_http_message_consume_parsed_headers(&message), 7, "Consumed the rest 7 bytes");
}

static void test_long_value(void)
{
    SXE_HTTP_MESSAGE message;
    unsigned        line_len;

    tap_test_case_name("Long Header Value");
    max_buffer_size = 50;

    memset(message_headers, 0, sizeof(message_headers));

    snprintf(message_headers, sizeof(message_headers), "\r\n%s: %s\r\n\r\n", HEADER(1), LONG_VALUE(1));
    sxe_http_message_construct(&message, message_headers, max_buffer_size);
    line_len = strlen(message_headers);
    diag("Long Header Value: The header is %u bytes, the max buffer size is %u bytes", line_len, max_buffer_size);

    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_END_OF_FILE,
                                                "Parsed message request line");

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(sxe_http_message_consume_parsed_headers(&message),  2,                     "Consuming the request line");
    /* Simulate sxe_buf_consume() */
    memmove(message_headers, &message_headers[2], sizeof(message_headers) - 2);
    diag("Long Header Value: Consumed %u bytes", 2);

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(sxe_http_message_consume_parsed_headers(&message),  0,                     "Buffer is full");
    /* Start ignoring */
    memmove(message_headers, &message_headers[max_buffer_size], sizeof(message_headers) - max_buffer_size);
    diag("Long Header Value: Consumed %u bytes", max_buffer_size);
    message.ignore_line = 1;

    sxe_http_message_increase_buffer_length(&message, max_buffer_size);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(message.ignore_line, 1, "Ignore buffer flag is set");
    is(message.ignore_length, max_buffer_size, "Ignore length is right");
    memmove(message_headers, &message_headers[max_buffer_size], sizeof(message_headers) - max_buffer_size);
    diag("Long Header Value: Consumed %u bytes", max_buffer_size);

    sxe_http_message_increase_buffer_length(&message, line_len - 2 - 2 * max_buffer_size);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_END_OF_FILE, "Get the end of line");
    is(message.ignore_line, 0, "Ignore buffer flag is clear");
    is(message.ignore_length, 27, "Ignore length is right");
    is(sxe_http_message_consume_parsed_headers(&message), 27, "Consumed the rest of 27 bytes");
}

static void test_long_value_with_line_continuations(void)
{
    SXE_HTTP_MESSAGE message;
    unsigned        line_len;

    tap_test_case_name("With Line Continuations");
    max_buffer_size = 60;

    memset(message_headers, 0, sizeof(message_headers));

    snprintf(message_headers, sizeof(message_headers), "\r\n%s: %s\r\n CONTINUE_LINE1\r\n\tCONTINUE_LINE2\r\n\r\n", HEADER(1), LONG_VALUE(1));
    sxe_http_message_construct(&message, message_headers, max_buffer_size);
    line_len = strlen(message_headers);

    diag("With Line Continuations: The header is %u bytes, the max buffer size is %u bytes", line_len, max_buffer_size);

    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_END_OF_FILE,
                                                "Parsed message request line");

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(sxe_http_message_consume_parsed_headers(&message),  2,                     "Consuming the request line");
    /* Simulate sxe_buf_consume() */
    memmove(message_headers, &message_headers[2], sizeof(message_headers) - 2);
    diag("With Line Continuations: Consumed %u bytes", 2);

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(sxe_http_message_consume_parsed_headers(&message),  0,                     "Buffer is full");
    /* Start ignoring */
    memmove(message_headers, &message_headers[max_buffer_size], sizeof(message_headers) - max_buffer_size);
    diag("With Line Continuations: Consumed %u bytes", max_buffer_size);
    message.ignore_line = 1;

    sxe_http_message_increase_buffer_length(&message, max_buffer_size);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(message.ignore_line, 1, "Ignore buffer flag is set");
    is(message.ignore_length, max_buffer_size, "Ignore length is right");
    memmove(message_headers, &message_headers[max_buffer_size], sizeof(message_headers) - max_buffer_size);
    diag("With Line Continuations: Consumed %u bytes", max_buffer_size);

    sxe_http_message_increase_buffer_length(&message, line_len - 2 - 2 * max_buffer_size);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_END_OF_FILE, "Get the end of line");
    is(message.ignore_line, 0, "Ignore buffer flag is clear");
    is(message.ignore_length, 41, "Ignore length is right");
    is(sxe_http_message_consume_parsed_headers(&message), 41, "Consumed the rest of 41 bytes");
}

static void test_five_headers(void)
{
    SXE_HTTP_MESSAGE message;
    unsigned        line_len;

    tap_test_case_name("Five Headers - Normal Long Normal Long-With-Continuations Normal");
    max_buffer_size = 70;

    memset(message_headers, 0, sizeof(message_headers));

    snprintf(message_headers, sizeof(message_headers), "\r\n%s: %s\r\n%s: %s\r\n%s: %s\r\n%s: %s\r\n CONTINUE_LINE1\r\n\tCONTINUE_LINE2\r\n%s: %s\r\n\r\n",
                                                        HEADER(1), VALUE(1),
                                                        HEADER(2), LONG_VALUE(2),
                                                        HEADER(333), VALUE(3333),
                                                        LONG_HEADER(4), VALUE(4),
                                                        HEADER(55), VALUE(5));
    sxe_http_message_construct(&message, message_headers, max_buffer_size);
    line_len = strlen(message_headers);

    diag("Five Headers: The header is %u bytes, the max buffer size is %u bytes", line_len, max_buffer_size);

    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_END_OF_FILE,
                                                "Parsed message request line");

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_OK, "Get header 1");
    is(message.ignore_length, 0, "No ignore");
    is(message.ignore_length, 0, "Ignore length is 0");
    is_strncmp(sxe_http_message_get_header_name(&message), HEADER(1), strlen(HEADER(1)), "Name is " HEADER(1));
    is(sxe_http_message_get_header_name_length(&message), strlen(HEADER(1)), "Name length is %u", (unsigned)strlen(HEADER(1)));
    is_strncmp(sxe_http_message_get_header_value(&message), VALUE(1), strlen(VALUE(1)), "Value is " VALUE(1));
    is(sxe_http_message_get_header_value_length(&message), strlen(VALUE(1)), "Value length is %u", (unsigned)strlen(VALUE(1)));

    /* Don't do any consume since last sxe_http_message_parse_next_header() returns OK */

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(message.ignore_length, 0, "No ignore");
    /* 21 == 2 + strlen(HEADER(1)) + 2 + strlen(VALUE(1)) + 2 */
    is(sxe_http_message_consume_parsed_headers(&message),  21, "Consuming the request line and header 1");
    /* Simulate sxe_buf_consume() */
    memmove(message_headers, &message_headers[21], sizeof(message_headers) - 21);
    diag("Five Headers: Consumed 21 bytes");

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(sxe_http_message_consume_parsed_headers(&message),  0,                     "Buffer is full");
    /* Start ignoring */
    memmove(message_headers, &message_headers[max_buffer_size], sizeof(message_headers) - max_buffer_size);
    diag("Five Headers: Consumed %u bytes", max_buffer_size);
    message.ignore_line = 1;

    sxe_http_message_increase_buffer_length(&message, max_buffer_size);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(message.ignore_line, 0, "Ignore buffer flag is clear, since found the end of long header 2");
    /* 55 = strlen(HEADER(2)) + 2 + strlen(LONG_VALUE(2)) + 2 - 70 */
    is(message.ignore_length, 55, "Ignore length is right");
    memmove(message_headers, &message_headers[55], sizeof(message_headers) - 55);
    diag("Five Headers: Consumed 55 bytes");

    sxe_http_message_increase_buffer_length(&message, max_buffer_size);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_OK, "Get normal header 3");
    is(message.ignore_length, 0, "No ignore");
    is_strncmp(sxe_http_message_get_header_name(&message), HEADER(333), strlen(HEADER(333)), "Name is " HEADER(333));
    is(sxe_http_message_get_header_name_length(&message), strlen(HEADER(333)), "Name length is %u", (unsigned)strlen(HEADER(333)));
    is_strncmp(sxe_http_message_get_header_value(&message), VALUE(3333), strlen(VALUE(3333)), "Value is " VALUE(3333));
    is(sxe_http_message_get_header_value_length(&message), strlen(VALUE(3333)), "Value length is %u", (unsigned)strlen(VALUE(3333)));

    /* Don't do any consume since last sxe_http_message_parse_next_header() returns OK */

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(message.ignore_length, 0, "No ignore");
    /* 24 == strlen(HEADER(333)) + 2 + strlen(VALUE(3333)) + 2 */
    is(sxe_http_message_consume_parsed_headers(&message),  24, "Consuming the header 3");
    /* Simulate sxe_buf_consume() */
    memmove(message_headers, &message_headers[24], sizeof(message_headers) - 24);
    diag("Five Headers: Consumed 24 bytes");

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(sxe_http_message_consume_parsed_headers(&message),  0,                     "Buffer is full");
    /* Start ignoring */
    memmove(message_headers, &message_headers[max_buffer_size], sizeof(message_headers) - max_buffer_size);
    diag("Five Headers: Consumed %u bytes", max_buffer_size);
    message.ignore_line = 1;

    sxe_http_message_increase_buffer_length(&message, max_buffer_size);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header 4");
    is(message.ignore_line, 1, "Ignore buffer flag is set, the end of long header 4 not found yet");
    /* 55 = strlen(HEADER(2)) + 2 + strlen(LONG_VALUE(2)) + 2 - 70 */
    is(message.ignore_length, max_buffer_size, "Ignore length is right");
    memmove(message_headers, &message_headers[max_buffer_size], sizeof(message_headers) - max_buffer_size);
    diag("Five Headers: Consumed %u bytes", max_buffer_size);

    /* 41 = 351(line_len) - 21 - 55 - 24 - 3 * 210 */
    sxe_http_message_increase_buffer_length(&message, 41);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header 4");
    is(message.ignore_line, 0, "Ignore buffer flag is set, the end of long header 4 not found yet");
    /* 19 = strlen("\r\n\tCONTINUE_LINE2\r\n") */
    is(message.ignore_length, 19, "Ignore length is right");
    memmove(message_headers, &message_headers[19], sizeof(message_headers) - 19);
    diag("Five Headers: Consumed 19 bytes, the left part of long header 4");

    /* 22 = 41 - 19 */
    sxe_http_message_increase_buffer_length(&message, 22);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_OK, "Get the header 5");
    is(message.ignore_line, 0, "Ignore buffer flag is clear");
    is(message.ignore_length, 0, "Ignore length is 0");
    is_strncmp(sxe_http_message_get_header_name(&message), HEADER(55), strlen(HEADER(55)), "Name is " HEADER(55));
    is(sxe_http_message_get_header_name_length(&message), strlen(HEADER(55)), "Name length is %u", (unsigned)strlen(HEADER(55)));
    is_strncmp(sxe_http_message_get_header_value(&message), VALUE(5), strlen(VALUE(5)), "Value is " VALUE(5));
    is(sxe_http_message_get_header_value_length(&message), strlen(VALUE(5)), "Value length is %u", (unsigned)strlen(VALUE(5)));

    /* Don't do any consume since last sxe_http_message_parse_next_header() returns OK */

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_END_OF_FILE, "Get long header");
    /* 22 = strlen("HEADER-55: VALUE_5\r\n\r\n") */
    is(sxe_http_message_consume_parsed_headers(&message), 22, "Consumed the rest of 22 bytes which are header 5 and eof");
}

static void test_long_check_two_more_bytes(void)
{
    SXE_HTTP_MESSAGE message;
    unsigned        line_len;

    tap_test_case_name("Check Two More Bytes");
    max_buffer_size = 6;

    memset(message_headers, 0, sizeof(message_headers));

    /* "HEADER: VA" is 6 + 4 bytes */
    snprintf(message_headers, sizeof(message_headers), "\r\nHEADER: VA\r\n\r\n");
    sxe_http_message_construct(&message, message_headers, max_buffer_size);
    line_len = strlen(message_headers);
    diag("Check Two More Bytes: The header is %u bytes, the max buffer size is %u bytes", line_len, max_buffer_size);

    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_END_OF_FILE,
                                                "Parsed message request line");

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(sxe_http_message_consume_parsed_headers(&message),  2,                     "Consuming the request line");
    /* Simulate sxe_buf_consume() */
    memmove(message_headers, &message_headers[2], sizeof(message_headers) - 2);
    diag("Check Two More Bytes: Consumed %u bytes", 2);

    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(sxe_http_message_consume_parsed_headers(&message),  0,                     "Buffer is full");

    /* Start ignoring */
    memmove(message_headers, &message_headers[max_buffer_size], sizeof(message_headers) - max_buffer_size);

    diag("Check Two More Bytes: Consumed %u bytes", max_buffer_size);
    message.ignore_line = 1;

    /* 6 = strlen(": VA\r\n\r\n") - 2 */
    sxe_http_message_increase_buffer_length(&message, 6);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Get long header");
    is(message.ignore_line, 1, "Ignore buffer flag is set");
    /* 4 = strlen(":VA") */
    is(message.ignore_length, 4, "Ignore length is right, the ending '\\r\\n' is left for next time");

    memmove(message_headers, &message_headers[4], sizeof(message_headers) - 4);
    diag("Check Two More Bytes: Consumed 4 bytes");

    /* Now buffer is left with "\r\n\r\n" */
    sxe_http_message_increase_buffer_length(&message, 4);
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_END_OF_FILE, "Get the end of line");
    is(message.ignore_line, 0, "Ignore buffer flag is clear");
    is(message.ignore_length, 4, "Ignore length is right");
    is(sxe_http_message_consume_parsed_headers(&message), 4, "Consumed the rest of 4 bytes");
}

/* These test cases are designed for the caller's usage (which is lib-sxe-httpd).
 * Please refer to sxe_httpd_event_read() for how the "ignore" feature is being used.
 */
int
main(void)
{
    plan_tests(96);

    test_long_name();
    test_long_value();
    test_long_value_with_line_continuations();
    test_five_headers();
    test_long_check_two_more_bytes();

    return exit_status();
}

