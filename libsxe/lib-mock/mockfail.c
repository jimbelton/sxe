#include "mockfail.h"

#if SXE_DEBUG || SXE_COVERAGE
const void *mockfail_failaddr;
unsigned    mockfail_failfreq;
unsigned    mockfail_failnum;
#endif
