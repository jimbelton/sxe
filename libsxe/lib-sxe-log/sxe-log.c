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

#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h> /* for exit() & getenv() */
#include <stdarg.h> /* for va_list */
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>  /* for GetCurrentThreadId() */
#include <stdio.h>    /* for fopen() */
#else
#include <sys/time.h> /* for gettimeofday() */
#include <unistd.h>   /* for getpid() */
#endif

#include "sxe-log.h"

#define SXE_U32                     unsigned int
#define SXE_S32                     int

#define SXE_LOG_BUFFER_SIZE         1024
#define SXE_LOG_BUFFER_SIZE_ASSERT  1024

#define SXE_LOG_LEVEL_MINIMUM       (SXE_LOG_LEVEL_UNDER_MINIMUM + 1)
#define SXE_LOG_LEVEL_MAXIMUM       (SXE_LOG_LEVEL_OVER_MAXIMUM  - 1)

#ifndef SXE_LOG_LEVEL_DEFAULT
#define SXE_LOG_LEVEL_DEFAULT       SXE_LOG_LEVEL_MAXIMUM
#endif

typedef union SXE_PTR_UNION
{
    char           * as_chr_ptr       ;
    unsigned char  * as_u08_ptr       ;
    signed   char  * as_s08_ptr       ;
    SXE_U32        * as_u32_ptr       ;
    SXE_S32        * as_s32_ptr       ;
    SXE_U32          as_u32           ; /* Useful for debug build only */
    void           * as_void_ptr      ;
    void const     * as_void_ptr_const; /* Useful for debug build only */
} SXE_PTR_UNION;

static unsigned sxe_log_is_first_call = 1;

#ifdef _WIN32
    static DWORD    sxe_log_tls_slot;
#   define SXE_LOG_INDENT             (sxe_log_get_indent_windows())
#   define SXE_LOG_SET_INDENT(indent) (sxe_log_set_indent_windows(indent))
#else
    static unsigned sxe_log_indent;
#   define SXE_LOG_INDENT             sxe_log_indent
#   define SXE_LOG_SET_INDENT(indent) (sxe_log_indent = (indent))
#endif

#define SXE_CHECK_LOG_INITIALIZE()                 {if (sxe_log_is_first_call) { sxe_log_init(); }}
#define SXE_CHECK_LEVEL_BEFORE_ANYTHING_ELSE() {if (level > sxe_log_level) { return; }}

SXE_LOG_LEVEL sxe_log_level = SXE_LOG_LEVEL_DEFAULT;

#ifdef _WIN32
static unsigned
sxe_log_get_indent_windows(void)
{
    unsigned indent;

    DWORD error = GetLastError();
    indent = (unsigned)TlsGetValue(sxe_log_tls_slot);
    SetLastError(error);
    return indent;
}

static void
sxe_log_set_indent_windows(unsigned indent)
{
    DWORD error = GetLastError();

    (void)TlsSetValue(sxe_log_tls_slot, (void *)indent);
    SetLastError(error);
}
#endif

#define SXE_RETURN_CASE(ret) case SXE_RETURN_ ## ret: return #ret

const char *
sxe_return_to_string(SXE_RETURN ret)
{
    /* Convert to string.  If any enumerand is missing, should get a compiler error (works with gcc).
     */
    switch (ret) {
    SXE_RETURN_CASE(OK);
    SXE_RETURN_CASE(UNCATEGORIZED);
    SXE_RETURN_CASE(WARN_CACHE_DOUBLE_INITIALIZED);
    SXE_RETURN_CASE(WARN_WOULD_BLOCK);
    SXE_RETURN_CASE(ERROR_CACHE_UNINITIALIZED);
    SXE_RETURN_CASE(ERROR_ALLOC);
    SXE_RETURN_CASE(ERROR_INTERNAL);
    SXE_RETURN_CASE(ERROR_NO_CONNECTION);
    SXE_RETURN_CASE(ERROR_ALREADY_CONNECTED);
    SXE_RETURN_CASE(ERROR_INVALID_URI);
    SXE_RETURN_CASE(ERROR_ADDRESS_IN_USE);
    SXE_RETURN_CASE(WARN_ALREADY_CLOSED);
    SXE_RETURN_CASE(ERROR_INTERRUPTED);
    SXE_RETURN_CASE(ERROR_COMMAND_NOT_RUN);
    SXE_RETURN_CASE(INVALID_VALUE);            /* Required to make compiler happy */
    }

    return "INVALID_VALUE";
}

static int
sxe_log_safe_append(char * log_buffer, unsigned * index_ptr, int appended)
{
    unsigned i;

    /* printf("DEBUG: sxe_log_safe_append: i=%d, appended=%d, remaining=%d\n", *index_ptr, appended, SXE_LOG_BUFFER_SIZE - *index_ptr); */

    /* If output was truncated, Win32 returns negative result while Linux returns positive result! */

    if ((appended < 0) || ((unsigned)appended >= SXE_LOG_BUFFER_SIZE - *index_ptr))
    {
        i = SXE_LOG_BUFFER_SIZE - 1;
        log_buffer[i - 0] = '\0';   /* snprintf() will give up & not append a \0    */
        log_buffer[i - 1] = '\n';   /* in which case a \n also comes in handy       */
        log_buffer[i - 2] = '.';
        log_buffer[i - 3] = '.';
        return 0;
    }

    *index_ptr += appended;
    return 1;
}

#ifdef WIN32
#define  SNPRINTF  _snprintf
#define VSNPRINTF _vsnprintf
#else
#define  SNPRINTF   snprintf
#define VSNPRINTF  vsnprintf
#endif

static void
sxe_log_line_out_default(SXE_LOG_LEVEL level, char * line)    /* Coverage Exclusion - default version for testing */
{                                                             /* Coverage Exclusion - default version for testing */
    SXE_UNUSED_ARGUMENT(level);

    fputs(line, stderr);                                      /* Coverage Exclusion - default version for testing */
    fflush(stderr);                                           /* Coverage Exclusion - default version for testing */
}

static void (*sxe_log_line_out)(SXE_LOG_LEVEL level, char * line) = &sxe_log_line_out_default;

void
sxe_log_hook_line_out(void (*line_out)(SXE_LOG_LEVEL level, char *))
{
    if (line_out == NULL) {
        sxe_log_line_out = sxe_log_line_out_default;
    }
    else {
        sxe_log_line_out = line_out;
    }
}

static void
sxe_log_line_out_escaped(SXE_LOG_LEVEL level, char * line)
{
    unsigned from;
    unsigned to;
    char     line_escaped[SXE_LOG_BUFFER_SIZE * 4];   /* Worse case, all hex encoded */

    for (from = 0; (line[from] != '\0') && (isprint((unsigned char)line[from]) || (line[from] == ' ')); from++) {
    }

    if (line[from] != '\0') {
        memcpy(line_escaped, line, from);

        for (to = from; line[from] != '\0'; from++) {
            if (line[from] == '\\' ) {
                line_escaped[to++] = '\\';
                line_escaped[to++] = '\\';
            }
            else if (line[from] == '\n' ) {
                line_escaped[to++] = '\\';
                line_escaped[to++] = 'n' ;
            }
            else if (line[from] == '\r' ) {
                line_escaped[to++] = '\\';
                line_escaped[to++] = 'r' ;
            }
            else if (isprint((unsigned char)line[from])) {
                line_escaped[to++] = line[from];
            }
            else {
                to += SNPRINTF(&line_escaped[to], 5, "\\x%02X", (unsigned)(unsigned char)line[from]);
            }
        }

        if (to > SXE_LOG_BUFFER_SIZE)
        {
            to = SXE_LOG_BUFFER_SIZE;
            line_escaped[to - 4] = '.';
            line_escaped[to - 3] = '.';
        }

        line_escaped[to - 2] = '\n';
        line_escaped[to - 1] = '\0';
        line = line_escaped;
    }

    (*sxe_log_line_out)(level, line);
}

static unsigned
sxe_log_buffer_set_prefix(char * log_buffer, unsigned id, SXE_LOG_LEVEL level)
{
    unsigned         length;
#if defined(WIN32)
    SYSTEMTIME       st;
    DWORD            ThreadId;
#else
    struct timeval   mytv;
    struct tm        mytm;
    pid_t            ProcessId;
#endif

#if defined(WIN32)
    GetSystemTime(&st);
    ThreadId  = GetCurrentThreadId();

    SNPRINTF(log_buffer, SXE_LOG_BUFFER_SIZE, "%04d%02d%02d %02d%02d%02d.%03d T%08x ", st.wYear, st.wMonth, st.wDay, st.wHour,
             st.wMinute, st.wSecond, st.wMilliseconds, ThreadId);
#else
    gettimeofday(&mytv, NULL);
    gmtime_r((time_t *)&mytv.tv_sec, &mytm);
    ProcessId = getpid();

    SNPRINTF(log_buffer, SXE_LOG_BUFFER_SIZE, "%04d%02d%02d %02d%02d%02d.%03ld P%08x ", mytm.tm_year + 1900, mytm.tm_mon+1,
             mytm.tm_mday, mytm.tm_hour, mytm.tm_min, mytm.tm_sec, mytv.tv_usec / 1000, ProcessId);
#endif

    if (id == ~0U) {
        strcat(log_buffer, "------ ");
    }
    else {
        SNPRINTF(&log_buffer[strlen(log_buffer)], SXE_LOG_BUFFER_SIZE, "%6u ", id);
    }

    length = strlen(log_buffer);
    log_buffer[length++]    = '0' + level;
    log_buffer[length++]    = ' ';
    log_buffer[length]      = '\0';
    return length;
}

void
sxe_log_init(void)
{
    if (!sxe_log_is_first_call) {
        return;
    }

#ifdef _WIN32
    if ((sxe_log_tls_slot = TlsAlloc()) == TLS_OUT_OF_INDEXES) {
        (*sxe_log_line_out)(SXE_LOG_LEVEL_FATAL, "sxe_log_init: Unable to allocate thread local storage\n");
        abort();
    }
#endif

    SXE_LOG_SET_INDENT(0);    /* Not needed under Windows, where TLS is initialized to 0 by the OS */
    sxe_log_is_first_call = 0;

    if (NULL != getenv("SXE_LOG_LEVEL")) {
        sxe_log_level = atoi(getenv("SXE_LOG_LEVEL"));
        sxe_log_level = sxe_log_level <= SXE_LOG_LEVEL_MINIMUM ? SXE_LOG_LEVEL_MINIMUM : sxe_log_level;
        sxe_log_level = sxe_log_level >= SXE_LOG_LEVEL_MAXIMUM ? SXE_LOG_LEVEL_MAXIMUM : sxe_log_level;
    }
}

void
sxe_log(unsigned id, SXE_LOG_LEVEL level, const char *fmt, ...)
{
    char  log_buffer[SXE_LOG_BUFFER_SIZE];
    va_list  ap;
    unsigned i;

    SXE_CHECK_LOG_INITIALIZE();
    SXE_CHECK_LEVEL_BEFORE_ANYTHING_ELSE();

    i = sxe_log_buffer_set_prefix(log_buffer, id, level);
    va_start(ap, fmt);

    if (!sxe_log_safe_append(log_buffer, &i,  SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "%*s- ", SXE_LOG_INDENT * 2, "")) ||
        !sxe_log_safe_append(log_buffer, &i, VSNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, fmt    , ap                          )))
    {
        goto SXE_EARLY_OUT;
    }

    sxe_log_safe_append( log_buffer, &i,  SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "\n"));

SXE_EARLY_OR_ERROR_OUT:
    sxe_log_line_out_escaped(level, log_buffer);
}

void
sxe_log_entry(unsigned id, SXE_LOG_LEVEL level, const char * file, int line, const char *fmt, ...)
{
    char  log_buffer[SXE_LOG_BUFFER_SIZE];
    va_list  ap;
    unsigned i;
    unsigned prefix_length;

    SXE_CHECK_LOG_INITIALIZE();
    SXE_CHECK_LEVEL_BEFORE_ANYTHING_ELSE();

    prefix_length   = sxe_log_buffer_set_prefix(log_buffer, id, level);
    i               = prefix_length;
    va_start(ap, fmt);

    if (sxe_log_safe_append(log_buffer, &i,  SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "%*s+ ", SXE_LOG_INDENT * 2, ""))
    &&  sxe_log_safe_append(log_buffer, &i, VSNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, fmt, ap))) {
        sxe_log_safe_append(log_buffer, &i,  SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "\n"));
    }

    sxe_log_line_out_escaped(level, log_buffer);
    i = prefix_length;
    sxe_log_safe_append(log_buffer, &i,  SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "%*s  { // %s:%d\n",
                        SXE_LOG_INDENT * 2, "", file, line));
    sxe_log_line_out_escaped(level, log_buffer);
    SXE_LOG_SET_INDENT(SXE_LOG_INDENT + 1);
}

void
sxe_log_return(unsigned id, SXE_LOG_LEVEL level, const char * file, int line )
{
    char  log_buffer[SXE_LOG_BUFFER_SIZE];
    unsigned i;

    SXE_CHECK_LOG_INITIALIZE();
    SXE_CHECK_LEVEL_BEFORE_ANYTHING_ELSE();
    SXEA61(SXE_LOG_INDENT > 0, "Indentation level %d must be greater than zero!\n", SXE_LOG_INDENT);

    i = sxe_log_buffer_set_prefix(log_buffer, id, level);
    sxe_log_safe_append( log_buffer, &i,  SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "%*s} // %s:%d\n",
                                  SXE_LOG_INDENT * 2, "", file, line));
    SXE_LOG_SET_INDENT(SXE_LOG_INDENT - 1);

SXE_EARLY_OR_ERROR_OUT:
    sxe_log_line_out_escaped(level, log_buffer);
}

void
sxe_log_assert(unsigned id, const char *file, int line, const char *con, const char *fmt, ...)
{
    char  log_buffer[SXE_LOG_BUFFER_SIZE_ASSERT];
    va_list  ap;
    unsigned i = 0;

    SXE_CHECK_LOG_INITIALIZE();

    i = sxe_log_buffer_set_prefix(log_buffer, id, 1);
    va_start(ap, fmt);

    if (sxe_log_safe_append(log_buffer, &i,  SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "ERROR: debug assertion '%s' failed at %s:%d; ", con, file, line))
    &&  sxe_log_safe_append(log_buffer, &i, VSNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, fmt, ap))) {
        sxe_log_safe_append(log_buffer, &i,  SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "\n"));
    }

SXE_EARLY_OR_ERROR_OUT:
    sxe_log_line_out_escaped(SXE_LOG_LEVEL_FATAL, log_buffer);
    abort();
}

void
sxe_log_dump_memory(unsigned id, SXE_LOG_LEVEL level, const void * pointer, unsigned length)
{
    char            log_buffer[SXE_LOG_BUFFER_SIZE];
    unsigned        prefix_length;
    unsigned        c;
    unsigned        i;
    unsigned        j;
    unsigned        k;
    SXE_PTR_UNION   memory;

    SXE_CHECK_LOG_INITIALIZE();
    SXE_CHECK_LEVEL_BEFORE_ANYTHING_ELSE();

    prefix_length               = sxe_log_buffer_set_prefix(log_buffer, id, level);
    memory.as_void_ptr_const    = pointer;

    /* TODO: pretty up dumping :-) */
    /* - 0x12345678                   56 78 12 34 56 78 12 34 56 78       .......... */
    /* - 0x12345678 12 34 56 78 12 34 56 78 12 34 56 78 12 34 56 78 ................ */
    /* - 0x12345678 12 34 56 78                                     ....             */

    for (k = 0; k <= length / 16; k++)
    {
        i = prefix_length;

        if (!sxe_log_safe_append( log_buffer, &i, SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "%*s- ",
                                  SXE_LOG_INDENT * 2, ""))
         || !sxe_log_safe_append( log_buffer, &i, SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i,
                                  sizeof(unsigned long) > 4 ? "%016lx" : "%08lx",
                                  (unsigned long)(memory.as_u08_ptr + (k * 16)))))
        {
            goto SXE_EARLY_OUT;
        }

        /* print 16 bytes unless we get to the end */
        for(j = 0; j < 16; j++) {
            if (((k * 16) + j) < length)
            {
                if (!sxe_log_safe_append( log_buffer, &i, SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, " %02x", memory.as_u08_ptr[(k * 16) + j]))) {
                    goto SXE_EARLY_OUT;     /* COVERAGE EXCLUSION: Line truncation in sxe_log_dump_memory function */
                }
            }
            else
            {
                if (!sxe_log_safe_append( log_buffer, &i, SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "   "))) {
                    goto SXE_EARLY_OUT;     /* COVERAGE EXCLUSION: Line truncation in sxe_log_dump_memory function */
                }
            }
        }

        if (!sxe_log_safe_append( log_buffer, &i, SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, " "))) {
            goto SXE_EARLY_OUT;             /* COVERAGE EXCLUSION: Line truncation in sxe_log_dump_memory function */
        }

        for (j = 0; (j < 16) && (((k * 16) + j) < length); j++) {
            c = memory.as_u08_ptr[(k * 16) + j];
            c = c < 0x20 ? '.' : c;
            c = c > 0x7E ? '.' : c;

            if (!sxe_log_safe_append( log_buffer, &i, SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "%c", c))) {
                goto SXE_EARLY_OUT;         /* COVERAGE EXCLUSION: Line truncation in sxe_log_dump_memory function */
            }
        }

        sxe_log_safe_append( log_buffer, &i, SNPRINTF(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "\n"));

SXE_EARLY_OR_ERROR_OUT:
        sxe_log_line_out_escaped(level, log_buffer);
    } /* for (k = ... */
} /* sxe_debug_dump_memory() */

/* TODO: refactor to form: sxe_debug_map_enum_to_string_<enum type> */
