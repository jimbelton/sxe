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

ifeq ($(OS),Windows_NT)

# On Windows, we make it ourselves

EVCFLAGS = $(CFLAGS)

# Compilation flags which sound good but are not found in the activestate build log for EV
#EVCFLAGS += -DDEV_SELECT_IS_WINSOCKET=1
# Compilation flags not found in the activestate build log for EV
EVCFLAGS += -DEV_STANDALONE=1
EVCFLAGS += -D_USE_32BIT_TIME_T
EVCFLAGS += -D_CRT_NONSTDC_NO_DEPRECATE
EVCFLAGS += -D_CRT_SECURE_NO_DEPRECATE
EVCFLAGS += -D_CRT_SECURE_NO_WARNINGS
# Compilation flags inspired by:
# http://ppm4.activestate.com/MSWin32-x86/5.10/1000/M/ML/MLEHMANN/EV-3.8.d/log-20090811T021343.txt
# and
# http://ppm4.activestate.com/MSWin32-x64/5.10/1000/M/ML/MLEHMANN/EV-3.8.d/log-20090810T125825.txt
EVCFLAGS += -GF
EVCFLAGS += -W3
#Note: using /MT or /MTd instead: CFLAGS += -MD
EVCFLAGS += -Zi
EVCFLAGS += -DDEBUG
#EVCFLAGS += -DNDEBUG
EVCFLAGS += -O1
EVCFLAGS += -DWIN32
EVCFLAGS += -D_CONSOLE
EVCFLAGS += -DHAVE_DES_FCRYPT
EVCFLAGS += -DVERSION=\"3.8\"
EVCFLAGS += -DEV_USE_MONOTONIC=1
EVCFLAGS += -DEV_USE_REALTIME=0
EVCFLAGS += -DEV_USE_SELECT=1
EVCFLAGS += -DEV_USE_POLL=0
EVCFLAGS += -DEV_USE_EPOLL=0
EVCFLAGS += -DEV_USE_KQUEUE=0
EVCFLAGS += -DEV_USE_PORT=0
EVCFLAGS += -DEV_USE_INOTIFY=0
EVCFLAGS += -DEV_USE_EVENTFD=0
EVCFLAGS += -DEV_USE_SIGNALFD=0
EVCFLAGS += -Uinline               # Gorp: libev/ev.c wants to define inline to *nothing*, so nuke our definition

THIRDPARTY_LIB_EV_OBJECTS = $(MAKE.dir)/$(DST.dir)/libsxe/lib-ev/libev/ev$(EXT.obj)

$(MAKE.dir)/$(DST.dir)/libsxe/lib-ev/libev/ev$(EXT.obj): \
    $(TOP.dir)/libsxe/lib-ev/libev/ev.c \
    $(TOP.dir)/libsxe/lib-ev/libev/ev.h
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(COPYDIR) $(call OSPATH,$(TOP.dir)/libsxe/lib-ev) $(call OSPATH,$(MAKE.dir)/$(DST.dir)/libsxe/lib-ev)
	$(CC) $(EVCFLAGS) $(CC_INC)$(MAKE.dir)/$(DST.dir)/libsxe $< $(CC_OUT)$(MAKE.dir)/$(DST.dir)/libsxe/lib-ev/libev/ev$(EXT.obj)

else

THIRDPARTY_LIB_EV_OBJECTS = $(MAKE.dir)/$(DST.dir)/libsxe/lib-ev/libev/ev$(EXT.obj) $(MAKE.dir)/$(DST.dir)/libsxe/lib-ev/libev/event$(EXT.obj)

$(MAKE.dir)/$(DST.dir)/libsxe/lib-ev/libev/ev$(EXT.obj): \
    $(MAKE.dir)/$(DST.dir)/libsxe/sxe-log.h \
    $(MAKE.dir)/$(DST.dir)/libsxe/lib-ev/libev/Makefile \
    $(TOP.dir)/libsxe/lib-ev/libev/ev.c \
    $(TOP.dir)/libsxe/lib-ev/libev/ev.h
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(COPYDIR) $(call OSPATH,$(TOP.dir)/libsxe/lib-ev) $(call OSPATH,$(MAKE.dir)/$(DST.dir)/libsxe/lib-ev)
	@$(COPY) $(MAKE.dir)/$(DST.dir)/libsxe/sxe-log.h      $(MAKE.dir)/$(DST.dir)/libsxe/lib-ev/libev
	@$(COPY) $(MAKE.dir)/$(DST.dir)/libsxe/sxe-*-proto.h  $(MAKE.dir)/$(DST.dir)/libsxe/lib-ev/libev
	@$(MAKE) -C $(MAKE.dir)/$(DST.dir)/libsxe/lib-ev/libev

$(MAKE.dir)/$(DST.dir)/libsxe/lib-ev/libev/Makefile:
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(COPYDIR) $(call OSPATH,$(TOP.dir)/libsxe/lib-ev) $(call OSPATH,$(MAKE.dir)/$(DST.dir)/libsxe/lib-ev)
	@cd $(MAKE.dir)/$(DST.dir)/libsxe/lib-ev/libev && ./configure

endif
