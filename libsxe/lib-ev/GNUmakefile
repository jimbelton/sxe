# Wrapper makefile for the third party package: libev

LIB_DEPENDENCIES = sxe-log
LIBRARIES        = ev
THIRD_PARTY.dir  = libev

include ../dependencies.mak

ifeq ($(OS),Windows_NT)
# Compilation flags which sound good but are not found in the activestate build log for EV
#CFLAGS += -DDEV_SELECT_IS_WINSOCKET=1
# Compilation flags not found in the activestate build log for EV
CFLAGS += -DEV_STANDALONE=1
CFLAGS += -D_USE_32BIT_TIME_T
CFLAGS += -D_CRT_NONSTDC_NO_DEPRECATE
CFLAGS += -D_CRT_SECURE_NO_DEPRECATE
CFLAGS += -D_CRT_SECURE_NO_WARNINGS
# Compilation flags inspired by:
# http://ppm4.activestate.com/MSWin32-x86/5.10/1000/M/ML/MLEHMANN/EV-3.8.d/log-20090811T021343.txt
# and
# http://ppm4.activestate.com/MSWin32-x64/5.10/1000/M/ML/MLEHMANN/EV-3.8.d/log-20090810T125825.txt
CFLAGS += -GF
CFLAGS += -W3
#Note: using /MT or /MTd instead: CFLAGS += -MD
CFLAGS += -Zi
CFLAGS += -DDEBUG
#CFLAGS += -DNDEBUG
CFLAGS += -O1
CFLAGS += -DWIN32
CFLAGS += -D_CONSOLE
CFLAGS += -DHAVE_DES_FCRYPT
CFLAGS += -DVERSION=\"3.8\"
CFLAGS += -DEV_USE_MONOTONIC=1
CFLAGS += -DEV_USE_REALTIME=0
CFLAGS += -DEV_USE_SELECT=1
CFLAGS += -DEV_USE_POLL=0
CFLAGS += -DEV_USE_EPOLL=0
CFLAGS += -DEV_USE_KQUEUE=0
CFLAGS += -DEV_USE_PORT=0
CFLAGS += -DEV_USE_INOTIFY=0
CFLAGS += -DEV_USE_EVENTFD=0
CFLAGS += -DEV_USE_SIGNALFD=0
endif

$(info make: TODO: make coverage test should not complain about lack of .c.gcov files, since there are no tests being run)

release debug coverage:		ev.h

clean::
	$(DEL) ev.h                        $(TO_NUL) $(FAKE_PASS)

$(info make: TODO: use Perl EV module test suite to test our freshly built libev)

ifeq ($(OS),Windows_NT)

# On Windows, we make it ourselves

$(DST.dir)/ev$(EXT.lib):	$(DST.dir)/ev$(EXT.obj)
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(LIB_CMD) $(LIB_FLAGS) $(LIB_OUT)$@ $^

$(DST.dir)/ev$(EXT.obj):	$(DST.dir)/$(THIRD_PARTY.dir)/ev.c ev.h
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(CC) $(CFLAGS) $< $(CC_OUT)$@

else

# On UNIX like OSs, use autoconf

$(DST.dir)/ev$(EXT.lib):	$(DST.dir)/$(THIRD_PARTY.dir)/.libs/libev$(EXT.lib)
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(COPY) $^ $@

$(DST.dir)/$(THIRD_PARTY.dir)/.libs/libev$(EXT.lib):   $(DST.dir)/$(THIRD_PARTY.dir)/Makefile
	@$(MAKE_PERL_ECHO) "make: building: $@"
	@$(COPY) ../lib-sxe-log/sxe-log.h                  $(DST.dir)/$(THIRD_PARTY.dir)
	@$(COPY) ../lib-sxe-log/$(DST.dir)/sxe-*-proto.h   $(DST.dir)/$(THIRD_PARTY.dir)
	@$(MAKE) -C $(DST.dir)/$(THIRD_PARTY.dir)

$(DST.dir)/$(THIRD_PARTY.dir)/Makefile:			$(DST.dir)/$(THIRD_PARTY.dir)/configure
	@$(MAKE_PERL_ECHO) "make: building: $@"
	@cd $(DST.dir)/$(THIRD_PARTY.dir) && ./configure

endif

$(info make: TODO: update dep. gen. so that temp. files, e.g. ev.h, get copied into $(DST.dir) (and not ./) folder)

ev.h:				$(DST.dir)/$(THIRD_PARTY.dir)/ev.h
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(COPY) $(call OSPATH,$^) $@

