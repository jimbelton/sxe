# Wrapper makefile for the third party package: lua

LIB_DEPENDENCIES = sxe-log
LIBRARIES        = lua
THIRD_PARTY.dir  = lua

include ../dependencies.mak

CFLAGS	+= -Ilua
CFLAGS	:= $(filter-out -Wcast-qual,$(CFLAGS))

SRC=	lapi.c lcode.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c \
	lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c \
	ltable.c ltm.c lundump.c lvm.c lzio.c \
	lauxlib.c lbaselib.c ldblib.c liolib.c lmathlib.c loslib.c \
	ltablib.c lstrlib.c loadlib.c linit.c

HDRS=	lapi.h lauxlib.h lcode.h ldebug.h ldo.h lfunc.h lgc.h llex.h \
	llimits.h lmem.h lobject.h lopcodes.h lparser.h lstate.h \
	lstring.h ltable.h ltm.h luaconf.h lua.h lualib.h lundump.h \
	lvm.h lzio.h

# $(info make: TODO: make coverage test should not complain about lack of .c.gcov files, since there are no tests being run)

release debug coverage:		$(HDRS)

clean::
	$(DEL) $(HDRS)                        $(TO_NUL) $(FAKE_PASS)

$(DST.dir)/lua$(EXT.lib):	$(addprefix $(DST.dir)/$(THIRD_PARTY.dir)/, $(SRC:%.c=%$(EXT.obj)))
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(LIB_CMD) $(LIB_FLAGS) $(LIB_OUT)$@ $^

$(DST.dir)/$(THIRD_PARTY.dir)/%$(EXT.obj):	lua/%.c
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(CC) $(CFLAGS) $< $(CC_OUT)$@

# $(info make: TODO: update dep. gen. so that temp. files, e.g. ev.h, get copied into $(DST.dir) (and not ./) folder)

%.h:	lua/%.h
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(COPY) $(call OSPATH,$^) $@

