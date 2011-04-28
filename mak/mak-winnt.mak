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

PERL       = perl

#
# Note: Only use MSVC, native NT command prompt, or $(PERL) commands below
#
#

MKDIR      = mkdir
PWD        = $(PERL) -MCwd=cwd -e "print cwd"
TOUCH      = $(PERL) -e "open FH, '>>', $$_ foreach @ARGV"
DEL        = del /f
DIR_SEP    = \\
RMDIR      = $(PERL) -MFile::Path -e "foreach(@ARGV){rmtree($$_)};"
REDIRECT   = 2>&1
TO_NUL     = > NUL: 2> NUL:
FAKE_PASS  = || dir > NUL:
EXT.lib    = .lib
EXT.obj    = .obj
EXT.dll    = .dll
EXT.exe    = .exe
CC         = cl.exe
CC_DEF     = -D
CC_OUT     = -Fo
CC_INC     = -I

# For /DFD_SETSIZE=2048 see http://msdn.microsoft.com/en-us/library/6e3b887c.aspx
CFLAGS.base  = -c -DWINDOWS_NT=1 -DWIN32 -DFD_SETSIZE=2048 -Dinline=__inline
CFLAGS.more  = -WX -nologo -Z7
ifneq ($(filter debug,$(MAKECMDGOALS)),)
CFLAGS.more += -MTd
else
CFLAGS.more += -MT
endif
CFLAGS_DEBUG =
COPY       = copy /y

# - e.g. $(COPYDIR) folder1 folder2
#   - /I           If destination does not exist and copying more than one file,
#                  assumes that destination must be a directory.
#   - /S           Copies directories and subdirectories except empty ones.
#   - /D:m-d-y     Copies files changed on or after the specified date.
#                  If no date is given, copies only those files whose
#                  source time is newer than the destination time.
#   - /F           Displays full source and destination file names while copying.
#   - /Y           Suppresses prompting to confirm you want to overwrite an
#                  existing destination file.
COPYDIR       = xcopy /D /F /Y /I /S

define COPYFILES2DIR
$(PERL) -e $(OSQUOTE) \
	exit copyfiles2dir_sub ( @ARGV ); \
	$(COPYFILES2DIR_SUB) \
	$(OSQUOTE)
endef

COV_CFLAGS 	=
COV_LFLAGS 	=
COV_INIT   	=
CXX         = cl.exe
LINK       	= link.exe
LINK_CHECK 	= $(LINK) --version | $(PERL) -lane "$$lines .= $$_; sub END{die qq[make[$(MAKELEVEL)]: ERROR: link.exe in path is not from Microsoft; need to delete your GNU win32 link.exe? ($$lines)] if($$lines !~ m~microsoft~is);}" &&
LINK_OUT   	= /OUT:
LINK_FLAGS += /nologo /DEBUG /PDB:$@.pdb /DEFAULTLIB:Ws2_32.lib /NODEFAULTLIB:libc.lib /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcd.lib /NODEFAULTLIB:msvcrtd.lib
ifneq ($(filter debug,$(MAKECMDGOALS)),)
LINK_FLAGS += /NODEFAULTLIB:libcmt.lib  /DEFAULTLIB:LIBCMTD.lib
# See http://zeuxcg.org/2010/11/22/z7-everything-old-is-new-again/
LINK_FLAGS += /DEBUG /PDB:$@.pdb
else
LINK_FLAGS += /NODEFAULTLIB:libcmtd.lib /DEFAULTLIB:LIBCMT.lib
endif
# The following attempt to use static linking doesn't quite work
# LINK_FLAGS += /nologo /link /DEFAULTLIB:Ws2_32.lib /NODEFAULTLIB:MSVCRT.lib /DEFAULTLIB:LIBCMT.lib
LIB_CMD    	= $(MAKE_PERL_LIB)
LIB_OUT    	= /OUT:
LIB_FLAGS  	= /nologo
OS_class   	= any-winnt
OS_name	   	= winnt
OS_bits     = $(shell echo %PROCESSOR_ARCHITEW6432% | $(PERL) -lane "$$o.=$$_;sub END{printf qq[%%d], $$o =~ m~64~s ? 64 : 32;}")
TEST_ENV_VARS   =

OSQUOTE    	= "
OSPC       	= %%
ECHOQUOTE  	=
ECHOESCAPE 	= ^
CAT        	= type
TRUE       	= dir > NUL:
PROVE      	= prove

##
# Function to fix a path name for the OS
#   - e.g. $(COPY) $(call OSPATH,$^) $@
OSPATH      = $(subst /,\,$(1))

##
# Function to copy one or more files to a directory
#   - e.g. $(call COPY_TO_DIR,$source_files,$dest_dir)
COPY_TO_DIR = $(TOP.dir)/mak/bin/cp.pl -f $(1) $(2)

ifneq "$(VCINSTALLDIR)$(MSVCDir)" ""
# come here if    ms visual c installed / vcvars32.bat     run
MAKE_MSVC   = inpath
else
# come here if no ms visual c installed / vcvars32.bat not run
endif

ifneq ($(filter coverage,$(MAKECMDGOALS)),)
ifeq "$(MAKE_MSVC)" "inpath"
# come here if    ms visual c installed / vcvars32.bat     run
ifeq "$(MAKELEVEL)" "0"
$(info make[$(MAKELEVEL)]: note:     special case: winnt coverage build: fake that MSVC is uninstalled in order to default to MinGW)
endif
MAKE_MSVC   = specialcase
endif
endif

ifeq "$(MAKE_MSVC)" "inpath"
# come here if    ms visual c installed / vcvars32.bat     run
else
# come here if no ms visual c installed / vcvars32.bat not run
MINGW_DETECTED := $(shell gcc.exe --version 2>&1 | $(PERL) -lane "$$lines .= $$_; sub END{ printf qq[%%d], $$lines =~ m~gcc~is; })
ifneq ($(filter 1,$(MINGW_DETECTED)),)
# come here if gcc.exe found
MAKE_MINGW = 1
else
# come here if no compiler found
$(info make[$(MAKELEVEL)]: note:     install 32bit MinGW bundle from http://tdm-gcc.tdragon.net/download)
$(info make[$(MAKELEVEL)]: error:    neither mingw32-gcc.exe nor cl.exe found; please add MinGW to path or run vcvars32.bat)
$(error make[$(MAKELEVEL)]: error: see above)
endif
endif

ifdef MAKE_MINGW
ifdef MAKE_DEBUG
$(info make[$(MAKELEVEL)]: want mingw build, remapping compiler options)
endif
ifeq "$(MAKE_MSVC)" "specialcase"
# come here if using MinGW to do the coverage build instead of cl.exe
else
OS_name     = mingw
endif
EXT.lib     = .a
CXX         = gcc.exe
CC          = gcc.exe
CC_OUT      = -o
CFLAGS_PROD = -std=gnu99
CFLAGS_TEST = -std=gnu99

# See http://gcc.gnu.org/onlinedocs/gcc-3.0/gcc_8.html#SEC135
# "...but if you want to prove that every single line in your program
#  was executed, you should not compile with optimization at the same
#  time."
CFLAGS.more = -DMAKE_MINGW -g \
              -W -Waggregate-return -Wall -Werror -Wcast-align -Wcast-qual -Wchar-subscripts -Wcomment \
              -Wformat -Wimplicit  -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wparentheses \
              -Wpointer-arith -Wredundant-decls -Wreturn-type -Wshadow -Wstrict-prototypes -Wswitch \
              -Wtrigraphs -Wunused -Wwrite-strings

# See http://gcc.gnu.org/onlinedocs/gcc-3.0/gcc_8.html#SEC135
# "...but if you want to prove that every single line in your program
#  was executed, you should not compile with optimization at the same
#  time."
ifneq ($(filter coverage,$(MAKECMDGOALS)),)
CFLAGS.more += -O0
else
CFLAGS.more +=  -O -Wuninitialized
endif

LINK        = gcc.exe
LINK_CHECK  =
LINK_OUT    = -o
LINK_FLAGS  = -g -lm -lWs2_32
COV_CFLAGS  = -fprofile-arcs -ftest-coverage
COV_LFLAGS  = -coverage -lgcov
COV_INIT    = $(DEL) $(call OSPATH,$(DST.dir)/*.gcda $(DST.dir)/*.ok)
LIB_OUT     =
LIB_FLAGS   =

else # VS/CL

CFLAGS.more += -D__func__=__FUNCTION__

endif

CFLAGS += $(CFLAGS.base) $(CFLAGS.more) $(CFLAGS_EXTRA)
