# Copyright (c) 2010 Sophos Group.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

OSQUOTE            = '
OSPC               = %
OS_class           = any-unix
OS_name            = $(if $(findstring CentOS,$(shell cat /etc/redhat-release 2>/dev/null)),centos56,$(if $(findstring el5,$(shell uname -r)),rhes53,$(shell uname -s | tr '[:upper:]' '[:lower:]')))
OS_bits            = $(shell uname -a | $(PERL) -lane '$$o.=$$_;sub END{printf qq[%d], $$o =~ m~(amd|_)64~s ? 64 : 32;}')

# Add GCC flags used on all platforms.
#

# todo: change libsxe to compile with the stricter CFLAGS_WFORMAT = -Wformat=2

CFLAGS_W                     ?= -W
CFLAGS_WAGGREGATE_RETURN     ?= -Waggregate-return
CFLAGS_WALL                  ?= -Wall
CFLAGS_WERROR                ?= -Werror
ifneq ($(OS_name), freebsd)
CFLAGS_WCAST_ALIGN           ?= -Wcast-align
endif
CFLAGS_WCAST_QUAL            ?= -Wcast-qual
CFLAGS_WCHAR_SUBSCRIPTS      ?= -Wchar-subscripts
CFLAGS_WCOMMENT              ?= -Wcomment
CFLAGS_WFORMAT               ?= -Wformat
CFLAGS_WIMPLICIT             ?= -Wimplicit
CFLAGS_WMISSING_DECLARATIONS ?= -Wmissing-declarations
CFLAGS_WMISSING_PROTOTYPES   ?= -Wmissing-prototypes
CFLAGS_WNESTED_EXTERNS       ?= -Wnested-externs
CFLAGS_WPARENTHESES          ?= -Wparentheses
CFLAGS_WPOINTER_ARITH        ?= -Wpointer-arith
CFLAGS_WREDUNDANT_DECLS      ?= -Wredundant-decls
CFLAGS_WRETURN_TYPE          ?= -Wreturn-type
CFLAGS_WSHADOW               ?= -Wshadow
CFLAGS_WSTRICT_PROTOTYPES    ?= -Wstrict-prototypes
CFLAGS_WSWITCH               ?= -Wswitch
CFLAGS_WTRIGRAPHS            ?= -Wtrigraphs
CFLAGS_WUNINITIALIZED        ?= -Wuninitialized
CFLAGS_WUNUSED               ?= -Wunused
CFLAGS_WWRITE_STRINGS        ?= -Wwrite-strings

CFLAGS            += -c -g                           \
                     $(CFLAGS_W)                     \
                     $(CFLAGS_WAGGREGATE_RETURN)     \
                     $(CFLAGS_WALL)                  \
                     $(CFLAGS_WERROR)                \
                     $(CFLAGS_WCAST_ALIGN)           \
                     $(CFLAGS_WCAST_QUAL)            \
                     $(CFLAGS_WCHAR_SUBSCRIPTS)      \
                     $(CFLAGS_WCOMMENT)              \
                     $(CFLAGS_WFORMAT)               \
                     $(CFLAGS_WIMPLICIT)             \
                     $(CFLAGS_WMISSING_DECLARATIONS) \
                     $(CFLAGS_WMISSING_PROTOTYPES)   \
                     $(CFLAGS_WNESTED_EXTERNS)       \
                     $(CFLAGS_WPARENTHESES)          \
                     $(CFLAGS_WPOINTER_ARITH)        \
                     $(CFLAGS_WREDUNDANT_DECLS)      \
                     $(CFLAGS_WRETURN_TYPE)          \
                     $(CFLAGS_WSHADOW)               \
                     $(CFLAGS_WSTRICT_PROTOTYPES)    \
                     $(CFLAGS_WSWITCH)               \
                     $(CFLAGS_WTRIGRAPHS)            \
                     $(CFLAGS_WWRITE_STRINGS)        \
                     $(CFLAGS_EXTRA)

CXXFLAGS          += -c -g                           \
                     $(CFLAGS_W)                     \
                     $(CFLAGS_WAGGREGATE_RETURN)     \
                     $(CFLAGS_WALL)                  \
                     $(CFLAGS_WERROR)                \
                     $(CFLAGS_WCAST_ALIGN)           \
                     $(CFLAGS_WCAST_QUAL)            \
                     $(CFLAGS_WCHAR_SUBSCRIPTS)      \
                     $(CFLAGS_WCOMMENT)              \
                     $(CFLAGS_WFORMAT)               \
                     $(CFLAGS_WIMPLICIT)             \
                     $(CFLAGS_WMISSING_DECLARATIONS) \
                     $(CFLAGS_WPARENTHESES)          \
                     $(CFLAGS_WPOINTER_ARITH)        \
                     $(CFLAGS_WREDUNDANT_DECLS)      \
                     $(CFLAGS_WRETURN_TYPE)          \
                     $(CFLAGS_WSHADOW)               \
                     $(CFLAGS_WSWITCH)               \
                     $(CFLAGS_WTRIGRAPHS)            \
                     $(CFLAGS_WWRITE_STRINGS)        \
                     $(CXXFLAGS_EXTRA)

# todo: if there an easier way to make sxe logging work with gcc 4.6.3 than using -fno-inline-functions-called-once -fPIC ?
CFLAGS_OPT_ON      = -O2 -fPIC
CFLAGS_OPT_OFF     = -O0 -fPIC
ifneq ($(OS_name), freebsd)
CFLAGS_OPT_ON     += -fno-inline-functions-called-once -pie -fstack-protector-all
CFLAGS_OPT_OFF    += -pie -fstack-protector-all
endif

LINK_FLAGS        += $(LINK_FLAGS_EXTRA)

# See http://gcc.gnu.org/onlinedocs/gcc-3.0/gcc_8.html#SEC135
# "...but if you want to prove that every single line in your program
#  was executed, you should not compile with optimization at the same
#  time."
#     ifneq ($(filter coverage,$(MAKECMDGOALS)),)
#     CFLAGS            += $(CFLAGS_OPT_OFF) $(CFLAGS_WUNUSED)
#     CXXFLAGS          += $(CFLAGS_OPT_OFF) $(CFLAGS_WUNUSED)
#     else ifneq ($(filter debug,$(MAKECMDGOALS)),)
#     CFLAGS            += $(CFLAGS_OPT_OFF) $(CFLAGS_WUNUSED:-W%=-Wno-%)
#     CXXFLAGS          += $(CFLAGS_OPT_OFF) $(CFLAGS_WUNUSED:-W%=-Wno-%)
#     else
#     CFLAGS            += $(CFLAGS_OPT_ON) $(CFLAGS_WUNINITIALIZED) $(CFLAGS_WUNUSED)
#     CXXFLAGS          += $(CFLAGS_OPT_ON) $(CFLAGS_WUNINITIALIZED) $(CFLAGS_WUNUSED)
#     endif
ifneq ($(filter release,$(MAKECMDGOALS)),)
CFLAGS            += $(CFLAGS_OPT_ON) $(CFLAGS_WUNINITIALIZED) $(CFLAGS_WUNUSED)
CXXFLAGS          += $(CFLAGS_OPT_ON) $(CFLAGS_WUNINITIALIZED) $(CFLAGS_WUNUSED)
else
CFLAGS            += $(CFLAGS_OPT_OFF) $(CFLAGS_WUNINITIALIZED) $(CFLAGS_WUNUSED)
CXXFLAGS          += $(CFLAGS_OPT_OFF) $(CFLAGS_WUNINITIALIZED) $(CFLAGS_WUNUSED)
endif

ifdef SXE_DISABLE_OPENSSL
CFLAGS            += -DSXE_DISABLE_OPENSSL
CXXFLAGS          += -DSXE_DISABLE_OPENSSL
else ifdef SXE_EXTERNAL_OPENSSL
LINK_FLAGS        += -lssl -lcrypto
endif

PERL               = perl
MKDIR              = mkdir -p
PWD                = pwd
TOUCH              = touch
DEL                = rm -f
DIR_SEP            = /
RMDIR              = rm -rf
REDIRECT           = 2>&1
TO_NUL             = >/dev/null 2>&1
FAKE_PASS          =
EXT.lib            = .a
EXT.obj            = .o
EXT.dll            = .so
EXT.exe            =
CC                ?= gcc
CXX               ?= g++
COV               ?= gcov
CC_DEF             = -D
CC_OUT             = -o
CC_INC             = -I
#CFLAGS_PROD        = -std=c89 -Winline # for now, use the test flags.
CFLAGS_PROD        = -std=gnu99
CXXFLAGS_PROD      = -std=c++0x
CFLAGS_TEST        = -std=gnu99
CXXFLAGS_TEST      = -std=c++0x
COPY               = cp -f
COV_CFLAGS         = -fprofile-arcs -ftest-coverage
COV_LFLAGS         = -coverage -lgcov
COV_INIT           = $(DEL) $(DST.dir)/*.gcda $(DST.dir)/*.ok
LINK              ?= $(CC)
LINK_TEST         ?= $(LINK)
LINK_OUT           = -o $(EMPTY)
# -lm needed by ev; ceil()
LINK_FLAGS        += -g -lm
LIB_CMD            = $(MAKE_PERL_LIB)
LIB_OUT            =
LIB_FLAGS          =
LIB_EXTRACT        = ar x
LIB_LIST           = ar t
LIB_LIST_FILTER    = cat
LIB_INDEX          = ranlib
DLL_CMD            = $(MAKE_PERL_DLL)
DLL_OUT            =
DLL_FLAGS          =

# syntax engine workaround ' (a.k.a. VIM)
TEST_ENV_VARS      = LIBC_FATAL_STDERR_=1
CHMOD_R_WRITABLE   = chmod -R +w

MAKE_DEPLOY_TYPE  ?= experimental
CFLAGS            += -DMAKE_DEPLOY_TYPE=\"$(MAKE_DEPLOY_TYPE)\"
CXXFLAGS          += -DMAKE_DEPLOY_TYPE=\"$(MAKE_DEPLOY_TYPE)\"

ifdef MAKE_FORCE_32BIT
CFLAGS_M3264       = -m32
OS_bits            = 32
endif

CC                += $(CFLAGS_M3264)
CXX               += $(CFLAGS_M3264)
LINK              += $(CFLAGS_M3264)

ifneq ($(OS_name), freebsd)
LINK_FLAGS        += -ldl
endif
# FreeBSD platform specific compiler flags.
# Additional to Tank flags: -O, pedantic, std=gnu99 Waggregate-return Wall Wcomment Wformat Wimplicit Wmissing-declarations
#                           Wparentheses Wtrigraphs Wuninitialized Wunused
# Removed  from Tank flags: -Wno-format-y2k -Wno-unused-parameter
#
ifeq ($(OS_name), freebsd)
CFLAGS            += -Wsystem-headers
endif

ifeq ($(OS_name), darwin)
CFLAGS_MARCH       = -march=core2
else ifeq ($(OS_bits), 32)
CFLAGS_MARCH       = -march=i486
else
# for 64 bit systems: x86-64
CFLAGS_MARCH       = -march=x86-64
endif

# note: -march=i486 for __sync_val_compare_and_swap gcc builtin used by sxe-mmap
ifeq ($(OS_name), darwin)
	CFLAGS            += $(CFLAGS_MARCH)
	CXXFLAGS          += $(CFLAGS_MARCH)
	CFLAGS            += -Wno-deprecated-declarations
	CXXFLAGS          += -Wno-deprecated-declarations
	CFLAGS            += -pthread
	CXXFLAGS          += -pthread
	OS_bits            = 64
else ifeq ($(OS_bits), 32)
	CFLAGS            += $(CFLAGS_MARCH)
	CXXFLAGS          += $(CFLAGS_MARCH)
else
# for 64 bit systems
	CFLAGS            += $(CFLAGS_MARCH)
	CXXFLAGS          += $(CFLAGS_MARCH)
endif

ECHOQUOTE          = $(OSQUOTE)
ECHOESCAPE         =
CAT                = cat
TRUE               = true
PROVE              = prove
# - e.g. $(COPYDIR) folder1 folder2
#   - --update              copy only when the SOURCE file is newer than the destination file or when the destination file is missing
#   - --recursive           copy directories recursively
#   - --no-target-directory treat DEST as a normal file
#   - --verbose             explain what is being done
#   - --preserve=timestamps make copies the same age as the sources
# todo: figure out a cross-platform way to exclude folders (e.g. .svn/) from getting copied
CP_UPDATE          = -u
CP_VERBOSE         = -v
CP_PRESERVE        = -p
CP_RECURSIVE       = -R
CP_NO_TARGET_DIR   = -T
CP_FORCE           = -f
CP_NO_DEREFERENCE  = -P
ifeq ($(OS_name), darwin)
COPYDIR            = cp $(CP_PRESERVE) $(CP_NO_DEREFERENCE) $(CP_RECURSIVE) $(CP_FORCE) $(CP_VERBOSE)
COPYFILES2DIR      = cp $(CP_PRESERVE) $(CP_NO_DEREFERENCE) $(CP_FORCE) $(CP_VERBOSE)
else ifeq ($(OS_name), freebsd)
COPYDIR            = cp $(CP_PRESERVE) $(CP_NO_DEREFERENCE) $(CP_RECURSIVE) $(CP_FORCE) $(CP_VERBOSE)
COPYFILES2DIR      = cp $(CP_PRESERVE) $(CP_NO_DEREFERENCE) $(CP_FORCE) $(CP_VERBOSE)
else
COPYDIR            = cp $(CP_UPDATE) $(CP_VERBOSE) $(CP_PRESERVE) $(CP_RECURSIVE) $(CP_NO_TARGET_DIR) $(CP_FORCE)
COPYFILES2DIR      = cp $(CP_UPDATE) $(CP_VERBOSE) $(CP_PRESERVE) $(CP_FORCE)
endif
CFLAGS_DEBUG       = -g
CXXFLAGS_DEBUG     = -g
CFLAGS_FOR_CPP     = -lstdc++

##
# Function to fix a path name for the OS
#   - e.g. $(COPY) $(call OSPATH,$^) $@
#   - On UNIX, this is a no-op
OSPATH             = $(1)

##
# Function to copy one or more files to a directory
#   - e.g. $(call COPY_TO_DIR,$source_files,$dest_dir)
COPY_TO_DIR        = cp -f $(1) $(2)
