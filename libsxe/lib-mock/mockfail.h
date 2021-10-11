#ifndef MOCKFAIL_H
#define MOCKFAIL_H

#if !SXE_DEBUG && !SXE_COVERAGE    // In the release build, remove all mock scaffolding

#define MOCKFAIL(addr, ret, expr)     expr
#define MOCKFAIL_START_TESTS(n, addr) skip_start(1, n, "MOCKFAIL: Skipping %d test%s for release build", n, (n) == 1 ? "" : "s")
#define MOCKFAIL_SET_FREQ(n)
#define MOCKFAIL_END_TESTS()          skip_end

#else    // In the debug and coverage builds, mock failures can be triggered

#define MOCKFAIL(addr, ret, expr) (((addr) == mockfail_failaddr && !--mockfail_failnum) ? ((mockfail_failnum = mockfail_failfreq), ret) : expr)
#define MOCKFAIL_START_TESTS(n, addr) do { mockfail_failaddr = (addr); mockfail_failfreq = mockfail_failnum = 1; } while (0)
#define MOCKFAIL_SET_FREQ(n)          do { mockfail_failfreq = mockfail_failnum = n; } while (0)
#define MOCKFAIL_END_TESTS()          do { mockfail_failaddr = NULL; } while (0)

extern const void *mockfail_failaddr;
extern unsigned    mockfail_failfreq;
extern unsigned    mockfail_failnum;
#endif

/*
 * We can change a piece of code such as
 *
 *     if (almost_impossible_failure(args) == hard_to_make_fail) {
 *         really_hard_to_test();
 *     }
 *
 * to
 *
 *     #include "mockfail.h"
 *
 *     if (MOCKFAIL(UNIQUE_ADDRESS, hard_to_make_fail, almost_impossible_failure(args)) == hard_to_make_fail) {
 *         really_hard_to_test();
 *     }
 *
 * and test with:
 *
 *     #include "mockfail.h"
 *
 *     MOCKFAIL_START_TESTS(3, UNIQUE_ADDR);
 *     ok(!some_caller_of_almost_impossible_failure(), "caller fails because allmost_impossible_failure() occurred");
 *     ok(!another_caller(), "caller fails because allmost_impossible_failure() occurred");
 *     MOCKFAIL_SET_FREQ(3);
 *     ok(some_caller_of_almost_impossible_failure(), "caller succeeds because allmost_impossible_failure() only fails every third time now");
 *     MOCKFAIL_END_TESTS();
 *
 * UNIQUE_ADDR can be a global function name or the address of a global variable. It must be a global so that the test can
 * use it. If you are mocking more than one failure in a function or the function is static, you can define tags in the .h:
 *
 *     #if defined(SXE_DEBUG) || defined(SXE_COVERAGE)    // Define unique tags for mockfails
 *     #   define APPLICATION_CLONE             ((const char *)application_register_resolver + 0)
 *     #   define APPLICATION_CLONE_DOMAINLISTS ((const char *)application_register_resolver + 1)
 *     #   define APPLICATION_MOREDOMAINLISTS   ((const char *)application_register_resolver + 2)
 *     #endif
 */

#endif
