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

/*
 * Compiler-dependent macros to declare that functions take printf-like
 * or scanf-like arguments.  They are null except for versions of gcc
 * that are known to support the features properly (old versions of gcc-2
 * didn't permit keeping the keywords out of the application namespace).
 */
#if !defined(__GNUC__) || __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#   define __printflike(fmtarg, firstvararg)
#else
#   define __printflike(fmtarg, firstvararg) __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#endif

/* The following log levels are based on discussions with Mario
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

#define SXE_UNUSED_ARGUMENT(x) (void)x
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

/* Private type used to allow dynamic modification of log levels
 */
typedef struct SXE_LOG_CONTROL {
    SXE_LOG_LEVEL                     level;
    volatile struct SXE_LOG_CONTROL * next;
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

/* Per module (file) log control structure
 */
static volatile SXE_LOG_CONTROL sxe_log_control = {SXE_LOG_LEVEL_OVER_MAXIMUM, NULL};

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

/* Common blocks stuff that we pass to functions; SXE_FILE is the file in the form (e.g.) "libsxe/lib-sxe-log/sxe-log.c"
 */
#ifndef SXE_FILE
#   define SXE_FILE __FILE__    /* Normally, the make system defines this as <component>/<package>/<file>.c */
#endif

#define SXE_LOG_FRAME_CREATE(entry_level) SXE_LOG_FRAME frame; frame.level=(entry_level); frame.file=SXE_FILE; \
                                          frame.line=__LINE__; frame.function=__func__; frame.id=~0U;
#define SXE_LOG_FRAME_SET_ID(this)        frame.id=((this) != NULL ? (this)->id : ~0U)
#define SXE_LOG_NO                        &sxe_log_control, SXE_FILE, ~0U, __LINE__
#define SXE_LOG_NO_ASSERT                 &sxe_log_control, __FILE__, ~0U, __LINE__
#define SXE_LOG_FRAME_NO                  &frame, &sxe_log_control, ~0U

#define SXEL1(...)                        SXE_IF_LEVEL_GE(1) {sxe_log(SXE_LOG_NO,1,__VA_ARGS__                   );}
#define SXEL2(...)                        SXE_IF_LEVEL_GE(2) {sxe_log(SXE_LOG_NO,2,__VA_ARGS__                   );}
#define SXEL3(...)                        SXE_IF_LEVEL_GE(3) {sxe_log(SXE_LOG_NO,3,__VA_ARGS__                   );}
#define SXEL4(...)                        SXE_IF_LEVEL_GE(4) {sxe_log(SXE_LOG_NO,4,__VA_ARGS__                   );}
#define SXEL5(...)                        SXE_IF_LEVEL_GE(5) {sxe_log(SXE_LOG_NO,5,__VA_ARGS__                   );}

#define SXEA1(con,...)                    if((     con)==0){sxe_log_assert(SXE_LOG_NO_ASSERT,#con,__VA_ARGS__           );}
#define SXEV1(func,con,...)               if((func con)==0){sxe_log_assert(SXE_LOG_NO_ASSERT,#func " " #con, __VA_ARGS__);}

#define SXED1(ptr,len)                    SXE_IF_LEVEL_GE(1) {sxe_log_dump_memory(SXE_LOG_NO,1,ptr,len);}
#define SXED2(ptr,len)                    SXE_IF_LEVEL_GE(2) {sxe_log_dump_memory(SXE_LOG_NO,2,ptr,len);}
#define SXED3(ptr,len)                    SXE_IF_LEVEL_GE(3) {sxe_log_dump_memory(SXE_LOG_NO,3,ptr,len);}
#define SXED4(ptr,len)                    SXE_IF_LEVEL_GE(4) {sxe_log_dump_memory(SXE_LOG_NO,4,ptr,len);}
#define SXED5(ptr,len)                    SXE_IF_LEVEL_GE(5) {sxe_log_dump_memory(SXE_LOG_NO,5,ptr,len);}

#define SXEE5(...)                         {SXE_LOG_FRAME_CREATE(5); SXE_IF_LEVEL_GE(5) {sxe_log_entry(SXE_LOG_FRAME_NO,5,__VA_ARGS__     );} else {sxe_log_frame_push(&frame,false);}
#define SXER5(...)                         SXE_IF_LEVEL_GE(5) {sxe_log(SXE_LOG_NO,5,__VA_ARGS__); sxe_log_return(&sxe_log_control,&frame,5);} else {sxe_log_frame_pop(&frame);}}

#if SXE_DEBUG

#define SXEL6(...)                        SXE_IF_LEVEL_GE(6) {sxe_log(SXE_LOG_NO,6,__VA_ARGS__);}
#define SXEL7(...)                        SXE_IF_LEVEL_GE(7) {sxe_log(SXE_LOG_NO,7,__VA_ARGS__);}

#define SXEE6(...)                        {SXE_LOG_FRAME_CREATE(6); SXE_IF_LEVEL_GE(6) {sxe_log_entry(SXE_LOG_FRAME_NO,6,__VA_ARGS__);} else {sxe_log_frame_push(&frame,false);}
#define SXEE7(...)                        {SXE_LOG_FRAME_CREATE(7); SXE_IF_LEVEL_GE(7) {sxe_log_entry(SXE_LOG_FRAME_NO,7,__VA_ARGS__);} else {sxe_log_frame_push(&frame,false);}

#define SXER6(...)                        SXE_IF_LEVEL_GE(6) {sxe_log(SXE_LOG_NO,6,__VA_ARGS__); sxe_log_return(&sxe_log_control,&frame,6);} else {sxe_log_frame_pop(&frame);}}
#define SXER7(...)                        SXE_IF_LEVEL_GE(7) {sxe_log(SXE_LOG_NO,7,__VA_ARGS__); sxe_log_return(&sxe_log_control,&frame,7);} else {sxe_log_frame_pop(&frame);}}

#define SXEA6(con,...)                    if((     con)==0){sxe_log_assert(SXE_LOG_NO_ASSERT,#con,__VA_ARGS__);}
#define SXEV6(func,con,...)               if((func con)==0){sxe_log_assert(SXE_LOG_NO_ASSERT,#func " " #con,__VA_ARGS__);}

#define SXED6(ptr,len)                    SXE_IF_LEVEL_GE(6) {sxe_log_dump_memory(SXE_LOG_NO,6,ptr,len);}
#define SXED7(ptr,len)                    SXE_IF_LEVEL_GE(7) {sxe_log_dump_memory(SXE_LOG_NO,7,ptr,len);}

#else /* SXE_DEBUG == 0 */

#define SXEL6(...)
#define SXEL7(...)

#define SXEE6(...)
#define SXEE7(...)

#define SXER6(...)
#define SXER7(...)

#define SXEA6(con,...)
#define SXEV6(func,con,...)               ((void)(func))

#define SXED6(ptr,len)
#define SXED7(ptr,len)

#endif

#define SXE_LOG_ID                &sxe_log_control, SXE_FILE, ((this) != NULL ? (this)->id : ~0U), __LINE__
#define SXE_LOG_ID_ASSERT         &sxe_log_control, __FILE__, ((this) != NULL ? (this)->id : ~0U), __LINE__
#define SXE_LOG_FRAME_ID  &frame, &sxe_log_control,           ((this) != NULL ? (this)->id : ~0U)

#define SXEL1I(...)                       SXE_IF_LEVEL_GE(1) {sxe_log(SXE_LOG_ID,1,__VA_ARGS__);}
#define SXEL2I(...)                       SXE_IF_LEVEL_GE(2) {sxe_log(SXE_LOG_ID,2,__VA_ARGS__);}
#define SXEL3I(...)                       SXE_IF_LEVEL_GE(3) {sxe_log(SXE_LOG_ID,3,__VA_ARGS__);}
#define SXEL4I(...)                       SXE_IF_LEVEL_GE(4) {sxe_log(SXE_LOG_ID,4,__VA_ARGS__);}
#define SXEL5I(...)                       SXE_IF_LEVEL_GE(5) {sxe_log(SXE_LOG_ID,5,__VA_ARGS__);}

#define SXEA1I(con,...)                   if((     con)==0){sxe_log_assert(SXE_LOG_ID_ASSERT,          #con,__VA_ARGS__);}
#define SXEV1I(func,con,...)              if((func con)==0){sxe_log_assert(SXE_LOG_ID_ASSERT,#func " " #con,__VA_ARGS__);}

#define SXED1I(ptr,len)                   SXE_IF_LEVEL_GE(1) {sxe_log_dump_memory(SXE_LOG_ID,1,ptr,len);}
#define SXED2I(ptr,len)                   SXE_IF_LEVEL_GE(2) {sxe_log_dump_memory(SXE_LOG_ID,2,ptr,len);}
#define SXED3I(ptr,len)                   SXE_IF_LEVEL_GE(3) {sxe_log_dump_memory(SXE_LOG_ID,3,ptr,len);}
#define SXED4I(ptr,len)                   SXE_IF_LEVEL_GE(4) {sxe_log_dump_memory(SXE_LOG_ID,4,ptr,len);}
#define SXED5I(ptr,len)                   SXE_IF_LEVEL_GE(5) {sxe_log_dump_memory(SXE_LOG_ID,5,ptr,len);}

#if SXE_DEBUG

#define SXEL6I(...)                       SXE_IF_LEVEL_GE(6) {sxe_log(SXE_LOG_ID,6,__VA_ARGS__);}
#define SXEL7I(...)                       SXE_IF_LEVEL_GE(7) {sxe_log(SXE_LOG_ID,7,__VA_ARGS__);}

#define SXEE6I(...)                       {SXE_LOG_FRAME_CREATE(6); SXE_LOG_FRAME_SET_ID(this); SXE_IF_LEVEL_GE(6) {sxe_log_entry(SXE_LOG_FRAME_ID,6,__VA_ARGS__);} else {sxe_log_frame_push(&frame,false);}
#define SXEE7I(...)                       {SXE_LOG_FRAME_CREATE(7); SXE_LOG_FRAME_SET_ID(this); SXE_IF_LEVEL_GE(7) {sxe_log_entry(SXE_LOG_FRAME_ID,7,__VA_ARGS__);} else {sxe_log_frame_push(&frame,false);}

#define SXER6I(...)                       SXE_IF_LEVEL_GE(6) {sxe_log(SXE_LOG_ID,6,__VA_ARGS__); sxe_log_return(&sxe_log_control,&frame,6);} else {sxe_log_frame_pop(&frame);}}
#define SXER7I(...)                       SXE_IF_LEVEL_GE(7) {sxe_log(SXE_LOG_ID,7,__VA_ARGS__); sxe_log_return(&sxe_log_control,&frame,7);} else {sxe_log_frame_pop(&frame);}}

#define SXEA6I(con,...)                   if((     con)==0){sxe_log_assert(SXE_LOG_ID_ASSERT,          #con,__VA_ARGS__);}
#define SXEV6I(func,con,...)              if((func con)==0){sxe_log_assert(SXE_LOG_ID_ASSERT,#func " " #con,__VA_ARGS__);}

#define SXED6I(ptr,len)                   SXE_IF_LEVEL_GE(6) {sxe_log_dump_memory(SXE_LOG_ID,6,ptr,len);}
#define SXED7I(ptr,len)                   SXE_IF_LEVEL_GE(7) {sxe_log_dump_memory(SXE_LOG_ID,7,ptr,len);}

#else /* SXE_DEBUG == 0 */

#define SXEL6I(...)
#define SXEL7I(...)

#define SXEE6I(...)
#define SXEE7I(...)

#define SXER6I(...)
#define SXER7I(...)

#define SXEA6I(con,...)
#define SXEV6I(func,con,...)              ((void)(func))

#define SXED6I(ptr,len)
#define SXED7I(ptr,len)

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
    SXE_UNUSED_ARGUMENT(file);
    SXE_UNUSED_ARGUMENT(id);
    SXE_UNUSED_ARGUMENT(line);

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

    if (frame >= stack->top && stack->top != &stack->bottom) {
        sxe_log_assert(NULL, frame->file, frame->id, frame->line, "frame < sxe_stack.top",
                       "New stack frame %p is not lower than previous %p; maybe SXEE## was called and SXER## was not?",
                       frame, stack->top);
    }

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
