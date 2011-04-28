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

# Add GCC flags used on all platforms.
#
CFLAGS+=-c -g -W -Waggregate-return -Wall -Werror -Wcast-align -Wcast-qual -Wchar-subscripts -Wcomment                   \
		-Wformat -Wimplicit  -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wparentheses -Wpointer-arith     \
		-Wredundant-decls -Wreturn-type -Wshadow -Wstrict-prototypes -Wswitch  -Wtrigraphs         \
		-Wwrite-strings $(CFLAGS_EXTRA)

# See http://gcc.gnu.org/onlinedocs/gcc-3.0/gcc_8.html#SEC135
# "...but if you want to prove that every single line in your program
#  was executed, you should not compile with optimization at the same
#  time."
ifneq ($(filter coverage,$(MAKECMDGOALS)),)
CFLAGS += -O0 -Wunused
else ifneq ($(filter debug,$(MAKECMDGOALS)),)
CFLAGS += -O0 -Wno-unused
else
CFLAGS += -O -Wuninitialized -Wunused
endif

# FreeBSD platform specific compiler flags.
# Additional to Tank flags: -O, pedantic, std=gnu99 Waggregate-return Wall Wcomment Wformat Wimplicit Wmissing-declarations
#                           Wparentheses Wtrigraphs Wuninitialized Wunused
# Removed  from Tank flags: -Wno-format-y2k -Wno-unused-parameter
#
ifeq ($(OS), freebsd)
CFLAGS+=        -Wsystem-headers

# Linux: extra flag in GCC 4
#
else
# Once we upgrade to Lenny, we can add the -fstack-protector-all flag back in
#CFLAGS+=        -fstack-protector-all
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
CC                 = gcc
CC_DEF             = -D
CC_OUT             = -o
CC_INC             = -I
#CFLAGS_PROD        = -std=c89 -Winline # for now, use the test flags.
CFLAGS_PROD        = -std=gnu99
CFLAGS_TEST        = -std=gnu99
COPY               = cp -f
COV_CFLAGS         = -fprofile-arcs -ftest-coverage
COV_LFLAGS         = -coverage -lgcov
COV_INIT           = $(DEL) $(DST.dir)/*.gcda $(DST.dir)/*.ok
CXX                = g++
LINK               = gcc
LINK_OUT           = -o $(EMPTY)
LINK_FLAGS        += -ldl
# -lm needed by ev; ceil()
LINK_FLAGS        += -g -lm
LIB_CMD            = $(MAKE_PERL_LIB)
LIB_OUT            =
LIB_FLAGS          =
OSQUOTE            = '
OSPC               = %
OS_class		   = any-unix
OS_name            = $(if $(findstring el5,$(shell uname -r)),rhes5,$(shell uname -s | tr '[:upper:]' '[:lower:]'))
OS_bits			   = $(shell uname -a | $(PERL) -lane '$$o.=$$_;sub END{printf qq[%d], $$o =~ m~_64~s ? 64 : 32;}')
TEST_ENV_VARS      = LIBC_FATAL_STDERR_=1


# note: -march=i486 for __sync_val_compare_and_swap gcc builtin used by sxe-mmap
ifeq ($(OS_name), darwin)
	CFLAGS		  += -march=core2 -pthread
	OS_bits		  = 64
else ifeq ($(OS_bits), 32)
	CFLAGS		  += -march=i486
else
# x86-64 for 64 bit systems
	CFLAGS		  += -march=x86-64
endif

# syntax engine workaround ' (a.k.a. VIM)
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
ifeq ($(OS_name), darwin)
COPYDIR            = cp -pPRfv
COPYFILES2DIR      = cp -pPfv
else
COPYDIR            = cp --update --verbose --preserve=timestamps --recursive --no-target-directory --force
COPYFILES2DIR      = cp --update --verbose --preserve=timestamps --force
endif
CFLAGS_DEBUG       = -g
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
