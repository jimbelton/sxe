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

/* These must be separate functions from main and eachother so they have their own "frame" objects; see the SXEE## macros.
 * In addition, they are separated into this helper module to give them a separate sxe_log_control object.
 */

#if SXE_DEBUG != 0
#define LOCAL_SXE_DEBUG 1
#endif

#undef   SXE_DEBUG      /* Since we are testing diagnostic functions, this test program forces debug mode */
#define  SXE_DEBUG 1

#include "sxe-log.h"
#include "tap.h"

#ifdef __GNUC__
#define NOINLINE __attribute__ ((noinline))
#else
#define NOINLINE /* empty */
#endif
static void test_level_seven(SXE_LOG_LEVEL) NOINLINE;

void test_level_six(SXE_LOG_LEVEL level);

static void
test_level_seven(SXE_LOG_LEVEL level)
{
    SXEE7("(level=%u)", level);    /* Function name should be automatically inserted */

    if (level == SXE_LOG_LEVEL_DUMP) {
        SXEL2("No indent");
        sxe_log_set_level(SXE_LOG_LEVEL_DEBUG);
        SXEL2("Indented by 2");
    }

    sxe_log_set_level(level);
    SXEL2("Set log level to %u", level);
    SXER7("return // seven");
}

void
test_level_six(SXE_LOG_LEVEL level)
{
    SXEE6("(level=%u)", level);    /* Function name should be automatically inserted */
    test_level_seven(level);
    SXER6("return // six");
}
