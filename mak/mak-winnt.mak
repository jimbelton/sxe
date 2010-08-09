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
CC_DEF     = /D
CC_OUT     = /Fo
CC_INC     = /I

# For /DFD_SETSIZE=2048 see http://msdn.microsoft.com/en-us/library/6e3b887c.aspx
CFLAGS     = /c /WX /nologo /Fd$(DST.dir)/vc60.pdb /DWINDOWS_NT=1 /DWIN32 /DFD_SETSIZE=2048
ifneq ($(filter debug,$(MAKECMDGOALS)),)
CFLAGS    += /MTd
else
CFLAGS    += /MT
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
COPYDIR    	= xcopy /I /S /D /F /Y

COV_CFLAGS 	=
COV_LFLAGS 	=
COV_INIT   	=
COV_REAP   	=
CXX         = cl.exe
LINK       	= cl.exe
LINK_OUT   	= /Fe
LINK_FLAGS += /nologo /link /DEFAULTLIB:Ws2_32.lib /NODEFAULTLIB:libc.lib /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcd.lib /NODEFAULTLIB:msvcrtd.lib
ifeq ($(RELEASE_TYPE),debug)
LINK_FLAGS += /NODEFAULTLIB:libcmt.lib  /DEFAULTLIB:LIBCMTD.lib
else
LINK_FLAGS += /NODEFAULTLIB:libcmtd.lib /DEFAULTLIB:LIBCMT.lib
endif
# The following attempt to use static linking doesn't quite work
# LINK_FLAGS += /nologo /link /DEFAULTLIB:Ws2_32.lib /NODEFAULTLIB:MSVCRT.lib /DEFAULTLIB:LIBCMT.lib
LIB_CMD    	= lib.exe
LIB_OUT    	= /OUT:
LIB_FLAGS  	= /nologo
OS_class   	= any-winnt
OS_name	   	= winnt
OS_bits     = $(shell echo %PROCESSOR_ARCHITEW6432% | $(PERL) -lane "$$o.=$$_;sub END{printf qq[%%d], $$o =~ m~64~s ? 64 : 32;}")

OSQUOTE    	= "
OSPC       	= %%
ECHOQUOTE  	=
ECHOESCAPE 	= ^
CAT        	= more
TRUE       	= dir > NUL:
PROVE      	= prove

# - Function to fix a path name for the OS
#   - e.g. $(COPY) $(call OSPATH,$^) $@
OSPATH      = $(subst /,\,$(1))

ifndef VCINSTALLDIR
ifndef MSVCDir
$(error make: please run vcvars32.bat)
endif
endif
