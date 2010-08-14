/* Copyright (c) 2009 Jim Belton
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

#include <stddef.h>
#include "tap.h"
#include "sxe-cstr.h"

int
main(void)
{
    sxe_cstr     cstr;
    char         buf[16];
    const char * str;
    time_t       era = 0;
    struct tm    tm;

    plan_tests(20);
    sxe_cstr_make(&cstr, buf, sizeof(buf));
    is(sxe_cstr_length(&cstr), 0,                                                        "sxe_cstr is initially 0 length");
    is((unsigned)(str = sxe_cstr_get_str(&cstr))[0], (unsigned)'\0',                     "sxe_cstr is '\\0' terminated");
    is(sxe_cstr_printf(&cstr, "This string is too big"), 22,                             "Printf string is 22 characters");
    is_eq(sxe_cstr_get_str(&cstr), "This string is ",                                    "Truncated string is 'This string is '");
    gmtime_r(&era, &tm);
    is(sxe_cstr_ftime(&cstr, "%Y", &tm), 0,                                              "No room for year");
    ok(sxe_cstr_is_overflow(&cstr),                                                      "Overflow is recognized");
    sxe_cstr_clear(&cstr);
    ok(!sxe_cstr_is_overflow(&cstr),                                                     "Overflow was cleared");
    ok(sxe_cstr_ftime(&cstr, "%Y", &tm),                                                 "Year formatted");
    is_eq(sxe_cstr_get_str(&cstr), "1970",                                               "Time began in 1970");
    ok(sxe_cstr_eq(&cstr, SXE_CSTR_LITERAL(1970)),                                       "cstr eq SXE_CSTR_LITERAL(1970)");
    ok(sxe_cstr_eq(&cstr, &cstr),                                                        "cstr eq cstr");
    ok(sxe_cstr_ne(SXE_CSTR_EMPTY, &cstr),                                               "SXE_CSTR_EMPTY ne cstr");
    ok(sxe_cstr_cmp(SXE_CSTR_LITERAL(1234), SXE_CSTR_LITERAL(1235)) < 0,                 "'1234' lt '1235'");
    ok(sxe_cstr_eq(sxe_cstr_cat(&cstr, (const sxe_cstr *)"-01-01"), (const sxe_cstr *)"1970-01-01"), "Short concatenation works");
    ok(!sxe_cstr_is_overflow(&cstr),                                                     "No overflow on short concatenation");
    is_eq(sxe_cstr_get_str(sxe_cstr_cat(&cstr, &cstr)), "1970-01-011970-",               "Long self concatenation works");
    ok(sxe_cstr_is_overflow(&cstr),                                                      "Overflow on long self concatenation");
    is_eq("hello", sxe_cstr_get_str((const sxe_cstr *)"hello"),                          "get_str of literal CSTR");
    is(sxe_cstr_length((const sxe_cstr *)"hello"), 5,                                    "length of liteal CSTR 'hello' is 5");
    sxe_cstr_clear(&cstr);
    is(sxe_cstr_printf(&cstr, "Short string"), 12,                                       "Printf string is 12 characters");

    return exit_status();
}
