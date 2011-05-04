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
#include "tap.h"

#define CONTENT_LENGTH                 "Content-Length"
#define MESSAGE_HAPPY                  "GET  /\r\n" CONTENT_LENGTH ": 12\r\n \r\n\r\n"
#define MESSAGE_BAD_REQUEST_LINE       "GET\r#\n"
#define MESSAGE_BAD_HEADER_COLON       "\r\n:Mal-Content-Length: 0"
#define MESSAGE_BAD_HEADER_SPACE       "\r\nDis-Content Length: -1"
#define MESSAGE_BAD_HEADER_UNPRINTABLE "\r\nBond-\007: James"
#define MESSAGE_BAD_HEADER_CR_NO_LF    "\r\nContent-Length: 12\r#\n\r\n"
#define TEST_MAX_BUF                   1500

#define LONG_HEADER_VALUE \
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" \
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" \
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" \
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" \
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" \
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" \
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" \
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
#define HEADER_1_NAME        "Header-1"
#define HEADER_1             HEADER_1_NAME ": " LONG_HEADER_VALUE "\r\n"
#define HEADER_2_NAME        "Header-2"
#define HEADER_2             HEADER_2_NAME ": " LONG_HEADER_VALUE "\r\n"
#define MESSAGE_LONG_HEADERS "\r\n" HEADER_1 HEADER_2 "\r\n"

#define MESSAGE_MULTI_HEADER        "GET /this/is/a/URL HTTP/1.1\r\nConnection: whatever\r\nHost: interesting\r\n" \
                                    "\r\nContent-Length: 10\r\n\r\n12345678\r\n"
#define MESSAGE_MULTI_HEADER_OFFSET 29

/* Long headers must be larger than TEST_MAX_BUF
 */
static char message_long_headers[] = MESSAGE_LONG_HEADERS;

int
main(void)
{
    SXE_HTTP_MESSAGE message;
    plan_tests(59);

    tap_test_case_name("defragmentation");
    sxe_http_message_construct(&message, MESSAGE_HAPPY, 0);
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN), SXE_RETURN_WARN_WOULD_BLOCK,
                                                                                  "Parsed tiny partial request:   WOULD_BLOCK");
    sxe_http_message_increase_buffer_length(&message, 1);                               /*  "G" = Not even enough for \r\n    */
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN), SXE_RETURN_WARN_WOULD_BLOCK,
                                                                                  "Parsed tiny partial element:   WOULD_BLOCK");
    sxe_http_message_increase_buffer_length(&message, 2);                               /*  "GE" = Partial token      */
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN), SXE_RETURN_WARN_WOULD_BLOCK,
                                                                                  "Parsed partial line element:   WOULD_BLOCK");
    sxe_http_message_increase_buffer_length(&message, 4);                               /*  "GET " = Full token      */
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN), SXE_RETURN_OK,
                                                                                  "Parsed full token 'GET'");
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_WARN_WOULD_BLOCK,
                                                                                  "Parsed leading whitespace:     WOULD_BLOCK");
    sxe_http_message_increase_buffer_length(&message, strlen("GET  /\r"));              /*   Partial token line ending */
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_WARN_WOULD_BLOCK,
                                                                                  "Parsed partial line ending:    WOULD_BLOCK");
    sxe_http_message_increase_buffer_length(&message, strlen("GET  /\r\nC"));           /*  1 = Not even enough for next \r\n */
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_OK,
                                                                                   "Parsed message request line");
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_END_OF_FILE,
                                                                                   "Parsed message request line a second time");
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Parsing tiny partial header:   WOULD_BLOCK");
    sxe_http_message_increase_buffer_length(&message, strlen("GET  /\r\nCon"));         /* "Con" == partial header field name */
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Parsing partial header name:   WOULD_BLOCK");
    sxe_http_message_increase_buffer_length(&message, strlen("GET  /\r\n" CONTENT_LENGTH ":"));      /* ":" == partial header */
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Parsing partial header split:  WOULD_BLOCK");
    sxe_http_message_increase_buffer_length(&message, strlen("GET  /\r\n" CONTENT_LENGTH ": 1"));    /* "1" == partial header */
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Parsing partial header value:  WOULD_BLOCK");
    sxe_http_message_increase_buffer_length(&message, strlen(MESSAGE_HAPPY) - 3);       /* "\r" == partial end of header      */
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK, "Parsing partial end of header: WOULD_BLOCK");

    tap_test_case_name("happy path");
    sxe_http_message_increase_buffer_length(&message, strlen(MESSAGE_HAPPY));   /* Parse the whole message                    */
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_OK,               "Parsing full header returns OK");
    is(sxe_http_message_get_header_name_length(&message), strlen(CONTENT_LENGTH), "Header name is as long as 'Content-Length'");
    is_strncmp(sxe_http_message_get_header_name(&message), CONTENT_LENGTH, strlen(CONTENT_LENGTH),
                                                                                  "Header name is 'Content-Length'");
    is(sxe_http_message_get_header_value_length(&message), strlen("12"),          "Header value is as long as '12'");
    is_strncmp(sxe_http_message_get_header_value(&message), "12", strlen("12"),   "Header value is '12'");
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_END_OF_FILE,      "Parsing end of header returns END_OF_FILE");

    tap_test_case_name("bad messages");
    sxe_http_message_construct(&message, MESSAGE_BAD_REQUEST_LINE, strlen(MESSAGE_BAD_REQUEST_LINE));
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_ERROR_BAD_MESSAGE,
                                                                                   "Request line has CR w/o LF:   BAD_MESSAGE");
    sxe_http_message_construct(&message, MESSAGE_BAD_HEADER_COLON, strlen(MESSAGE_BAD_HEADER_COLON));
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_END_OF_FILE,
                                                                                   "Parsed message request line");
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_ERROR_BAD_MESSAGE, "Header name begins with ':':  BAD_MESSAGE");
    sxe_http_message_construct(&message, MESSAGE_BAD_HEADER_SPACE, strlen(MESSAGE_BAD_HEADER_SPACE));
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_END_OF_FILE,
                                                                                   "Parsed message request line");
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_ERROR_BAD_MESSAGE, "Header name includes a space: BAD_MESSAGE");
    sxe_http_message_construct(&message, MESSAGE_BAD_HEADER_UNPRINTABLE, strlen(MESSAGE_BAD_HEADER_UNPRINTABLE));
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_END_OF_FILE,
                                                                                   "Parsed message request line");
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_ERROR_BAD_MESSAGE, "Header name includes a bell:  BAD_MESSAGE");
    sxe_http_message_construct(&message, MESSAGE_BAD_HEADER_CR_NO_LF,    strlen(MESSAGE_BAD_HEADER_CR_NO_LF));
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_END_OF_FILE,
                                                                                   "Parsed message request line");
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_ERROR_BAD_MESSAGE, "Header value CR without LF:   BAD_MESSAGE");

    tap_test_case_name("long headers");
    sxe_http_message_construct(&message, message_long_headers, TEST_MAX_BUF);
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_END_OF_FILE,
                                                                                      "Parsed message request line");
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_OK,                   "Parsed first long header");
    is_strncmp(sxe_http_message_get_header_name(&message), HEADER_1_NAME, strlen(HEADER_1_NAME),
                                                                                      "It has the expected header name");
    is(sxe_http_message_get_header_value_length(&message), strlen(LONG_HEADER_VALUE), "Header value is 1024 bytes long");
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK,     "Second long header is incomplete");
    is(sxe_http_message_consume_parsed_headers(&message),  2 + strlen(HEADER_1),      "First header is to be consumed");
    memmove(message_long_headers, &message_long_headers[2 + strlen(HEADER_1)], sizeof(message_long_headers) - (2 + strlen(HEADER_1)));
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK,     "Second long header is still incomplete");
    sxe_http_message_increase_buffer_length(&message, strlen(MESSAGE_LONG_HEADERS) - (2 + strlen(HEADER_1)));
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_OK,                   "Parsed second long header");
    is_strncmp(sxe_http_message_get_header_name(&message), HEADER_2_NAME, strlen(HEADER_2_NAME),
                                                                                      "It has the expected header name");
    is(sxe_http_message_get_header_value_length(&message), strlen(LONG_HEADER_VALUE), "Header value is 1024 bytes long");
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_END_OF_FILE,          "Parsed end of headers");

    /* Header is consumed before sxe_http_message_parse_next_header() has returned WOULD_BLOCK (should not be the normal case)
     */
    tap_test_case_name("early consume");
    memcpy(message_long_headers, MESSAGE_LONG_HEADERS, strlen(MESSAGE_LONG_HEADERS));
    sxe_http_message_construct(&message, message_long_headers, TEST_MAX_BUF);
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_END_OF_FILE,
                                                                                      "Parsed message request line");
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_OK,                   "Parsed first long header");
    is(sxe_http_message_consume_parsed_headers(&message), 2 + strlen(HEADER_1),       "First header is to be consumed");
    memmove(message_long_headers, &message_long_headers[2 + strlen(HEADER_1)], sizeof(message_long_headers) - (2 + strlen(HEADER_1)));
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_WARN_WOULD_BLOCK,     "Second long header is incomplete");
    sxe_http_message_increase_buffer_length(&message, strlen(MESSAGE_LONG_HEADERS) - strlen(HEADER_1));
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_OK,                   "Parsed second long header");
    is_strncmp(sxe_http_message_get_header_name(&message), HEADER_2_NAME, strlen(HEADER_2_NAME),
                                                                                      "It has the expected header name");
    is(sxe_http_message_get_header_value_length(&message), strlen(LONG_HEADER_VALUE), "Header value is 1024 bytes long");
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_END_OF_FILE,          "Parsed end of headers");

    /* This test was added after noticing a bug when the headers started at an offset into the message
     */
    tap_test_case_name("request line & multiple headers");
    sxe_http_message_construct(&message, MESSAGE_MULTI_HEADER, strlen(MESSAGE_MULTI_HEADER));
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN), SXE_RETURN_OK, "Got the 1st token");
    is(sxe_http_message_get_line_element_length(&message), strlen("GET"),             "1st token is 3 bytes long");
    is_strncmp(sxe_http_message_get_line_element(&message), "GET", strlen("GET"),     "1st token is 'GET'");
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN), SXE_RETURN_OK, "Got the 2nd token");
    is(sxe_http_message_get_line_element_length(&message), strlen("/this/is/a/URL"),  "2nd token is 14 bytes long");
    is_strncmp(sxe_http_message_get_line_element(&message), "/this/is/a/URL", strlen("/this/is/a/URL"),
                                                                                      "2nd token is '/this/is/a/URL'");
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_OK,
                                                                                      "Got the rest of the line");
    is(sxe_http_message_get_line_element_length(&message), strlen("HTTP/1.1"),        "Rest of line is 8 bytes long");
    is_strncmp(sxe_http_message_get_line_element(&message), "HTTP/1.1", strlen("HTTP/1.1"),
                                                                                      "Rest of line is 'HTTP/1.1'");
    is(sxe_http_message_parse_next_header(&message), SXE_RETURN_OK,                   "Parsed first multi header");
    is(sxe_http_message_get_header_name_length(&message), strlen("Connection"),       "Header name is as long as 'Connection'");

    /* Test to replicate bug found during WDX cnxd integration
     */
    tap_test_case_name("nearly a complete response line");
    sxe_http_message_construct(&message, "HTTP/1.1 200 ", strlen("HTTP/1.1 200 "));
    is(sxe_http_message_parse_next_line_element(&message, SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE), SXE_RETURN_WARN_WOULD_BLOCK,
                                                                                      "Would block on rest of response line");

    return exit_status();
}

