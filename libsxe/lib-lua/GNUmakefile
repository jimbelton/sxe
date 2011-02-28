# Wrapper makefile for the third party package: lua

LIB_DEPENDENCIES = sxe-log
LIBRARIES        = lua
THIRD_PARTY.dir  = lua

include ../dependencies.mak

CFLAGS	+= -Ilua
CFLAGS	:= $(filter-out -Wcast-qual,$(CFLAGS))

SRC= \
	lapi.c lcode.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c \
	lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c \
	ltable.c ltm.c lundump.c lvm.c lzio.c \
	lauxlib.c lbaselib.c ldblib.c liolib.c lmathlib.c loslib.c \
	ltablib.c lstrlib.c loadlib.c linit.c

HDRS= \
	lapi.h lauxlib.h lcode.h ldebug.h ldo.h lfunc.h lgc.h llex.h \
	llimits.h lmem.h lobject.h lopcodes.h lparser.h lstate.h \
	lstring.h ltable.h ltm.h luaconf.h lua.h lualib.h lundump.h \
	lvm.h lzio.h

$(info make[$(MAKELEVEL)]: todo:     somehow test our freshly built $(THIRD_PARTY.dir))

$(DST.dir)/lua$(EXT.lib):	$(addprefix $(DST.dir)/$(THIRD_PARTY.dir)/, $(SRC:%.c=%$(EXT.obj)))
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	$(MAKE_RUN) $(LIB_CMD) $(LIB_FLAGS) $(LIB_OUT)$@ $^
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: updating: $(DST.dir)/*.h"
	$(MAKE_RUN) $(COPYFILES2DIR) $(addprefix $(THIRD_PARTY.dir)/, $(HDRS)) $(DST.dir)
