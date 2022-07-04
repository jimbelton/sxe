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
#include <limits.h>          /* for PATH_MAX                         */
#include <stdarg.h>          /* for va_list                          */
#include <stdio.h>
#include <stdlib.h>          /* for exit() & getenv()                */
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>         /* for GetCurrentThreadId()             */
#include <assert.h>          /* for _set_error_mode()                */
#else
#include <syslog.h>          /* for openlog(), syslog()              */
#include <sys/syscall.h>     /* for syscall(), SYS_gettid            */
#include <sys/time.h>        /* for gettimeofday()                   */
#ifdef __FreeBSD__
#   include <sys/thr.h>
#endif
#include <unistd.h>          /* for getpid()                         */
#include "mock.h"            /* allow mocking openlog() and syslog() */
#endif

#include "sxe-log.h"

/* Undefine mocks for timeGetTime() and gettimeofday(), so that we always use the real ones.
 * This prevents an infinite loop in other tests, which call SXE[RLEA]... macros inside of their
 * mocked timeGetTime and getimeofday functions!
 */
#ifdef _WIN32
#undef timeGetTime
#else
#undef gettimeofday
#endif

#define SXE_LOG_BUFFER_SIZE_ASSERT  SXE_LOG_BUFFER_SIZE

#define SXE_LOG_LEVEL_MINIMUM       (SXE_LOG_LEVEL_UNDER_MINIMUM + 1)
#define SXE_LOG_LEVEL_MAXIMUM       (SXE_LOG_LEVEL_OVER_MAXIMUM  - 1)

#ifndef SXE_LOG_LEVEL_DEFAULT
#define SXE_LOG_LEVEL_DEFAULT       SXE_LOG_LEVEL_MAXIMUM
#endif

#define SXE_LOG_LEVEL_VALIDATE(level, maximum_level)                                                                       \
    if (!((level) >= SXE_LOG_LEVEL_MINIMUM && (level) <= (maximum_level))) {                                               \
        sxe_log_assert(NULL, __FILE__, ~0U, __LINE__, #level " >= SXE_LOG_LEVEL_MINIMUM && " #level " <= " #maximum_level, \
                       "%s(): Detected invalid level %d", __func__, (level));                                              \
    }

/* Global variables used by header in-lines for performance */

#ifdef __APPLE__
pthread_key_t          sxe_log_stack_key;
pthread_key_t          sxe_log_indent_maximum_key;
unsigned               sxe_log_transaction_id = SXE_LOG_NO_ID;    // Apple should support __thread. For now, global on Apple.
#else
__thread SXE_LOG_STACK sxe_log_stack;
__thread unsigned      sxe_log_indent_maximum = 0;                // To allow release mode tests to link, always define
__thread unsigned      sxe_log_transaction_id = SXE_LOG_NO_ID;    // Value displayed in id column if no id passed.
#endif

/* Local variables */

static volatile SXE_LOG_LEVEL     sxe_log_level        = SXE_LOG_LEVEL_OVER_MAXIMUM;
static volatile SXE_LOG_CONTROL * sxe_log_control_list = NULL;
static volatile unsigned          sxe_log_setting_era  = 0;
static unsigned                   sxe_log_options      = 0;

#define SXE_RETURN_CASE(ret) case SXE_RETURN_ ## ret: return #ret

void sxe_log_assert_cb_default(void) { /* do nothing in the name of code coverage */ }

SXE_LOG_ASSERT_CB sxe_log_assert_cb = &sxe_log_assert_cb_default; /* NULL or function to call before abort() in sxe_log_assert() */

/**
 * Set options for logging
 *
 * @param options Flags that control the log.
 */
void
sxe_log_set_options(unsigned options)
{
    sxe_log_options = options;
}

/**
 * Set the id field per thread
 *
 * @param id An integer used as an id that identifies a transaction (connection, etc) or SXE_LOG_NO_ID to clear it
 *
 * @note The SXE[ELR]#I macros allow the identifier to be overridden with a more specific value.
 */
void
sxe_log_set_thread_id(unsigned id)
{
    sxe_log_transaction_id = id;
}

const char *
sxe_return_to_string(SXE_RETURN ret)
{
    /* Convert to string.  If any enumerand is missing, should get a compiler error (works with gcc).
     */
    switch (ret) {
    SXE_RETURN_CASE(OK);
    SXE_RETURN_CASE(EXPIRED_VALUE);
    SXE_RETURN_CASE(NO_UNUSED_ELEMENTS);
    SXE_RETURN_CASE(IN_PROGRESS);
    SXE_RETURN_CASE(UNCATEGORIZED);
    SXE_RETURN_CASE(END_OF_FILE);
    SXE_RETURN_CASE(WARN_ALREADY_INITIALIZED);
    SXE_RETURN_CASE(WARN_WOULD_BLOCK);
    SXE_RETURN_CASE(WARN_ALREADY_CLOSED);
    SXE_RETURN_CASE(ERROR_NOT_INITIALIZED);
    SXE_RETURN_CASE(ERROR_ALLOC);
    SXE_RETURN_CASE(ERROR_INTERNAL);
    SXE_RETURN_CASE(ERROR_NO_CONNECTION);
    SXE_RETURN_CASE(ERROR_ALREADY_CONNECTED);
    SXE_RETURN_CASE(ERROR_INVALID_URI);
    SXE_RETURN_CASE(ERROR_BAD_MESSAGE);
    SXE_RETURN_CASE(ERROR_ADDRESS_IN_USE);
    SXE_RETURN_CASE(ERROR_INTERRUPTED);
    SXE_RETURN_CASE(ERROR_COMMAND_NOT_RUN);
    SXE_RETURN_CASE(ERROR_LOCK_NOT_TAKEN);
    SXE_RETURN_CASE(ERROR_INCORRECT_STATE);
    SXE_RETURN_CASE(ERROR_TIMED_OUT);
    SXE_RETURN_CASE(ERROR_WRITE_FAILED);
    SXE_RETURN_CASE(INVALID_VALUE);            /* Required to make compiler happy */
    }

    return NULL;
}

static bool
sxe_log_safe_append(char * log_buffer, unsigned * index_ptr, int appended)
{
    /* If output was truncated, Win32 returns negative result while Linux returns positive result!
     */
    if ((appended < 0) || ((unsigned)appended >= SXE_LOG_BUFFER_SIZE - *index_ptr))
    {
        log_buffer[SXE_LOG_BUFFER_SIZE - 4] = '.';
        log_buffer[SXE_LOG_BUFFER_SIZE - 3] = '.';
        log_buffer[SXE_LOG_BUFFER_SIZE - 2] = '\n';
        log_buffer[SXE_LOG_BUFFER_SIZE - 1] = '\0';
        return false;
    }

    *index_ptr += appended;
    return true;
}

static void
sxe_log_line_out_default(SXE_LOG_LEVEL level, const char * line) /* Coverage Exclusion - default version for testing */
{                                                             /* Coverage Exclusion - default version for testing */
    SXE_UNUSED_PARAMETER(level);

    fputs(line, stderr);                                      /* Coverage Exclusion - default version for testing */
    fflush(stderr);                                           /* Coverage Exclusion - default version for testing */
}

/* Log level indicators, corresponding to the SXE_LOG_LEVEL enum
 */
static const char *level_to_str[] = { "???", "FAT", "ERR", "WAR", "INF", "DEB", "TRA", "DUM" };

static unsigned
sxe_log_prefix_default(char * log_buffer, unsigned id, SXE_LOG_LEVEL level)
{
    size_t           length;
    int              ret;
#if defined(WIN32)
    SYSTEMTIME       st;
    DWORD            ThreadId;
#else
    struct timeval   mytv;
    struct tm        mytm;
#   if defined(__APPLE__)
    pid_t            ThreadId;
    pid_t            ProcessId;
#   elif defined(__FreeBSD__)
    long             ThreadId;
    pid_t            ProcessId;
#else
    pid_t            ThreadId;
#endif
#endif

#if defined(WIN32)
    GetSystemTime(&st);
    ThreadId  = GetCurrentThreadId();

    ret = snprintf(log_buffer, SXE_LOG_BUFFER_SIZE, "%04d%02d%02d %02d%02d%02d.%03d T%08x ", st.wYear, st.wMonth, st.wDay,
                   st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, ThreadId);
#else
    gettimeofday(&mytv, NULL);
    gmtime_r((time_t *)&mytv.tv_sec, &mytm);
#   if defined(__APPLE__)
    ThreadId = syscall(SYS_thread_selfid);
    ProcessId = getpid();
    ret = snprintf(log_buffer, SXE_LOG_BUFFER_SIZE, "%04d%02d%02d %02d%02d%02d.%03ld P% 10d T% 10d ", mytm.tm_year + 1900,
                   mytm. tm_mon + 1, mytm.tm_mday, mytm.tm_hour, mytm.tm_min, mytm.tm_sec, (long)mytv.tv_usec / 1000,
                   ProcessId, ThreadId);
#   elif defined(__FreeBSD__)
    thr_self(&ThreadId);
    ProcessId = getpid();
    ret = snprintf(log_buffer, SXE_LOG_BUFFER_SIZE, "%04d%02d%02d %02d%02d%02d.%03ld P% 10d T% 10ld ", mytm.tm_year + 1900,
                   mytm. tm_mon + 1, mytm.tm_mday, mytm.tm_hour, mytm.tm_min, mytm.tm_sec, (long)mytv.tv_usec / 1000,
                   ProcessId, ThreadId);
#else
    ThreadId = syscall(SYS_gettid);
    ret = snprintf(log_buffer, SXE_LOG_BUFFER_SIZE, "%04d%02d%02d %02d%02d%02d.%03ld T% 10d ", mytm.tm_year + 1900,
                   mytm. tm_mon + 1, mytm.tm_mday, mytm.tm_hour, mytm.tm_min, mytm.tm_sec, mytv.tv_usec / 1000, ThreadId);
#endif /* !__APPLE__ */
#endif

    SXEA6(ret >= 0 && ret < SXE_LOG_BUFFER_SIZE, "Error or timestamp and thread id overflowed the log line buffer");

    if (id == SXE_LOG_NO_ID || id == (~0U - 1)) {
        if (sxe_log_options & SXE_LOG_OPTION_ID_HEX) {
            length = sxe_strlcpy(&log_buffer[ret], "-------- ", SXE_LOG_BUFFER_SIZE - ret);
            SXEA6(length == sizeof("-------- ") - 1, "Id overflowed the log line buffer");
        }
        else {
            length = sxe_strlcpy(&log_buffer[ret], "------ ", SXE_LOG_BUFFER_SIZE - ret);
            SXEA6(length == sizeof("------ ") - 1, "Id overflowed the log line buffer");
        }
    }
    else {
        length = ret;

        if (sxe_log_options & SXE_LOG_OPTION_ID_HEX)
            ret = snprintf(&log_buffer[length], SXE_LOG_BUFFER_SIZE - length, "%08x ", id);
        else
            ret = snprintf(&log_buffer[length], SXE_LOG_BUFFER_SIZE - length, "%6u ", id);

        SXEA6(ret >= 0 && length + ret < SXE_LOG_BUFFER_SIZE, "Error or id overflowed the log line buffer");
    }

    length += ret;

    if (sxe_log_options & SXE_LOG_OPTION_LEVEL_TEXT) {
        ret = snprintf(&log_buffer[length], SXE_LOG_BUFFER_SIZE - length, "%s ",
                       level_to_str[level < SXE_LOG_LEVEL_OVER_MAXIMUM ? level : 0]);
    }
    else {
        ret = snprintf(&log_buffer[length], SXE_LOG_BUFFER_SIZE - length, "%c ",  '0' + level);
    }

    SXEA6(ret >= 0 && length + ret < SXE_LOG_BUFFER_SIZE, "Error or level overflowed the log line buffer");
    return length + ret;
}

static SXE_LOG_LINE_OUT_PTR      sxe_log_line_out      = &sxe_log_line_out_default;
static SXE_LOG_BUFFER_PREFIX_PTR sxe_log_buffer_prefix = &sxe_log_prefix_default;

/**
 * Override the default logging (to stderr) function
 *
 * @param line_out Pointer to a function that takes two arguments, a log level and a pointer to a log line
 *
 * @return Pointer to the previous logging function
 */
SXE_LOG_LINE_OUT_PTR
sxe_log_hook_line_out(void (*line_out)(SXE_LOG_LEVEL level, const char * line))
{
    SXE_LOG_LINE_OUT_PTR previous_line_out = sxe_log_line_out;

    if (line_out == NULL) {
        sxe_log_line_out = sxe_log_line_out_default;
    }
    else {
        sxe_log_line_out = line_out;
    }

    return previous_line_out;
}

/**
 * Override the default buffer prefix function
 *
 * @param line_out = Pointer to a function that takes three arguments, a log buffer, an object id, and a log level and returns
 *                   the number of bytes saved in the buffer
 *
 * @return Pointer to the previous buffer prefix function
 */
SXE_LOG_BUFFER_PREFIX_PTR
sxe_log_hook_buffer_prefix(unsigned (*buffer_prefix)(char * log_buffer, unsigned id, SXE_LOG_LEVEL level))
{
    SXE_LOG_BUFFER_PREFIX_PTR previous_buffer_prefix = sxe_log_buffer_prefix;

    if (buffer_prefix == NULL) {
        sxe_log_buffer_prefix = sxe_log_prefix_default;
    }
    else {
        sxe_log_buffer_prefix = buffer_prefix;
    }

    return previous_buffer_prefix;
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
                to += snprintf(&line_escaped[to], 5, "\\x%02X", (unsigned)(unsigned char)line[from]);
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

/* Set all control levels back to OVER_MAXIMUM (i.e. unknown); do this after any change to log levels
 */
void
sxe_log_control_forget_all_levels(void)
{
    volatile SXE_LOG_CONTROL * control = sxe_log_control_list;

    if (control == NULL) {
        return;
    }

    for (;;) {
        if (!(control->next != NULL)) {
            sxe_log_assert(NULL, __FILE__, ~0U, __LINE__, "control->next != NULL",    /* Coverage Exclusion: Can't happen */
                           "sxe_log_control_forget_all_levels(): Log control object in list is not fully initialized");
        }

        SXE_LOG_LEVEL_VALIDATE(control->level, SXE_LOG_LEVEL_OVER_MAXIMUM);
        control->level = SXE_LOG_LEVEL_OVER_MAXIMUM;

        if (control == control->next) {
            break;
        }

        control = control->next;
    }
}

/**
 * Set the global log level
 *
 * @param level = Level to set (e.g. SXE_LOG_LEVEL_INFO)
 *
 * @return Previous global log level or SXE_LOG_LEVEL_OVER_MAXIMUM if log level has not been set/read from environment
 */

SXE_LOG_LEVEL
sxe_log_set_level(SXE_LOG_LEVEL level)
{
    SXE_LOG_LEVEL level_previous = sxe_log_level;

    SXE_LOG_LEVEL_VALIDATE(level_previous, SXE_LOG_LEVEL_OVER_MAXIMUM);
    SXE_LOG_LEVEL_VALIDATE(level,          SXE_LOG_LEVEL_OVER_MAXIMUM);
    sxe_log_level = level;
    sxe_log_setting_era++;
    sxe_log_control_forget_all_levels();
    return level_previous;
}

/**
 * Get the global log level
 *
 * @return Global log level or SXE_LOG_LEVEL_OVER_MAXIMUM if log level has not been set/read from environment
 */

SXE_LOG_LEVEL
sxe_log_get_level(void)
{
    return sxe_log_level;
}

/**
 * Decrease the global log level
 *
 * @param level = Level to decrease to (e.g. SXE_LOG_LEVEL_WARN); If log level is already less than this value, no action is taken
 *
 * @return Previous global log level or SXE_LOG_LEVEL_OVER_MAXIMUM if log level has not been set/read from environment
 *
 * @note To restore the log level, set it to the value returned by this function.
 */
SXE_LOG_LEVEL
sxe_log_decrease_level(SXE_LOG_LEVEL level)
{
    SXE_LOG_LEVEL level_previous = sxe_log_level;

    SXE_LOG_LEVEL_VALIDATE(level_previous, SXE_LOG_LEVEL_OVER_MAXIMUM);
    SXE_LOG_LEVEL_VALIDATE(level,          SXE_LOG_LEVEL_MAXIMUM);

    if (level < level_previous) {
        sxe_log_level = level;
        sxe_log_control_forget_all_levels();
    }

    return level_previous;
}

/* TODO: Add a global function that allows programmatic setting of fine grained levels */

static SXE_RETURN
sxe_log_level_getenv(SXE_LOG_LEVEL * out_level, const char * variable_name)
{
    SXE_RETURN    result = SXE_RETURN_ERROR_INTERNAL;
    SXE_LOG_LEVEL level;
    const char  * level_string;

    if ((level_string = getenv(variable_name)) == NULL) {
        goto SXE_ERROR_OUT;
    }

    level = atoi(level_string);

    if (level < SXE_LOG_LEVEL_MINIMUM || level > SXE_LOG_LEVEL_MAXIMUM) {
        /* TODO: Should log an error about the bad level value here */
        goto SXE_ERROR_OUT;             /* COVERAGE EXCLUSION: todo: Nothing tests setting bad SXE_LOG_LEVEL values */
    }

    *out_level = level;
    result     = SXE_RETURN_OK;

SXE_ERROR_OUT:
    return result;
}

#ifdef _WIN32
static void
sxe_log_suppress_crash_handlers(void)
{
    _set_error_mode(_OUT_TO_STDERR); /* pre-msvcrt80 mechanism */

#ifdef MAKE_MINGW
    // mingw doesn't support msvcrt80?
#else
    // When the app crashes, don't print the abort message and don't call Dr. Watson to make a crash dump
    _set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
#endif
}

static bool sxe_log_crash_handlers_supression_initialized = false;
#endif

/* Determine the log level based on the source file name. Levels can be controlled via environment variables.
 */
static SXE_LOG_LEVEL
sxe_log_control_learn_level(volatile SXE_LOG_CONTROL * control)
{
    SXE_LOG_LEVEL              level;
    volatile SXE_LOG_CONTROL * control_head;
    char                       variable_name[PATH_MAX];
    unsigned                   i;
    char                     * end_of_component = NULL;
    char                     * end_of_package   = NULL;

    /* If this is the first time this control structure has been accessed, link it into the list
     */
    if (control->next == NULL) {
        /* Keep trying to make our control structure the first in the list until we don't race with another thread.
         */
        do {
            control_head = sxe_log_control_list;
        } while (__sync_val_compare_and_swap(&sxe_log_control_list, control_head, control) != control_head);

        control->next = control_head == NULL ? control : control_head;

#ifdef _WIN32
        /* This is a great place to do a one-time disabling of the crash message-box on Windows
         */
        if (!sxe_log_crash_handlers_supression_initialized) {
            sxe_log_crash_handlers_supression_initialized = true;

            if (getenv("SXE_WINNT_ASSERT_MSGBOX") == NULL) {
                sxe_log_suppress_crash_handlers();
            }
        }
#endif
    }

    if ((level = control->level) <= SXE_LOG_LEVEL_MAXIMUM) {            /* Has the control's level already been learned? */
        return level;
    }

    strncpy(variable_name, "SXE_LOG_LEVEL_", sizeof(variable_name) - 1);

    /* Construct an environment variable name from the component/package/file name
     *  - if SXE_FILE is supplied by the make:     e.g.   libsxe/lib-sxe/sxe.c
     *  - if not, __FILE__ is used:                e.g.                  sxe.c
     */
    for (i = 0; sizeof("SXE_LOG_LEVEL_") + i < sizeof(variable_name); i++) {
        if (control->file[i] == '-' || control->file[i] == '/') {
            variable_name[sizeof("SXE_LOG_LEVEL_") - 1 + i] = '_';

            if (control->file[i] == '/') {
                if (end_of_component == NULL) {
                    end_of_component = &variable_name[sizeof("SXE_LOG_LEVEL_") - 1 + i];
                }
                else if (end_of_package == NULL) {
                    end_of_package = &variable_name[sizeof("SXE_LOG_LEVEL_") - 1 + i];
                }
            }
        }
        else if (control->file[i] == '.' || control->file[i] == '\0') {
            variable_name[sizeof("SXE_LOG_LEVEL_") - 1 + i] = '\0';
            break;
        }
        else {
            variable_name[sizeof("SXE_LOG_LEVEL_") - 1 + i] = toupper(control->file[i]);
        }
    }

    /* TODO: Print a warning if end_of_component or end_of_package is not set */

    variable_name[sizeof(variable_name) - 1] = '\0';

    if (control->file[i] != '\0' && sxe_log_level_getenv(&level, variable_name) == SXE_RETURN_OK) {    /* Has a file specific level been set? */
        goto SXE_EARLY_OUT; /* COVERAGE EXCLUSION: TODO: add coverage for this line */
    }

    if (end_of_package != NULL) {
        *end_of_package = '\0';

        if (sxe_log_level_getenv(&level, variable_name) == SXE_RETURN_OK) {         /* Has a package specific level been set? */
            goto SXE_EARLY_OUT;
        }
    }

    if (end_of_component != NULL) {
        *end_of_component = '\0';

        if (sxe_log_level_getenv(&level, variable_name) == SXE_RETURN_OK) {       /* Has a component specific level been set? */
            goto SXE_EARLY_OUT;
        }
    }

    /* TODO: Check more specific level settings */

    if ((level = sxe_log_level) <= SXE_LOG_LEVEL_MAXIMUM) {              /* Has a global level has been set programmatically? */
        goto SXE_EARLY_OUT;
    }

    if (sxe_log_level_getenv(&level, "SXE_LOG_LEVEL") == SXE_RETURN_OK) {    /* Has a global level has been set in the environment? */
        sxe_log_level = level;
        goto SXE_EARLY_OUT;
    }

    level = SXE_LOG_LEVEL_DEFAULT;                                                        /* Otherwise, use the default level.*/

SXE_EARLY_OUT:
    control->level = level;
    return level;
}

static unsigned
sxe_log_get_indent(volatile SXE_LOG_CONTROL * control, unsigned id, int line)
{
    SXE_LOG_STACK * stack = sxe_log_get_stack(control->file, id, line);
    unsigned        depth = 0;
    SXE_LOG_FRAME * frame;

    SXE_UNUSED_PARAMETER(id);
    SXE_UNUSED_PARAMETER(line);

#if SXE_DEBUG
    if (stack->era > sxe_log_setting_era) {
        sxe_log_assert(NULL, control->file, id, line, "stack->era <= sxe_log_setting_era",
                       "Previous stack era %u exceeds current setting era %u", stack->era, sxe_log_setting_era);
    }
#endif

    while (stack->era < sxe_log_setting_era) {
        stack->era = sxe_log_setting_era;

        for (frame = stack->top; frame != &stack->bottom; frame = frame->caller) {
            if (frame->level <= sxe_log_control_learn_level(control)) {
                depth++;
            }
        }

#if SXE_DEBUG
        if (depth > sxe_log_get_indent_maximum()) {
            sxe_log_set_indent_maximum(depth);
        }
#endif

        for (frame = stack->top; frame != &stack->bottom; frame = frame->caller) {
            frame->indent = depth;

            if (frame->level <= sxe_log_control_learn_level(control)) {
                depth--;
            }
        }
    }

#if SXE_DEBUG
    if (stack->top->indent > sxe_log_get_indent_maximum()) {
        sxe_log_assert(NULL, control->file, id, line, "stack->top->indent <= sxe_log_indent_maximum",
                       "Log indent %u exceeds maximum log indent %u", stack->top->indent, sxe_log_get_indent_maximum());
    }
#endif

    return stack->top->indent;
}

/**
 * Output a line to the log. Normally, this is called via one of the SXEL#[I] wrapper macros in sxe-log.h
 *
 * @param control Per source file control block. These are automatically created by the sxe-log.h file for each source file.
 * @param func    Function name. These are passed by the macros using the __func__ builtin.
 * @param id      Transaction identifier or SXE_LOG_NO_ID to use the per thread id. If both are SXE_LOG_NO_ID, uses '------'.
 * @param line    Line number. These are passed by the macros using the __LINE__ builtin.
 * @param level   SXE_LOG_LEVEL value. One of ERROR, WARNING, INFORMATION, DEBUG, TRACE, or DUMP.
 * @param format  Printf-like format string followed by optional arguments
 *
 * @note In previous versions, file was passed instead of func. It is now required to be set in the source file control object.
 */
__printflike(6, 7) void
sxe_log(volatile SXE_LOG_CONTROL * control, const char * func, unsigned id, int line, SXE_LOG_LEVEL level, const char * format, ...)
{
    char            log_buffer[SXE_LOG_BUFFER_SIZE];
    va_list         ap;
    unsigned        i;

    if (level > sxe_log_control_learn_level(control)) {
        return;
    }

    if (id == SXE_LOG_NO_ID)    // If no id specified, use the per thread value
        id = sxe_log_transaction_id;

    i = (*sxe_log_buffer_prefix)(log_buffer, id, level);
    va_start(ap, format);

    if (sxe_log_safe_append(log_buffer, &i,  snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "%*s%s",
                            2 * sxe_log_get_indent(control, id, line), "", (id == ~0U - 1) ? "" : "- "))
     && ((format[0] != ':' && format[0] != '(')
      || sxe_log_safe_append(log_buffer, &i, sxe_strlcpy(&log_buffer[i], func, SXE_LOG_BUFFER_SIZE - i)))
     && sxe_log_safe_append(log_buffer, &i, vsnprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, format, ap)))
    {
        sxe_log_safe_append( log_buffer, &i,  snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "\n"));
    }

    va_end(ap);
    sxe_log_line_out_escaped(level, log_buffer);
}

__printflike(5, 6) void
sxe_log_entry(SXE_LOG_FRAME * frame, volatile SXE_LOG_CONTROL * control, unsigned id,  SXE_LOG_LEVEL level,
              const char * format, ...)
{
    char     log_buffer[SXE_LOG_BUFFER_SIZE];
    va_list  ap;
    unsigned i;
    unsigned prefix_length;

    if (level > sxe_log_control_learn_level(control)) {
        sxe_log_frame_push(frame, false);
        return;
    }

    if (id == SXE_LOG_NO_ID)    // If no id specified, use the per thread value
        id = sxe_log_transaction_id;

    sxe_log_frame_push(frame, true);
    prefix_length   = (*sxe_log_buffer_prefix)(log_buffer, id, level);
    i               = prefix_length;
    va_start(ap, format);

    if (sxe_log_safe_append(log_buffer, &i, snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "%*s+ ",
                                                      2 * sxe_log_get_indent(control, id, frame->line) - 2, ""))
     && (format[0] != '(' || sxe_log_safe_append(log_buffer, &i, sxe_strlcpy(&log_buffer[i], frame->function, SXE_LOG_BUFFER_SIZE - i)))
     && sxe_log_safe_append(log_buffer, &i, vsnprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, format, ap)))
    {
        sxe_log_safe_append(log_buffer, &i, snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "\n"));
    }

    va_end(ap);
    sxe_log_line_out_escaped(level, log_buffer);
    i = prefix_length;
    sxe_log_safe_append(log_buffer, &i,  snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "%*s  { // %s:%d\n",
                        2 * sxe_log_get_indent(control, id, frame->line) - 2, "", frame->file, frame->line));
    sxe_log_line_out_escaped(level, log_buffer);
}

void
sxe_log_return(volatile SXE_LOG_CONTROL * control, SXE_LOG_FRAME * frame, SXE_LOG_LEVEL level)
{
    char     log_buffer[SXE_LOG_BUFFER_SIZE];
    unsigned id;
    unsigned i;

    sxe_log_frame_pop(frame);

    if (level > sxe_log_control_learn_level(control)) {
        return;
    }

    id = frame->id == SXE_LOG_NO_ID ? sxe_log_transaction_id : frame->id;   // If no id specified, use the per thread value
    i  = (*sxe_log_buffer_prefix)(log_buffer, id, level);
    sxe_log_safe_append(log_buffer, &i, snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "%*s} // %s:%d\n",
                        2 * sxe_log_get_indent(control, id, frame->line) + 2, "", frame->file,
                        frame->line));
    sxe_log_line_out_escaped(level, log_buffer);
}

/**
 * Output a fatal error and abort. This is normally called via the SXEA#[I] macros defined in sxe-log.h
 *
 * @param control Per source file control object (not used)
 * @param file    Full file name. __FILE__ is passed by the macros.
 * @param id      Transaction identifier or SXE_LOG_NO_ID to use the per thread id. If both are SXE_LOG_NO_ID, uses '------'.
 * @param line    Line number. These are passed by the macros using the __LINE__ builtin.
 * @param con     Text of the conditional expression that failed
 * @param format  Printf-like format string followed by optional arguments
 */
__printflike(6, 7) __noreturn void
sxe_log_assert(volatile SXE_LOG_CONTROL * control, const char * file, unsigned id, int line, const char * con, const char * format, ...)
{
    char            log_buffer[SXE_LOG_BUFFER_SIZE_ASSERT];
    va_list         ap;
    unsigned        length = 0;
    const char    * where  = "in";
    SXE_LOG_STACK * stack;
    SXE_LOG_FRAME * frame;

    SXE_UNUSED_PARAMETER(control);

    (*sxe_log_assert_cb)();

    if (id == SXE_LOG_NO_ID)    // If no id specified, use the per thread value
        id = sxe_log_transaction_id;

    length = (*sxe_log_buffer_prefix)(log_buffer, id, 1);
    va_start(ap, format);

    if (sxe_log_safe_append(log_buffer, &length,  snprintf(&log_buffer[length], SXE_LOG_BUFFER_SIZE - length,
        "ERROR: assertion '%s' failed at %s:%d; ", con, file, line))
    &&  sxe_log_safe_append(log_buffer, &length, vsnprintf(&log_buffer[length], SXE_LOG_BUFFER_SIZE - length, format, ap))) {
        sxe_log_safe_append(log_buffer, &length,  snprintf(&log_buffer[length], SXE_LOG_BUFFER_SIZE - length, "\n"));
    }

    va_end(ap);

SXE_EARLY_OR_ERROR_OUT:

    sxe_log_line_out_escaped(SXE_LOG_LEVEL_FATAL, log_buffer);
    stack = sxe_log_get_stack(file, id, line);

    /* Stack traceback
     */
    for (frame = stack->top; frame && frame != &stack->bottom; frame = frame->caller) {
        length = 0;
        sxe_log_safe_append(log_buffer, &length, snprintf(&log_buffer[length], SXE_LOG_BUFFER_SIZE - length,
                            "       %s function %s() at %s:%d\n", where, frame->function, frame->file, frame->line));
        sxe_log_line_out_escaped(SXE_LOG_LEVEL_FATAL, log_buffer);
        where = "called from";
    }

#ifdef _WIN32
    if (getenv("SXE_WINNT_ASSERT_MSGBOX") == NULL) {
        length = 0;
        sxe_log_safe_append(log_buffer, &length, snprintf(&log_buffer[length], SXE_LOG_BUFFER_SIZE - length,
            "NOTE: set SXE_WINNT_ASSERT_MSGBOX=1 to assert into the Microsoft visual debugger\n"));
        sxe_log_line_out_escaped(SXE_LOG_LEVEL_FATAL, log_buffer);
    }
#endif

    abort();
}

void
sxe_log_dump_memory(volatile SXE_LOG_CONTROL * control, const char * func, unsigned id, int line, SXE_LOG_LEVEL level,
                    const void * pointer, unsigned length)
{
    char                  log_buffer[SXE_LOG_BUFFER_SIZE];
    unsigned              prefix_length;
    const unsigned char * memory;
    unsigned              c;
    unsigned              i;
    unsigned              j;
    unsigned              k;

    SXE_UNUSED_PARAMETER(func);

    if (level > sxe_log_control_learn_level(control)) {
        return;
    }

    prefix_length = (*sxe_log_buffer_prefix)(log_buffer, id, level);
    memory        = (const unsigned char *)pointer;

    /* TODO: pretty up dumping :-) */
    /* - 0x12345678                   56 78 12 34 56 78 12 34 56 78       .......... */
    /* - 0x12345678 12 34 56 78 12 34 56 78 12 34 56 78 12 34 56 78 ................ */
    /* - 0x12345678 12 34 56 78                                     ....             */

    for (k = 0; k <= length / 16; k++)
    {
        i = prefix_length;

        if (!sxe_log_safe_append( log_buffer, &i, snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "%*s- ",
                                  2 * sxe_log_get_indent(control, id, line), ""))
         || !sxe_log_safe_append( log_buffer, &i, snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i,
                                  sizeof(unsigned long) > 4 ? "%016lx" : "%08lx",
                                  (unsigned long)(memory + (k * 16)))))
        {
            goto SXE_EARLY_OUT;             /* COVERAGE EXCLUSION: Line truncation in sxe_log_dump_memory function */
        }

        /* print 16 bytes unless we get to the end */
        for(j = 0; j < 16; j++) {
            if (((k * 16) + j) < length)
            {
                if (!sxe_log_safe_append( log_buffer, &i, snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, " %02x", memory[(k * 16) + j]))) {
                    goto SXE_EARLY_OUT;     /* COVERAGE EXCLUSION: Line truncation in sxe_log_dump_memory function */
                }
            }
            else
            {
                if (!sxe_log_safe_append( log_buffer, &i, snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "   "))) {
                    goto SXE_EARLY_OUT;     /* COVERAGE EXCLUSION: Line truncation in sxe_log_dump_memory function */
                }
            }
        }

        if (!sxe_log_safe_append( log_buffer, &i, snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, " "))) {
            goto SXE_EARLY_OUT;             /* COVERAGE EXCLUSION: Line truncation in sxe_log_dump_memory function */
        }

        for (j = 0; (j < 16) && (((k * 16) + j) < length); j++) {
            c = memory[(k * 16) + j];
            c = c < 0x20 ? '.' : c;
            c = c > 0x7E ? '.' : c;

            if (!sxe_log_safe_append( log_buffer, &i, snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "%c", c))) {
                goto SXE_EARLY_OUT;         /* COVERAGE EXCLUSION: Line truncation in sxe_log_dump_memory function */
            }
        }

        sxe_log_safe_append( log_buffer, &i, snprintf(&log_buffer[i], SXE_LOG_BUFFER_SIZE - i, "\n"));

SXE_EARLY_OR_ERROR_OUT:
        sxe_log_line_out_escaped(level, log_buffer);
    } /* for (k = ... */
} /* sxe_debug_dump_memory() */

#ifndef _WIN32
static void
sxe_log_line_to_syslog(SXE_LOG_LEVEL level, const char *line)
{
    int syslog_level;

    switch (level) {
    case SXE_LOG_LEVEL_FATAL:       syslog_level = LOG_ERR;     break;
    case SXE_LOG_LEVEL_ERROR:       syslog_level = LOG_WARNING; break;
    case SXE_LOG_LEVEL_WARNING:     syslog_level = LOG_NOTICE;  break;
    case SXE_LOG_LEVEL_INFORMATION: syslog_level = LOG_INFO;    break;
    default:                        syslog_level = LOG_DEBUG;   break;
    }

    syslog(syslog_level, "%s", line);
}

static unsigned
sxe_log_buffer_prefix_syslog(char * log_buffer, unsigned id, SXE_LOG_LEVEL level)
{
    unsigned length;
#if defined(__APPLE__)
    pid_t    ThreadId = syscall(SYS_thread_selfid);

    snprintf(log_buffer, SXE_LOG_BUFFER_SIZE, "T=%d ", ThreadId);
#elif defined(__FreeBSD__)
    long     ThreadId;
    thr_self(&ThreadId);

    snprintf(log_buffer, SXE_LOG_BUFFER_SIZE, "T=%ld ", ThreadId);
#else
    pid_t ThreadId = syscall(SYS_gettid);

    snprintf(log_buffer, SXE_LOG_BUFFER_SIZE, "T=%d ", ThreadId);
#endif

    if (id == ~0U || id == (~0U - 1)) {
        strcat(log_buffer, "------ ");
    }
    else {
        snprintf(&log_buffer[strlen(log_buffer)], SXE_LOG_BUFFER_SIZE, "%6u ", id);
    }

    length = strlen(log_buffer);
    log_buffer[length++]    = '0' + level;
    log_buffer[length++]    = ' ';
    log_buffer[length]      = '\0';
    return length;
}

void
sxe_log_use_syslog(const char *ident, int option, int facility)
{
    openlog(ident, option, facility);
    sxe_log_hook_buffer_prefix(sxe_log_buffer_prefix_syslog);
    sxe_log_hook_line_out(sxe_log_line_to_syslog);
}
#endif

/* TODO: refactor to form: sxe_debug_map_enum_to_string_<enum type> */
