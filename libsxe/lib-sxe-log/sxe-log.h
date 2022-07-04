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

/* TODO: Have exit automatically use the same level as entry, or error on missmatch         */
/* TODO: Auto add "return " on exit calls if not already there?                             */

#ifndef __SXE_LOG_H__
#define __SXE_LOG_H__

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>    /* For __thread on Windows */

#ifdef __APPLE__
#   include <pthread.h>          /* pthread_getspecific()                                   */
#endif

#ifndef SXE_DEBUG
#   define SXE_DEBUG 0
#endif

#define SXE_LOG_BUFFER_SIZE 1024    // Maximum size of a log line
#define SXE_LOG_NO_ID       ~0U     // Value indicating no identifier for sxe_log_set_id

/*
 * Compiler-dependent macros to declare that functions take printf-like
 * or scanf-like arguments.  They are null except for versions of gcc
 * that are known to support the features properly (old versions of gcc-2
 * didn't permit keeping the keywords out of the application namespace).
 */
#if !defined(__GNUC__) || __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#   define __printflike(fmtarg, firstvararg)
#   define __noreturn
#else
#   define __printflike(fmtarg, firstvararg) __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#   define __noreturn                        __attribute__((__noreturn__))
#endif

/* Options that can be set with sxe_log_set_options
 */
#define SXE_LOG_OPTION_LEVEL_TEXT 0x00000001    // Three letter log levels instead of numbers
#define SXE_LOG_OPTION_ID_HEX     0x00000002    // Display ids in hex instead of decimal

/* The following log levels are supported
 */
typedef enum SXE_LOG_LEVEL {
    SXE_LOG_LEVEL_UNDER_MINIMUM,  /* Guard value: do not use this                            */
    SXE_LOG_LEVEL_FATAL,          /* Release mode assertions                                 */
    SXE_LOG_LEVEL_ERROR,
    SXE_LOG_LEVEL_WARNING,
    SXE_LOG_LEVEL_INFORMATION,    /* e.g. SXLd transactions                                  */
    SXE_LOG_LEVEL_DEBUG,          /* e.g. External function entry/return                     */
/*  -------------------------------- only compiled into debug build below this line -------- */
    SXE_LOG_LEVEL_TRACE,          /* e.g. Internal function entry/return and further logging */
    SXE_LOG_LEVEL_DUMP,           /* e.g. Packet/structure dumps, periodic idle behaviour    */
    SXE_LOG_LEVEL_OVER_MAXIMUM    /* Guard value: do not use this                            */
} SXE_LOG_LEVEL;

/* The following macros and types are here because sxe-log.h is the lowest level package */

#define SXE_UNUSED_PARAMETER(param) (void)(param)
#define SXE_EARLY_OR_ERROR_OUT goto SXE_EARLY_OUT; SXE_EARLY_OUT: goto SXE_ERROR_OUT; SXE_ERROR_OUT

/* For those who like their bools to be SXE
 */
typedef bool      SXE_BOOL;
#define SXE_FALSE false
#define SXE_TRUE  true

typedef enum SXE_RETURN {
    SXE_RETURN_OK = 0,
    SXE_RETURN_EXPIRED_VALUE,
    SXE_RETURN_NO_UNUSED_ELEMENTS,
    SXE_RETURN_IN_PROGRESS,
    SXE_RETURN_UNCATEGORIZED,
    SXE_RETURN_END_OF_FILE,
    SXE_RETURN_WARN_ALREADY_INITIALIZED,
    SXE_RETURN_WARN_WOULD_BLOCK,
    SXE_RETURN_WARN_ALREADY_CLOSED,
    SXE_RETURN_ERROR_NOT_INITIALIZED,
    SXE_RETURN_ERROR_ALLOC,
    SXE_RETURN_ERROR_INTERNAL,
    SXE_RETURN_ERROR_NO_CONNECTION,
    SXE_RETURN_ERROR_ALREADY_CONNECTED,
    SXE_RETURN_ERROR_INVALID_URI,
    SXE_RETURN_ERROR_BAD_MESSAGE,
    SXE_RETURN_ERROR_ADDRESS_IN_USE,
    SXE_RETURN_ERROR_INTERRUPTED,
    SXE_RETURN_ERROR_COMMAND_NOT_RUN,
    SXE_RETURN_ERROR_LOCK_NOT_TAKEN,
    SXE_RETURN_ERROR_INCORRECT_STATE,
    SXE_RETURN_ERROR_TIMED_OUT,
    SXE_RETURN_ERROR_WRITE_FAILED,
    SXE_RETURN_INVALID_VALUE    /* This must be the last value. */
} SXE_RETURN;

/* Aliases for backward compatibility
 */
#define SXE_RETURN_WARN_CACHE_DOUBLE_INITIALIZED SXE_RETURN_WARN_ALREADY_INITIALIZED
#define SXE_RETURN_ERROR_BAD_MESSAGE_RECEIVED    SXE_RETURN_ERROR_BAD_MESSAGE
#define SXE_RETURN_ERROR_CACHE_UNINITIALIZED     SXE_RETURN_ERROR_NOT_INITIALIZED

/* Private type used to allow per file dynamic modification of log levels
 */
typedef struct SXE_LOG_CONTROL {
    SXE_LOG_LEVEL                     level;
    volatile struct SXE_LOG_CONTROL * next;
    const char *                      file;
} SXE_LOG_CONTROL;

/* Private type used to declare space for the log package on the stack of each function that calls SXEE##
 */
typedef struct SXE_LOG_FRAME {
    struct SXE_LOG_FRAME * caller;
    unsigned               indent;
    SXE_LOG_LEVEL          level;
    const char *           file;
    unsigned               line;
    const char *           function;
    unsigned               id;         /* SXE identifier */
} SXE_LOG_FRAME;

/* Per thread stack.
 */
typedef struct SXE_LOG_STACK {
    SXE_LOG_FRAME   bottom;
    SXE_LOG_FRAME * top;
    unsigned        era;
} SXE_LOG_STACK;

/* Types of pointers to functions which can be hooked
 */
typedef void     (*SXE_LOG_LINE_OUT_PTR)(     SXE_LOG_LEVEL level, const char *);                      /* Log line out function  */
typedef unsigned (*SXE_LOG_BUFFER_PREFIX_PTR)(char * log_buffer, unsigned id, SXE_LOG_LEVEL level);    /* Buffer prefix function */

/* Globals used by macros/inlines
 */
#ifdef __APPLE__
extern pthread_key_t          sxe_log_stack_key;             /* Top of stack;    initialized to NULL in sxe-log.c  */
#else
extern __thread SXE_LOG_STACK sxe_log_stack;                 /* Top of stack;    initialized to NULL in sxe-log.c  */
#endif

#if SXE_DEBUG
#ifdef __APPLE__
extern pthread_key_t          sxe_log_indent_maximum_key;    /* Maximum indent;  initialized to 0    in sxe-log.c  */
#else
extern __thread unsigned      sxe_log_indent_maximum;        /* Maximum indent;  initialized to 0    in sxe-log.c  */
#endif
#endif

typedef void (*SXE_LOG_ASSERT_CB)(void);

extern SXE_LOG_ASSERT_CB sxe_log_assert_cb; /* NULL or function to call before abort() in sxe_log_assert() */

#define SXE_LOG_ASSERT_SET_CALLBACK(cb) sxe_log_assert_cb = cb

#ifndef SXE_FILE
#   define SXE_FILE __FILE__    /* Normally, the make system defines this as <component>/<package>/<file>.c */
#endif

/* Per module (file) log control structure
 */
static volatile SXE_LOG_CONTROL sxe_log_control = {SXE_LOG_LEVEL_OVER_MAXIMUM, NULL, SXE_FILE};

/**
 * - Logging macros
 *   - SXE<log type><log level><log arguments>
 *     - Where:
 *       - <log type> is one of:
 *         - 'E' procedure (e)ntry
 *         - 'L' regular (l)og line
 *         - 'R' procedure (r)eturn
 *         - 'A' (a)assertion
 *         - 'V' (v)erify (assertion whose side effects are honored in release mode)
 *       - <log level> is one of:
 *         - 1 fatal (critical errors, assertions)
 *         - 2 error (high severity errors)
 *         - 3 warning (medium serverity errors)
 *         - 4 info (transactions, low severity errors)
 *         - 5 debug (release mode debug, used for OEM integration in SXL2)
 *         - 6 trace (function entry/exit, tell the "story" of what is going on)
 *         - 7 dump (packet dumps, more detailed and verbose debug information)
 *       - <log arguments> is one of:
 *         - 1 to 9
 * - Notes:
 *   - Use 'make SXE_DEBUG=1' to use instrumentation macros
 *   - Log type Verify keeps the condition if 'make SXE_DEBUG=0'
 */

/**
 * - Example C source code:
 *   - void
 *   - foo1 ( int a, int b )
 *   - {
 *   -     int result;
 *   -     SXEE6("foo1(a=%d, b=%d)", a, b);
 *   -     SXEL1("foo1 example critical:1\n");
 *   -     SXEL2("foo1 example high:2\n");
 *   -     SXEL3("foo1 example medium:3\n");
 *   -     SXEL4("foo1 example low:4\n");
 *   -     SXEL5("foo1 example oem:5\n");
 *   -     SXEL6("foo1 example debug:6\n");
 *   -     SXEV6((result = foo2(b,a)),== 10,"result=%d",result);
 *   -     SXER6("return");
 *   - }
 *   -
 *   - int
 *   - foo2 ( int c, int d )
 *   - {
 *   -     SXEE6("foo2(c=%d, d=%d)", c, d);
 *   -     SXEL1("foo2 example critical:1\n");
 *   -     SXEL2("foo2 example high:2\n");
 *   -     SXEL3("foo2 example medium:3\n");
 *   -     SXEL4("foo2 example low:4\n");
 *   -     SXEL5("foo2 example oem:5\n");
 *   -     SXEL6("foo2 example debug:6\n");
 *   -     SXER6("return");
 *   -     return (c+d);
 *   - }
 * - Example output with 'make SXE_DEBUG=1':
 *   - + foo1(a=1, b=2) { // foo.c:32
 *   -   - foo1 example critical:1
 *   -   - foo1 example high:2
 *   -   - foo1 example medium:3
 *   -   - foo1 example low:4
 *   -   - foo1 example oem:5
 *   -   - foo1 example debug:6
 *   -   + foo2(c=2, d=1) { // foo.c:46
 *   -     - foo2 example critical:1
 *   -     - foo2 example high:2
 *   -     - foo2 example medium:3
 *   -     - foo2 example low:4
 *   -     - foo2 example oem:5
 *   -     - foo2 example debug:6
 *   -     } // foo.c:53
 *   - ERROR: debug assertion '(result = foo2(b,a))==10' failed
 *     at sxe.c:39; result=3
 * - Example output with 'make SXE_DEBUG=0':
 *   - - foo1 example critical:1
 *   - - foo2 example critical:1
 */

#ifndef SXE_IF_LEVEL_GE
#define SXE_IF_LEVEL_GE(line_level) if ((line_level) <= sxe_log_control.level)
#endif

#ifndef SXE_FILE
#   ifdef MAK_FILE
#       define SXE_FILE MAK_FILE     // Normally, the mak system defines this as <component>/<package>/<file>.c
#   else
#       define SXE_FILE __FILE__
#   endif
#endif

#define SXE_LOG_FRAME_CREATE(entry_level) SXE_LOG_FRAME frame; frame.level=(entry_level); frame.file=SXE_FILE; \
                                          frame.line=__LINE__; frame.function=__func__; frame.id=~0U;
#define SXE_LOG_FRAME_SET_ID(this)        frame.id=((this) != NULL ? (this)->id : ~0U)
#define SXE_LOG_NO                        &sxe_log_control, __func__, ~0U, __LINE__
#define SXE_LOG_NO_ASSERT                 &sxe_log_control, __FILE__, ~0U, __LINE__
#define SXE_LOG_FRAME_NO                  &frame, &sxe_log_control, ~0U

#define SXEL1(...)                        do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_FATAL      ) {sxe_log(SXE_LOG_NO,SXE_LOG_LEVEL_FATAL      ,__VA_ARGS__);} } while (0)
#define SXEL2(...)                        do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_ERROR      ) {sxe_log(SXE_LOG_NO,SXE_LOG_LEVEL_ERROR      ,__VA_ARGS__);} } while (0)
#define SXEL3(...)                        do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_WARNING    ) {sxe_log(SXE_LOG_NO,SXE_LOG_LEVEL_WARNING    ,__VA_ARGS__);} } while (0)
#define SXEL4(...)                        do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_INFORMATION) {sxe_log(SXE_LOG_NO,SXE_LOG_LEVEL_INFORMATION,__VA_ARGS__);} } while (0)
#define SXEL5(...)                        do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_DEBUG      ) {sxe_log(SXE_LOG_NO,SXE_LOG_LEVEL_DEBUG      ,__VA_ARGS__);} } while (0)

#if SXE_DEBUG
#define SXEL1A6(...)                      SXEA6(__VA_ARGS__)
#define SXEL2A6(...)                      SXEA6(__VA_ARGS__)
#define SXEL3A6(...)                      SXEA6(__VA_ARGS__)
#define SXEL4A6(...)                      SXEA6(__VA_ARGS__)
#define SXEL5A6(...)                      SXEA6(__VA_ARGS__)
#else
#define SXEL1A6(expr, ...)                do { if (!(expr)) SXEL1(__VA_ARGS__); } while (0)
#define SXEL2A6(expr, ...)                do { if (!(expr)) SXEL2(__VA_ARGS__); } while (0)
#define SXEL3A6(expr, ...)                do { if (!(expr)) SXEL3(__VA_ARGS__); } while (0)
#define SXEL4A6(expr, ...)                do { if (!(expr)) SXEL4(__VA_ARGS__); } while (0)
#define SXEL5A6(expr, ...)                do { if (!(expr)) SXEL5(__VA_ARGS__); } while (0)
#endif

#define SXEA1(con,...)                    do { if((     con)==0){sxe_log_assert(SXE_LOG_NO_ASSERT,#con,__VA_ARGS__           );} } while (0)
#define SXEV1(func,con,...)               do { if((func con)==0){sxe_log_assert(SXE_LOG_NO_ASSERT,#func " " #con, __VA_ARGS__);} } while (0)

#define SXED1(ptr,len)                    do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_FATAL      ) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_NO,SXE_LOG_LEVEL_FATAL      ,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_FATAL,       "Attempted to dump memory with len <= 0");}} } while (0)
#define SXED2(ptr,len)                    do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_ERROR      ) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_NO,SXE_LOG_LEVEL_ERROR      ,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_ERROR,       "Attempted to dump memory with len <= 0");}} } while (0)
#define SXED3(ptr,len)                    do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_WARNING    ) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_NO,SXE_LOG_LEVEL_WARNING    ,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_WARNING,     "Attempted to dump memory with len <= 0");}} } while (0)
#define SXED4(ptr,len)                    do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_INFORMATION) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_NO,SXE_LOG_LEVEL_INFORMATION,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_INFORMATION, "Attempted to dump memory with len <= 0");}} } while (0)
#define SXED5(ptr,len)                    do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_DEBUG      ) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_NO,SXE_LOG_LEVEL_DEBUG      ,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_DEBUG,       "Attempted to dump memory with len <= 0");}} } while (0)

#define SXEE5(...)                        {SXE_LOG_FRAME_CREATE(SXE_LOG_LEVEL_DEBUG); SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_TRACE) {                                                     sxe_log_entry(        SXE_LOG_FRAME_NO,SXE_LOG_LEVEL_DEBUG,__VA_ARGS__);} else {sxe_log_frame_push(&frame,false);}
#define SXER5(...)                                                                    SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_DUMP ) {sxe_log(SXE_LOG_NO,SXE_LOG_LEVEL_DEBUG,__VA_ARGS__); sxe_log_return(&sxe_log_control,&frame,SXE_LOG_LEVEL_DEBUG            );} else {sxe_log_frame_pop(&frame);}}

#if SXE_DEBUG

#define SXEL6(...)                        do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_TRACE) {sxe_log(SXE_LOG_NO,SXE_LOG_LEVEL_TRACE,__VA_ARGS__);} } while (0)
#define SXEL7(...)                        do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_DUMP ) {sxe_log(SXE_LOG_NO,SXE_LOG_LEVEL_DUMP ,__VA_ARGS__);} } while (0)

#define SXEE6(...)                        {SXE_LOG_FRAME_CREATE(SXE_LOG_LEVEL_TRACE); SXE_IF_LEVEL_GE(6) {sxe_log_entry(SXE_LOG_FRAME_NO,SXE_LOG_LEVEL_TRACE,__VA_ARGS__);} else {sxe_log_frame_push(&frame,false);}
#define SXEE7(...)                        {SXE_LOG_FRAME_CREATE(SXE_LOG_LEVEL_DUMP ); SXE_IF_LEVEL_GE(7) {sxe_log_entry(SXE_LOG_FRAME_NO,SXE_LOG_LEVEL_DUMP ,__VA_ARGS__);} else {sxe_log_frame_push(&frame,false);}

#define SXER6(...)                        SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_TRACE) {sxe_log(SXE_LOG_NO,SXE_LOG_LEVEL_TRACE,__VA_ARGS__); sxe_log_return(&sxe_log_control,&frame,SXE_LOG_LEVEL_TRACE);} else {sxe_log_frame_pop(&frame);}}
#define SXER7(...)                        SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_DUMP ) {sxe_log(SXE_LOG_NO,SXE_LOG_LEVEL_DUMP ,__VA_ARGS__); sxe_log_return(&sxe_log_control,&frame,SXE_LOG_LEVEL_DUMP );} else {sxe_log_frame_pop(&frame);}}

#define SXEA6(con,...)                    do { if((     con)==0){sxe_log_assert(SXE_LOG_NO_ASSERT,#con,__VA_ARGS__);} } while (0)
#define SXEV6(func,con,...)               do { if((func con)==0){sxe_log_assert(SXE_LOG_NO_ASSERT,#func " " #con,__VA_ARGS__);} } while (0)

#define SXED6(ptr,len)                    do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_TRACE) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_NO,SXE_LOG_LEVEL_TRACE,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_TRACE, "Attempted to dump memory with len <= 0");}} } while (0)
#define SXED7(ptr,len)                    do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_DUMP ) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_NO,SXE_LOG_LEVEL_DUMP ,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_DUMP,  "Attempted to dump memory with len <= 0");}} } while (0)

#else /* SXE_DEBUG == 0 */

#define SXEL6(...)                        do { } while (0)
#define SXEL7(...)                        do { } while (0)

#define SXEE6(...)                        do { } while (0)
#define SXEE7(...)                        do { } while (0)

#define SXER6(...)                        do { } while (0)
#define SXER7(...)                        do { } while (0)

#define SXEA6(con,...)                    do { } while (0)
#define SXEV6(func,con,...)               do { ((void)(func)); } while (0)

#define SXED6(ptr,len)                    do { } while (0)
#define SXED7(ptr,len)                    do { } while (0)

#endif

#define SXE_LOG_ID                &sxe_log_control, __func__, ((this) != NULL ? (this)->id : ~0U), __LINE__
#define SXE_LOG_ID_ASSERT         &sxe_log_control, __FILE__, ((this) != NULL ? (this)->id : ~0U), __LINE__
#define SXE_LOG_FRAME_ID  &frame, &sxe_log_control,           ((this) != NULL ? (this)->id : ~0U)

#define SXEL1I(...)                       do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_FATAL      ) {sxe_log(SXE_LOG_ID,SXE_LOG_LEVEL_FATAL      ,__VA_ARGS__);} } while (0)
#define SXEL2I(...)                       do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_ERROR      ) {sxe_log(SXE_LOG_ID,SXE_LOG_LEVEL_ERROR      ,__VA_ARGS__);} } while (0)
#define SXEL3I(...)                       do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_WARNING    ) {sxe_log(SXE_LOG_ID,SXE_LOG_LEVEL_WARNING    ,__VA_ARGS__);} } while (0)
#define SXEL4I(...)                       do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_INFORMATION) {sxe_log(SXE_LOG_ID,SXE_LOG_LEVEL_INFORMATION,__VA_ARGS__);} } while (0)
#define SXEL5I(...)                       do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_DEBUG      ) {sxe_log(SXE_LOG_ID,SXE_LOG_LEVEL_DEBUG      ,__VA_ARGS__);} } while (0)

#define SXEA1I(con,...)                   do { if((     con)==0){sxe_log_assert(SXE_LOG_ID_ASSERT,          #con,__VA_ARGS__);} } while (0)
#define SXEV1I(func,con,...)              do { if((func con)==0){sxe_log_assert(SXE_LOG_ID_ASSERT,#func " " #con,__VA_ARGS__);} } while (0)

#define SXED1I(ptr,len)                   do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_FATAL      ) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_ID,SXE_LOG_LEVEL_FATAL      ,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_FATAL,       "Attempted to dump memory with len <= 0");}} } while (0)
#define SXED2I(ptr,len)                   do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_ERROR      ) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_ID,SXE_LOG_LEVEL_ERROR      ,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_ERROR,       "Attempted to dump memory with len <= 0");}} } while (0)
#define SXED3I(ptr,len)                   do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_WARNING    ) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_ID,SXE_LOG_LEVEL_WARNING    ,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_WARNING,     "Attempted to dump memory with len <= 0");}} } while (0)
#define SXED4I(ptr,len)                   do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_INFORMATION) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_ID,SXE_LOG_LEVEL_INFORMATION,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_INFORMATION, "Attempted to dump memory with len <= 0");}} } while (0)
#define SXED5I(ptr,len)                   do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_DEBUG      ) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_ID,SXE_LOG_LEVEL_DEBUG      ,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_DEBUG,       "Attempted to dump memory with len <= 0");}} } while (0)

#if SXE_DEBUG

#define SXEL6I(...)                       do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_TRACE) {sxe_log(SXE_LOG_ID,SXE_LOG_LEVEL_TRACE,__VA_ARGS__);} } while (0)
#define SXEL7I(...)                       do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_DUMP ) {sxe_log(SXE_LOG_ID,SXE_LOG_LEVEL_DUMP ,__VA_ARGS__);} } while (0)

#define SXEE6I(...)                       {SXE_LOG_FRAME_CREATE(SXE_LOG_LEVEL_TRACE); SXE_LOG_FRAME_SET_ID(this); SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_TRACE) {sxe_log_entry(SXE_LOG_FRAME_ID,SXE_LOG_LEVEL_TRACE,__VA_ARGS__);} else {sxe_log_frame_push(&frame,false);}
#define SXEE7I(...)                       {SXE_LOG_FRAME_CREATE(SXE_LOG_LEVEL_DUMP ); SXE_LOG_FRAME_SET_ID(this); SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_DUMP ) {sxe_log_entry(SXE_LOG_FRAME_ID,SXE_LOG_LEVEL_DUMP ,__VA_ARGS__);} else {sxe_log_frame_push(&frame,false);}

#define SXER6I(...)                       SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_TRACE) {sxe_log(SXE_LOG_ID,SXE_LOG_LEVEL_TRACE,__VA_ARGS__); sxe_log_return(&sxe_log_control,&frame,SXE_LOG_LEVEL_TRACE);} else {sxe_log_frame_pop(&frame);}}
#define SXER7I(...)                       SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_DUMP ) {sxe_log(SXE_LOG_ID,SXE_LOG_LEVEL_DUMP ,__VA_ARGS__); sxe_log_return(&sxe_log_control,&frame,SXE_LOG_LEVEL_DUMP );} else {sxe_log_frame_pop(&frame);}}

#define SXEA6I(con,...)                   do { if((     con)==0){sxe_log_assert(SXE_LOG_ID_ASSERT,          #con,__VA_ARGS__);} } while (0)
#define SXEV6I(func,con,...)              do { if((func con)==0){sxe_log_assert(SXE_LOG_ID_ASSERT,#func " " #con,__VA_ARGS__);} } while (0)

#define SXED6I(ptr,len)                   do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_TRACE) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_ID,SXE_LOG_LEVEL_TRACE,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_TRACE, "Attempted to dump memory with len <= 0");}} } while (0)
#define SXED7I(ptr,len)                   do { SXE_IF_LEVEL_GE(SXE_LOG_LEVEL_DUMP ) {if (len > 0) {sxe_log_dump_memory(SXE_LOG_ID,SXE_LOG_LEVEL_DUMP ,ptr,len);} else {sxe_log(SXE_LOG_NO, SXE_LOG_LEVEL_DUMP,  "Attempted to dump memory with len <= 0");}} } while (0)

#else /* SXE_DEBUG == 0 */

#define SXEL6I(...)                       do { } while (0)
#define SXEL7I(...)                       do { } while (0)

#define SXEE6I(...)                       do { } while (0)
#define SXEE7I(...)                       do { } while (0)

#define SXER6I(...)                       do { } while (0)
#define SXER7I(...)                       do { } while (0)

#define SXEA6I(con,...)                   do { } while (0)
#define SXEV6I(func,con,...)              do { ((void)(func)); } while (0)

#define SXED6I(ptr,len)                   do { } while (0)
#define SXED7I(ptr,len)                   do { } while (0)

#endif

#include "sxe-log-proto.h"
#include "sxe-log-legacy.h"
#include "sxe-strlcpy-proto.h"
#include "sxe-str-encode-proto.h"

/* Pushing and popping function call information is done inline for performance */

static inline SXE_LOG_STACK *
sxe_log_get_stack(const char * file, unsigned id, int line)
{
    SXE_LOG_STACK * stack;

#ifdef __APPLE__
    if (!sxe_log_stack_key) {
        pthread_key_create(&sxe_log_stack_key, free);
    }

    stack = pthread_getspecific(sxe_log_stack_key);
    if (stack == NULL) {
        if ((stack = calloc(1, sizeof(SXE_LOG_STACK))) == NULL) {
            sxe_log_assert(NULL, file, id, line, "calloc(1, sizeof(SXE_LOG_STACK) != NULL", "Failed to allocate thread's log stack");
        }

        pthread_setspecific(sxe_log_stack_key, stack);
    }

    stack = pthread_getspecific(sxe_log_stack_key);
#else
    SXE_UNUSED_PARAMETER(file);
    SXE_UNUSED_PARAMETER(id);
    SXE_UNUSED_PARAMETER(line);

    stack = &sxe_log_stack;
#endif

    if (stack->top == NULL) {
        stack->top = &stack->bottom;
    }

    return stack;
}

#if SXE_DEBUG
static inline unsigned
sxe_log_get_indent_maximum(void)
{
#ifdef __APPLE__
    if (!sxe_log_indent_maximum_key) {
        pthread_key_create(&sxe_log_indent_maximum_key, NULL);
    }

    return (unsigned)(uintptr_t)pthread_getspecific(sxe_log_indent_maximum_key);
#else
    return sxe_log_indent_maximum;
#endif
}

static inline void
sxe_log_set_indent_maximum(unsigned maximum)
{
#ifdef __APPLE__
    if (!sxe_log_indent_maximum_key) {
        pthread_key_create(&sxe_log_indent_maximum_key, NULL);
    }

    pthread_setspecific(sxe_log_indent_maximum_key, (void *)(uintptr_t)maximum);
#else
    sxe_log_indent_maximum = maximum;
#endif
}
#endif

static inline void
sxe_log_frame_push(SXE_LOG_FRAME * frame, bool visible)
{
    SXE_LOG_STACK * stack          = sxe_log_get_stack(frame->file, frame->id, frame->line);
#if SXE_DEBUG
    unsigned        log_indent_maximum = sxe_log_get_indent_maximum();
#endif

/* CAREFUL!!  things such as lib-sxe-log/test/test-log-levels.c define SXE_DEBUG to 1 before including us!! */
#if !defined(SXE_RELEASE) || !SXE_RELEASE || (defined(SXE_COVERAGE) && SXE_COVERAGE)
    /*
     * XXX: Only checked for debug & coverage builds
     * This is done to allow us to use -O2 with -pie for release builds.  With
     * both of these flags, the optimizer can place lower-level stack frames at
     * higher memory addresses!!
     */
    if (frame >= stack->top && stack->top != &stack->bottom) {
        sxe_log_assert(NULL, frame->file, frame->id, frame->line, "frame < sxe_stack.top",
                       "New stack frame %p is not lower than previous %p; maybe SXEE## was called and SXER## was not?",
                       frame, stack->top);
    }
#endif

#if SXE_DEBUG
    SXEA62(stack->top->indent <= log_indent_maximum, "Log indent %u exceeds maximum log indent %u",
           stack->top->indent,   log_indent_maximum);
#endif
    frame->indent = stack->top->indent;

    /* If this is a visible entry, increase the indent
     */
    if (visible) {
        frame->indent++;
#if SXE_DEBUG
        sxe_log_set_indent_maximum(frame->indent > log_indent_maximum ? frame->indent : log_indent_maximum);
#endif
    }

    frame->caller = stack->top;
    stack->top    = frame;
}

static inline void
sxe_log_frame_pop(SXE_LOG_FRAME * frame)
{
    SXE_LOG_STACK * stack = sxe_log_get_stack(frame->file, frame->id, frame->line);

    if (stack->top != frame) {
        sxe_log_assert(NULL, frame->file, frame->id, frame->line, "sxe_log_stack.top != frame",
                       "%s: The frame is not on the top of the log stack!\n",   __func__);
    }

    stack->top = stack->top->caller;
}

#endif /* __SXE_LOG_H__ */
