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

/* Test stuff common to all types of SXEs */

#include "sxe.h"
#include "tap.h"

int
main(void)
{
    SXE * first;
    SXE * la_st;
    SXE * sw_ap;

    plan_tests(10);
    sxe_register(2, 0);
    is(sxe_init(), SXE_RETURN_OK,                                  "SXE initialized successfully");
    ok((first = sxe_new_udp(NULL, "INADDR_ANY", 0, NULL)) != NULL, "Allocated the first SXE");
    ok((la_st = sxe_new_udp(NULL, "INADDR_ANY", 0, NULL)) != NULL, "Allocated the la_st SXE");
    ok(sxe_is_valid(first),                                        "First SXE is valid");
    ok(sxe_is_valid(la_st),                                        "La_st SXE is valid");

    if (first > la_st) {
        sw_ap = la_st;
        la_st = first;
        first = sw_ap;
        diag("Sw_apped the first and the la_st");
    }

    ok(!sxe_is_valid(NULL),                                        "NULL is not a valid SXE");
    ok(!sxe_is_valid(first - 1),                                   "(first - 1) is not valid");
    ok(!sxe_is_valid(la_st + 1),                                   "(la_st + 1) is not valid");
    ok(!sxe_is_valid((SXE *)~0UL),                                 "~NULL is not a valid SXE");
    ok(!sxe_is_valid((SXE *)((char *)la_st - sizeof(SXE) / 2)),    "Missaligned point is not a valid SXE");

    return exit_status();
}
