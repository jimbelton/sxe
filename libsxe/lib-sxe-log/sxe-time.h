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

#ifndef __SXE_TIME_H__
#define __SXE_TIME_H__

#include <stdint.h>
#include <time.h>

/* Format: YYYYmmDDHHMMSS[.fff...] + '\0'
 */
#define SXE_TIMESTAMP_LENGTH(fractional_digits) (16 + (fractional_digits))
#define SXE_TIME_BITS_IN_FRACTION               (sizeof(SXE_TIME_FRACTION) * 8)
#define SXE_TIME_1_SEC                          ((SXE_TIME)(1ULL << SXE_TIME_BITS_IN_FRACTION))
#define SXE_TIME_FROM_TIMEVAL(tv)               (((SXE_TIME)(tv)->tv_sec  << SXE_TIME_BITS_IN_FRACTION) \
                                                + ((SXE_TIME)(tv)->tv_usec * SXE_TIME_1_SEC / 1000000))

#define SXE_TIME_FROM_MSEC(msec)                (  ((SXE_TIME)(msec / 1000) << SXE_TIME_BITS_IN_FRACTION) \
                                                + (((SXE_TIME)(msec % 1000) << SXE_TIME_BITS_IN_FRACTION) / 1000))

#define SXE_TIME_TO_TIMEVAL(sxe_time, tv)       do { \
                                                    uint64_t sxe_time_lo32 = (unsigned)sxe_time; \
                                                    (tv)->tv_sec  = (unsigned)((sxe_time) >> SXE_TIME_BITS_IN_FRACTION); \
                                                    (tv)->tv_usec = (unsigned)(sxe_time_lo32 * 1000000 / SXE_TIME_1_SEC); \
                                                } while (0)

#define SXE_TIME_FROM_DOUBLE_SECONDS(seconds)   ((uint64_t)( \
       ((uint64_t)(((uint64_t)( seconds                               )) << (SXE_TIME_BITS_IN_FRACTION))) \
     + ((uint64_t)((  (double)((seconds) - (double)(uint64_t)(seconds))) *  ((double)SXE_TIME_1_SEC   ))) \
    ))

#define SXE_TIME_TO_DOUBLE_SECONDS(sxe_time)    ((double)  ( \
       (  (double)((          (uint64_t)(sxe_time)                     ) >> (SXE_TIME_BITS_IN_FRACTION))) \
     + (  (double)((  (double)(uint32_t)(sxe_time)                     ) /  ((double)SXE_TIME_1_SEC   ))) \
    ))

typedef uint64_t SXE_TIME;
typedef uint32_t SXE_TIME_FRACTION;

static inline time_t
sxe_time_to_unix_time(SXE_TIME sxe_time)
{
    return (time_t)(sxe_time >> SXE_TIME_BITS_IN_FRACTION);
}

static inline SXE_TIME
sxe_time_from_unix_time(time_t unix_time)
{
    return (SXE_TIME)(unix_time) << SXE_TIME_BITS_IN_FRACTION;
}

static inline SXE_TIME
sxe_time_from_double_seconds(double seconds)
{
    return SXE_TIME_FROM_DOUBLE_SECONDS(seconds);
}

static inline double
sxe_time_to_double_seconds(SXE_TIME sxe_time)
{
    return SXE_TIME_TO_DOUBLE_SECONDS(sxe_time);
}

#include "sxe-time-proto.h"

#endif /* __SXE_LOG_H__ */
