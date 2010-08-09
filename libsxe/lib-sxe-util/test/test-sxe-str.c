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

#include "sxe-util.h"
#include "tap.h"

char foobar[] = "foobarino";
char FOObar[] = "FOObar";

int
main(void)
{
    plan_tests(11);
    is(    sxe_strnchr(foobar, 'b',     6), &foobar[3], "Found 'b' in 'foobarino':6");
    is(    sxe_strnchr(foobar, 'b',     2), NULL,       "Did not find 'b' in 'foobarino':2");
    is(    sxe_strnchr(foobar, 'x',    10), NULL,       "Did not find 'x' in 'foobarino':10");
    is(    sxe_strnstr(foobar, "foo",   6), &foobar[0], "Found 'foo' in 'foobarino':6");
    is(    sxe_strnstr(foobar, "bar",   6), &foobar[3], "Found 'bar' in 'foobarino':6");
    is(    sxe_strnstr(foobar, "foo",   2), NULL,       "Did not find 'foo' in 'foobarino':2");
    is(    sxe_strnstr(foobar, "rino",  6), NULL,       "Did not find 'rino' in 'foobarino':6");
    is(    sxe_strnstr(foobar, "gorp", 13), NULL,       "Did not find 'gorp' in 'foobarino':13");
    is(sxe_strncasestr(FOObar, "foo",   6), &FOObar[0], "Found 'foo' in 'FOObar':6");
    is(sxe_strncasestr(FOObar, "foo",   2), NULL,       "Did not find 'foo' in 'FOObar':2");
    is(sxe_strncasestr(FOObar, "gorp", 10), NULL,       "Did not find 'gorp' in 'FOObar':10");
    return exit_status();
}


