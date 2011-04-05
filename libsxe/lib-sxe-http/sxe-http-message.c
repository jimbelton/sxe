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

#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "sxe-http.h"
#include "sxe-log.h"
#include "sxe-util.h"

#define SXE_HTTP_LINE_PARSED ~0U

/**
 * Construct a message from a received buffer
 *
 * @param message       Pointer to a message object
 * @param buffer        Received buffer
 * @param buffer_length Amount of data (used) in the receive buffer
 */
void
sxe_http_message_construct(SXE_HTTP_MESSAGE * message, const char * buffer, unsigned buffer_length)
{
    SXEE84("%s(message=%p, buffer=%p, buffer_length=%u)", __func__, message, buffer, buffer_length);
    message->buffer         = buffer;
    message->buffer_length  = buffer_length;
    message->consumed       = 0;
    message->element_length = 0;
    message->next_field     = 0;
    message->ignore_line    = 0;
    message->ignore_length  = 0;
    SXER80("return");
}

/**
 * Increase the amount of data in a received buffer
 *
 * @param message       Pointer to a message object
 * @param buffer_length Amount of data (used) in the receive buffer
 *
 * @exception Aborts if buffer_length is *less* than the previous value
 */
void
sxe_http_message_increase_buffer_length(SXE_HTTP_MESSAGE * message, unsigned buffer_length)
{
    SXEE83("%s(message=%p, buffer_length=%u)", __func__, message, buffer_length);
    SXEA13(buffer_length >= message->buffer_length, "%s: new buffer_length %u is not bigger than previous length %u", __func__,
           buffer_length,   message->buffer_length);
    message->buffer_length = buffer_length;
    SXER80("return");
}

static SXE_RETURN
sxe_http_message_skip_whitespace(SXE_HTTP_MESSAGE * message, unsigned * offset_inout)
{
    SXE_RETURN result = SXE_RETURN_WARN_WOULD_BLOCK;

    SXEE83("%s(message=%p, *offset_inout=%u)", __func__, message, *offset_inout);

    for (;;) {
        if (*offset_inout >= message->buffer_length) {
            SXEL80("Fragment: Partial message ends between tokens");
            break;
        }

        if (message->buffer[*offset_inout] != ' ' && message->buffer[*offset_inout] != '\t') {
            result = SXE_RETURN_OK;
            break;
        }

        (*offset_inout)++;
    }

    SXER82("return result=%s // *offset_inout=%u", sxe_return_to_string(result), *offset_inout);
    return result;
}

/**
 * Parse the next element from a message's request or response line
 *
 * @param message Pointer to a message object
 * @param type    Type of element to parse: SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN for the next whitespace terminated token or
 *                SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE for the remainder of the line
 *
 * @return SXE_RETURN_OK if an element is found, SXE_RETURN_END_OF_FILE if the end of the line has already been reached,
 *         SXE_RETURN_WARN_WOULD_BLOCK if a partial element is found, or SXE_RETURN_ERROR_BAD_MESSAGE_RECEIVED if there was an
 *         error parsing the line.
 */
SXE_RETURN
sxe_http_message_parse_next_line_element(SXE_HTTP_MESSAGE * message, SXE_HTTP_LINE_ELEMENT_TYPE type)
{
    SXE_RETURN   result     = SXE_RETURN_WARN_WOULD_BLOCK;
    const char * terminator = type == SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN ? " \t\r" : "\r";
    unsigned     next_field = message->next_field;
    unsigned     offset;

    SXEE84("%s(message=%p,type=%s) // message->buffer_length=%u", __func__, message,
           type == SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN ? "TOKEN" : "END_OF_LINE", message->buffer_length);
    SXEA11(message->buffer != NULL, "%s: Message has a NULL buffer pointer", __func__);

    if (message->element_length == SXE_HTTP_LINE_PARSED
     || (next_field >= 2 && message->buffer[next_field - 2] == '\r' && message->buffer[next_field - 1] == '\n'))
    {
        SXEL80("The request/response line has already been parsed");
        message->element_length = SXE_HTTP_LINE_PARSED;
        result                  = SXE_RETURN_END_OF_FILE;
        goto SXE_EARLY_OUT;
    }

    /* If the current header field has already been returned or this is the first call, move along
     */
    if (message->next_field != 0 || message->consumed == 0) {
        message->element_length = 0;
        message->consumed       = message->next_field;
        message->next_field     = 0;
    }

    /* If the element has not been found yet, skip over whitespace
     */
    if (message->element_length == 0) {
        if (sxe_http_message_skip_whitespace(message, &message->consumed) != SXE_RETURN_OK) {
            goto SXE_EARLY_OUT;
        }
    }

    if (message->buffer_length - message->consumed < SXE_LITERAL_LENGTH("\r\n")) {
        SXEL81("Fragment: Tiny partial request/response line element ('%c')", message->buffer[message->consumed]);
        goto SXE_EARLY_OUT;
    }

    /* If the end of headers is found, return EOF
     */
    if (message->buffer[message->consumed] == '\r' && message->buffer[message->consumed + 1] == '\n') {
        message->consumed      += 2;
        message->next_field     = message->consumed;
        message->element_length = SXE_HTTP_LINE_PARSED;
        result                  = SXE_RETURN_END_OF_FILE;
        goto SXE_EARLY_OUT;
    }

    for (offset = message->consumed; strchr(terminator, message->buffer[offset]) == NULL; offset++) {
        if (offset >= message->buffer_length) {
            SXEL80("Fragment: Partial message request/response line element");
            message->element_length = offset - message->consumed;
            message->consumed       = offset;
            goto SXE_EARLY_OUT;
        }
    }

    message->element_length = offset - message->consumed;

    if (message->buffer[offset] == '\r') {
         if (++offset >= message->buffer_length) {
             SXEL80("Fragment: Partial message request/response line terminator");
             message->element_length = offset - message->consumed;
             goto SXE_EARLY_OUT;
         }

         if (message->buffer[offset] != '\n') {
            SXEL32("%s: Bad message: request/response line contains a carriage return followed by '%c'",
                   __func__, message->buffer[offset]);
            result = SXE_RETURN_ERROR_BAD_MESSAGE_RECEIVED;
            goto SXE_ERROR_OUT;
        }
    }
    else if (type == SXE_HTTP_LINE_ELEMENT_TYPE_END_OF_LINE) {
        SXEL90("Fragment: Haven't reached end of request/response line");
        goto SXE_EARLY_OUT;
    }

    SXEL83("Parsed %s message line: '%.*s'", type == SXE_HTTP_LINE_ELEMENT_TYPE_TOKEN ? "token from" : "rest of",
           message->element_length, &message->buffer[message->consumed]);
    message->next_field = offset + 1;
    result              = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    SXER81("return result=%s", sxe_return_to_string(result));
    return result;
}

/**
 * Parse the next header from a message
 *
 * @param message Pointer to a message object
 *
 * @return SXE_RETURN_OK if a header was found, SXE_RETURN_END_OF_FILE if the end of the headers was reached,
 *         SXE_RETURN_WARN_WOULD_BLOCK if a partial header is found, or SXE_RETURN_ERROR_BAD_MESSAGE_RECEIVED if there was an
 *         error parsing the headers.
 */
SXE_RETURN
sxe_http_message_parse_next_header(SXE_HTTP_MESSAGE * message)
{
    SXE_RETURN result = SXE_RETURN_ERROR_BAD_MESSAGE_RECEIVED;
    unsigned   offset;

    SXEE82("%s(message=%p)", __func__, message);

    if (message->element_length != SXE_HTTP_LINE_PARSED) {
        SXEA80(message->next_field >= 2, "The request/response line has not been parsed yet");
        SXEA80(message->buffer[message->next_field - 2] == '\r' && message->buffer[message->next_field - 1] == '\n',
               "The request/response line is not terminated by '\\r\\n' or was not completely parsed");
        message->element_length = SXE_HTTP_LINE_PARSED;
    }

    /* Caller will ignore(consume) ignore_length byte of data, so need to reset it each time */
    message->ignore_length = 0;

    /* If the current header field has already been returned or this is the first call after parsing the line, move along
     */
    if (message->next_field != 0) {
        SXEL81("Current header field returned or this is the first call after parsing the line(next_field=%u)", message->next_field);
        message->consumed     = message->next_field;
        message->name_length  = 0;
        message->value_offset = 0;
        message->value_length = 0;
        message->next_field   = 0;

        if (message->buffer_length - message->consumed < SXE_LITERAL_LENGTH("\r\n")) {
            SXEL81("Fragment: Tiny partial header field ('%c')", message->buffer[message->consumed]);
            result = SXE_RETURN_WARN_WOULD_BLOCK;
            goto SXE_EARLY_OUT;
        }

        /* If the end of headers is found, return EOF.
         */
        if (message->buffer[message->consumed] == '\r' && message->buffer[message->consumed + 1] == '\n') {
            message->consumed += 2;
            result            = SXE_RETURN_END_OF_FILE;
            goto SXE_EARLY_OUT;
        }
    }

    if (message->ignore_line) {
        SXEL80("Ignore current line: trying to find the ending '\n'");
        for (;;) {
            for (offset = message->consumed; message->buffer[offset] != '\n'; offset++) {
                if (offset >= message->buffer_length) {
                    message->ignore_length  = message->buffer_length;
                    SXEL81("Ignore current line: Partial header, %u bytes to ignore", message->ignore_length);
                    goto SXE_IGNORE_LINE_EARLY_OUT;
                }
            }

            /* Need at least two more characters to check end of header or line continuation.
             */
            if (offset + 2 >= message->buffer_length) {
                message->ignore_length  = message->buffer_length - 2; /* save the ending '\n' for next time */
                SXEL81("Ignore current line: Partial end of header field, %u bytes to ignore", message->ignore_length);
                goto SXE_IGNORE_LINE_EARLY_OUT;
            }

            if (message->buffer[offset + 1] == '\r' && message->buffer[offset + 2] == '\n') {
                message->ignore_line    = 0;
                message->ignore_length  = message->consumed = offset + 3;
                message->buffer_length -= message->ignore_length;
                result                  = SXE_RETURN_END_OF_FILE;
                goto SXE_EARLY_OUT;
            }

            /* If this is a not a continuation, the end of line is found
             */
            if (message->buffer[++offset] != ' ' && message->buffer[offset] != '\t') {
                message->ignore_length  = offset;
                message->ignore_line    = 0;

                /* reset all the offset and lengths */
                message->name_length    = 0;
                message->value_offset   = 0;
                message->value_length   = 0;

                SXEL81("Ignore current line: Found the end of current line, %u bytes to ignore", message->ignore_length);
                goto SXE_IGNORE_LINE_EARLY_OUT;
            }

            /* It is a line continuation, carry on */
            SXEL80("Ignore current line: Found a line continuation, carry on");
            message->consumed = offset;
        }
    }

    /* If the head field name has not been parsed yet, do so.
     */
    if (message->value_offset == 0) {
        SXEL82("Value offset is 0, consumed is %u, name length is %u", message->consumed, message->name_length);
        if (message->name_length == 0 && message->buffer[message->consumed] == ':') {
            SXEL31("%s: Bad message: header field begins with ':'", __func__);
            goto SXE_ERROR_OUT;
        }

        /* Look for the end of the header field name.
         */
        for (offset = message->consumed + message->name_length; message->buffer[offset] != ':'; offset++) {
            if (offset >= message->buffer_length) {
                SXEL82("Fragment: Partial header field name '%.*s'", offset - message->consumed,
                       &message->buffer[message->consumed]);
                message->name_length = offset - message->consumed;
                result               = SXE_RETURN_WARN_WOULD_BLOCK;
                goto SXE_EARLY_OUT;
            }

            if (!isgraph(message->buffer[offset])) {    /* isgraph - printable but not a space (see RFC 822 3.1.2) */
                SXEL34("%s: Bad message: header field name contains non-printable character 0x%02x after '%.*s'", __func__,
                       message->buffer[offset], offset - message->consumed, &message->buffer[message->consumed]);
                goto SXE_ERROR_OUT;
            }
        }

        message->name_length  = offset - message->consumed;
        message->value_offset = offset + 1;
    }
    SXEL82("After looking for the header name, value offset is %u, name length is %u", message->value_offset, message->name_length);

    /* If the value hasn't started yet, trim leading spaces and tabs.  Note: Leading line continuations are not stripped.
     */
    if (message->value_length == 0) {
        SXEL80("Value length is 0");
        for (;;) {
            if (message->value_offset >= message->buffer_length) {
                SXEL80("Fragment: Partial header field before value");
                result                = SXE_RETURN_WARN_WOULD_BLOCK;
                goto SXE_EARLY_OUT;
            }

            if (message->buffer[message->value_offset] != ' ' && message->buffer[message->value_offset] != '\t') {
                break;
            }

            message->value_offset++;
        }
    }
    SXEL81("After skipping the leading SP and TAB, value offset is %u", message->value_offset);

    /* Until the end of the header field value is reached.
     */
    for (;;) {
        /* Look for a return in the header field value.
         * Note: Newlines without a preceding return are allowed (violating RFC 822 3.1.2)
         */
        SXEL80("Look for a return in the header field value");
        for (offset = message->value_offset + message->value_length; message->buffer[offset] != '\r'; offset++) {
            if (offset >= message->buffer_length) {
                SXEL80("Fragment: Partial header field value");
                message->value_length = offset - message->value_offset;
                result                = SXE_RETURN_WARN_WOULD_BLOCK;
                goto SXE_EARLY_OUT;
            }
        }

        message->value_length = offset - message->value_offset;

        /* Need at least two more characters.
         */
        if (offset + 2 >= message->buffer_length) {
            SXEL80("Fragment: Partial end of header field");
            result = SXE_RETURN_WARN_WOULD_BLOCK;
            goto SXE_EARLY_OUT;
        }

        if (message->buffer[++offset] != '\n') {
            SXEL32("%s: Bad message: header field value contains a carriage return followed by '%c'",
                   __func__, message->buffer[offset]);
            goto SXE_ERROR_OUT;
        }

        /* If this is a not a continuation, return the header field.
         */
        if (message->buffer[++offset] != ' ' && message->buffer[offset] != '\t') {
            message->value_length = offset - SXE_LITERAL_LENGTH("\r\n") - message->value_offset;
            message->next_field   = offset;
            break;
        }

        /* This is a line continuation; continue to parse the value.
         */
        SXEL82("Message header field '%.*s' contains a line continuation",
               message->name_length, &message->buffer[message->consumed]);
        message->value_length = offset - message->value_offset;
    }

    /* Trim trailing whitespace off the header field value. Note: Strips trailing formfeeds and vertical tabs, violating RFC
     * 2616 2.2 rules for linear white space (LWS)
     */
    while (message->value_length > 0 && isspace(message->buffer[message->value_offset + message->value_length - 1])) {
        message->value_length--;
    }

    SXEL84("Message header field '%.*s' has value '%.*s'", message->name_length,  &message->buffer[message->consumed],
                                                           message->value_length, &message->buffer[message->value_offset]);
    result = SXE_RETURN_OK;
    goto SXE_EARLY_OUT;

SXE_IGNORE_LINE_EARLY_OUT:
    message->buffer_length -= message->ignore_length;
    message->consumed       = 0;
    result                  = SXE_RETURN_WARN_WOULD_BLOCK;

SXE_EARLY_OR_ERROR_OUT:
    SXER81("return result=%s", sxe_return_to_string(result));
    return result;
}

/**
 * Consume the headers parsed from a message
 *
 * @param message Pointer to a message object
 *
 * @return The amount of data consumed
 *
 * @note Before calling sxe_http_message_parse_next_header() again, the remaining data in the received buffer, if any, must be
 *       moved to the beginning of the buffer.
 */
unsigned
sxe_http_message_consume_parsed_headers(SXE_HTTP_MESSAGE * message)
{
    unsigned consumed;

    SXEE82("%s(message=%p)", __func__, message);

    /* If the current header field has already been returned, consume it
     */
    if (message->next_field != 0) {
        consumed              = message->next_field;
        message->name_length  = 0;
        message->value_offset = 0;
        message->value_length = 0;
        message->next_field   = 0;
        goto SXE_EARLY_OUT;
    }

    consumed = message->consumed;

    if (message->value_offset != 0) {
        message->value_offset -= consumed;
    }

SXE_EARLY_OUT:
    message->consumed       = 0;
    message->buffer_length -= consumed;
    SXER81("return consumed=%u", consumed);
    return consumed;
}
