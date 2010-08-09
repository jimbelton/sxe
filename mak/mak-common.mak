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

# TODO: Dependency generation for test programs

# usage: mak should be a top level component. The higher level makefile is expected to define:
#   COM.dir                = The directory which is the root of the component being built (usually "." or "..")
#   TOP.dir                = The directory which includes "mak" (usually ".." or "../..")
#   LIB_DEPENDENCIES       = List of all the package names (removing lib-) of the libraries that the package depends on
#   CONVENTION_OPTOUT_LIST = List of the packages to opt out of convention checks (e.g: lib-mock lib-port)
#   COVERAGE_OPTOUT_LIST   = List of the packages to opt out of coverage   checks (e.g: lib-mock lib-port)

ifeq ($(OS),Windows_NT)
    include $(TOP.dir)/mak/mak-winnt.mak
else
    include $(TOP.dir)/mak/mak-unix.mak
endif

# Preserve intermediate files.
#
.SECONDARY:

#
# - Example usage:
#   - @$(MAKE_PERL_ECHO) "make: building: $@"
#

define MAKE_PERL_ECHO
$(PERL) -e $(OSQUOTE) \
	$$text  = shift @ARGV; \
    $$text .= qq[ ]; \
    $$text .= qq[-] x ( 128 - length ($$text) ); \
    printf qq[$(OSPC)s\n], $$text; \
	exit 0; \
	$(OSQUOTE)
endef

define MAKE_PERL_COVERAGE_CHECK
$(PERL) -e $(OSQUOTE) \
	for $$g ( @ARGV ) { \
		next if $$g =~ /debug/; \
		$$f =  `$(CAT) $$g`; \
		if ( $$f =~ m~(/\* FILE COVERAGE EXCLUSION.*?\*/)~is ) { \
			push @x, sprintf q{%s: %s}, $$g, $$1; \
			next; \
		} \
		$$g =~ s~^(.*)/$(DST.dir)/(.*)\.gcov$$~$$1/$$2~; \
		$$f =~ s~^[^\#:]+:.*$$~~gm; \
		$$f =~ s~^\n+~~s; \
		foreach ( split(m~\n{2,}~s,$$f) ) { \
			$$c = sprintf q{%s:%d}, $$g, m~:\s*(\d+)~; \
			if ( m~(/\* COVERAGE EXCLUSION.*?\*/)~is ) { \
				push @x, sprintf q{%s: %s}, $$c, $$1; \
				next; \
			} \
			$$r  = 1; \
			$$o .= sprintf qq{%s: Error: Insufficient coverage: \n$$_\n}, $$c; \
		} \
	} \
	print join(qq{\n},@x) . qq{\n} if @x; \
	printf qq{%sCoverage is %s\n}, $$o, $$r ? q{insufficient} : q{acceptable}; \
	exit $$r; \
	$(OSQUOTE)
endef

ifneq ($(filter release,$(MAKECMDGOALS)),)
    DEBUG    := 0
    COVERAGE := 0
    RELEASE_TYPE := release
    DST.dir  := build-$(OS_name)-$(OS_bits)-$(RELEASE_TYPE)
else ifneq ($(filter debug,$(MAKECMDGOALS)),)
    DEBUG    := 1
    COVERAGE := 0
    RELEASE_TYPE := debug
    DST.dir  := build-$(OS_name)-$(OS_bits)-$(RELEASE_TYPE)
    CFLAGS   += $(CC_DEF)SXE_DEBUG=1 $(CFLAGS_DEBUG)
else ifneq ($(filter coverage,$(MAKECMDGOALS)),)
    DEBUG    := 0
    COVERAGE := 1
    RELEASE_TYPE := release-coverage
    DST.dir  := build-$(OS_name)-$(OS_bits)-release-coverage
endif

ifneq ($(filter release,$(MAKECMDGOALS)),)
    ifneq ($(filter debug,$(MAKECMDGOALS)),)
        $(error make: do not specify both release and debug goals)
    endif
    ifneq ($(filter coverage,$(MAKECMDGOALS)),)
        $(error make: do not specify both release and coverage goals)
    endif
endif

ifneq ($(filter debug,$(MAKECMDGOALS)),)
    ifneq ($(filter coverage,$(MAKECMDGOALS)),)
        $(error make: do not specify both debug and coverage goals)
    endif
endif

ifdef DST.dir
    ifneq ($(filter clean,$(MAKECMDGOALS)),)
        $(error make: error: do not specify clean goal with release, debug or coverage goals)
    endif

    ifneq ($(filter realclean,$(MAKECMDGOALS)),)
        $(error make: error: do not specify realclean goal with release, debug or coverage goals)
    endif

    ifneq ($(COVERAGE),0)
        COVERAGE_LIBS    := $(COV_LFLAGS)
        COVERAGE_CFLAGS  := $(COV_CFLAGS)
        COVERAGE_INIT    := $(COV_INIT)
        COVERAGE_REAP    := $(COV_REAP)
        COVERAGE_CHECK   := $(MAKE_PERL_COVERAGE_CHECK) $(DST.dir)/*.c.gcov
    endif

    $(info make: ensure folder exists: $(DST.dir))
    DUMMY := $(shell $(MKDIR) $(DST.dir) $(REDIRECT))
    ifdef THIRD_PARTY.dir
        $(info make: replicating 3rd party: $(COPYDIR) $(call OSPATH,$(THIRD_PARTY.dir)) $(DST.dir)$(DIR_SEP)$(notdir $(THIRD_PARTY.dir)))
        DUMMY := $(shell $(COPYDIR) $(call OSPATH,$(THIRD_PARTY.dir)) $(DST.dir)$(DIR_SEP)$(notdir $(THIRD_PARTY.dir)))
		ifdef THIRD_PARTY.del
			DUMMY := $(shell $(DEL) $(DST.dir)$(DIR_SEP)$(THIRD_PARTY.dir)$(DIR_SEP)$(THIRD_PARTY.del))
		endif
    endif
else
    ifneq ($(filter test,$(MAKECMDGOALS)),)
        $(error make: error: do not specify test goal without release, debug or coverage goals)
    endif
    ifneq ($(filter prove,$(MAKECMDGOALS)),)
        $(error make: error: do not specify prove goal without release, debug or coverage goals)
    endif
endif

ifneq ($(filter test,$(MAKECMDGOALS)),)
	DST.d            += $(patsubst %,$(DST.dir)/%,$(subst .c,.d,$(wildcard test/*.c)))
endif

all : convention_usage
	@echo $(ECHOQUOTE)usage: make realclean$(ECHOESCAPE)|clean$(ECHOESCAPE)|(release$(ECHOESCAPE)|debug$(ECHOESCAPE)|coverage [test])$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: use MAKE_DEBUG=1 for make file instrumentation$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: use MAKE_PEER_DEPENDENTS=0 for no recursive make$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: use SXE_LOG_LEVEL=7 to filter lib-sxe-* logging$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: hudson url: http://ci-controller-ubuntu-32.gw.catest.sophos:8080/$(ECHOQUOTE)

include $(TOP.dir)/mak/mak-convention.mak

# Function to reverse a list
#
reverse  = $(if $(1),$(call reverse,$(wordlist  2,$(words $(1)),$(1)))) $(firstword $(1))

#CFLAGS   +=-DEV_MULTIPLICITY=0 # Do we need this?

DEP.lib_pkgs := $(notdir $(wildcard $(foreach PACKAGE, $(LIB_DEPENDENCIES), $(COM.dir)/lib-$(PACKAGE))))
DEP.dll_pkgs := $(notdir $(wildcard $(foreach PACKAGE, $(DLL_DEPENDENCIES), $(COM.dir)/dll-$(PACKAGE))))
ifeq ($(COM.dir),.)
DEP.exe_pkgs := $(notdir $(wildcard $(foreach PACKAGE, $(EXE_DEPENDENCIES), $(COM.dir)/exe-$(PACKAGE))))
endif

DEP.includes := $(foreach PACKAGE, $(DEP.lib_pkgs) $(DEP.dll_pkgs), $(COM.dir)/$(PACKAGE))

IFLAGS      = $(CC_INC). $(CC_INC)$(OS_class) $(foreach DIR,$(DEP.includes),$(CC_INC)$(DIR))
#IFLAGS     += $(if $(findstring port,$(LIB_DEPENDENCIES)),$(CC_INC)$(COM.dir)/lib-port/$(OS_class),)
#IFLAGS     += $(if $(findstring tap,$(LIB_DEPENDENCIES)),$(CC_INC)$(COM.dir)/lib-tap/$(DST.dir),)
#IFLAGS_TEST = $(CC_INC)$(COM.dir)/lib-tap/$(DST.dir)
CFLAGS     += -DMOCK=1 $(IFLAGS) $(CC_INC)$(DST.dir) $(foreach DIR, $(DEP.includes), $(CC_INC)$(DIR)/$(DST.dir))
CFLAGS_TEST = $(IFLAGS_TEST)
SRC.c      := $(wildcard *.c) $(subst $(OS_class)/,,$(wildcard $(OS_class)/*.c))
DST.d      += $(SRC.c:%.c=$(DST.dir)/%.d) $(patsubst %,$(DST.dir)/%,$(subst .c,.d,$(wildcard test/*.c)))
DST.lib    ?= $(foreach LIBRARY,$(LIBRARIES),$(DST.dir)/$(LIBRARY)$(EXT.lib))
DST.exe     = $(foreach EXECUTABLE,$(EXECUTABLES),$(DST.dir)/$(EXECUTABLE)$(EXT.exe))
DST.oks     = $(patsubst %.c,$(DST.dir)/%.ok,$(notdir $(wildcard test/test-*.c))) $(patsubst %.pl,$(DST.dir)/%.ok,$(notdir $(wildcard test/test-*.pl))) $(if $(DST_TESTS),$(foreach TEST,$(DST_TESTS),$(DST.dir)/test-$(TEST).ok),)
DST.obj    ?= $(SRC.c:%.c=$(DST.dir)/%$(EXT.obj)) $(if $(DST_OBJ),$(foreach OBJ,$(DST_OBJ),$(DST.dir)/$(OBJ)$(EXT.obj)),)
# look for other libraries in: libname/[release/debug]/libname$(EXT.lib)
DEP.libs    = $(foreach PACKAGE,$(DEP.lib_pkgs),$(COM.dir)/$(PACKAGE)/$(DST.dir)/$(patsubst lib-%,%,$(PACKAGE))$(EXT.lib))
DEP.dlls    = $(foreach PACKAGE,$(DEP.dll_pkgs),$(COM.dir)/$(PACKAGE)/$(DST.dir)/$(patsubst dll-%,%,$(PACKAGE))$(EXT.dll))
DEP.dirs    = $(call reverse, $(foreach PACKAGE, $(DEP.exe_pkgs) $(DEP.dll_pkgs) $(DEP.lib_pkgs),$(COM.dir)/$(PACKAGE)))

#TEST_LIBS   = $(COM.dir)/lib-tap/$(DST.dir)/tap$(EXT.lib)

# Only do coverage analysis if doing a coverage build and not in the opt-out list
#
ifneq ($(filter-out $(abspath $(addprefix $(COM.dir)/, $(COVERAGE_OPTOUT_LIST))), $(shell pwd)),)
DO_COVERAGE = $(COVERAGE)
endif

ALL_TEST.srcs    := $(wildcard test/*.c)
ACTUAL_TEST.srcs := $(wildcard test/test-*.c)
PERL_TEST.srcs   := $(wildcard test/test-*.pl)
COMMON_TEST.srcs := $(filter-out $(ACTUAL_TEST.srcs),$(ALL_TEST.srcs))
COMMON_TEST.objs := $(patsubst test/%.c,$(DST.dir)/test/%$(EXT.obj),$(COMMON_TEST.srcs))

# Walk sideways if there is no MAKE_PEER_DEPENDENTS defined
#
.PHONY:				$(DEP.dirs)

$(DEP.dirs):
ifndef MAKE_PEER_DEPENDENTS
	@$(MAKE_PERL_ECHO) "make: building: peer dependent: $@"
ifeq ($(COM.dir),.)
	$(MAKE) -C $@ MAKE_PEER_DEPENDENTS=1 $(MAKECMDGOALS)
else
	$(MAKE) -C $@ MAKE_PEER_DEPENDENTS=1 $(filter-out test,$(MAKECMDGOALS))
endif
endif

ifdef MAKE_DEBUG
$(info make: debug: TOP.dir           : $(TOP.dir))
$(info make: debug: COM.dir           : $(COM.dir))
$(info make: debug: SRC.c             : $(SRC.c))
$(info make: debug: OS_class          : $(OS_class))
$(info make: debug: OS_name           : $(OS_name))
$(info make: debug: OS_bits           : $(OS_bits))
$(info make: debug: MAKECMDGOALS      : $(MAKECMDGOALS))
$(info make: debug: LIB_DEPENDENCIES  : $(LIB_DEPENDENCIES))
$(info make: debug: DLL_DEPENDENCIES  : $(DLL_DEPENDENCIES))
$(info make: debug: EXE_DEPENDENCIES  : $(EXE_DEPENDENCIES))
$(info make: debug: ADDITIONAL_EXECUTABLES : $(addprefix $(DST.dir)/,$(ADDITIONAL_EXECUTABLES)))
$(info make: debug: CFLAGS            : $(CFLAGS))
$(info make: debug: IFLAGS            : $(IFLAGS))
$(info make: debug: THIRD_PARTY.dir   : $(THIRD_PARTY.dir))
$(info make: debug: DEP.dirs          : $(DEP.dirs))
$(info make: debug: DEP.includes      : $(DEP.includes))
$(info make: debug: DEP.libs          : $(DEP.libs))
$(info make: debug: DST.d             : $(DST.d))
$(info make: debug: DST.inc           : $(DST.inc))
$(info make: debug: DST.lib           : $(DST.lib))
$(info make: debug: DST.exe           : $(DST.exe))
$(info make: debug: DST.obj           : $(DST.obj))
$(info make: debug: DST.oks           : $(DST.oks))
$(info make: debug: SRC.lib           : $(SRC.lib))
$(info make: debug: COMMON_TEST.objs  : $(COMMON_TEST.objs))
$(info make: debug: build test objects: $(DST.dir)/test-%$(EXT.obj) : test/test-%.c)
$(info make: debug: build test exes   : $(DST.dir)/test-%.t: $(DST.dir)/test-%$(EXT.obj) $(DST.lib) $(DEP.libs))
$(info make: debug: run   test exes   : $(DST.dir)/%.ok: $(DST.dir)/%.t)
endif

release debug coverage:  $(addprefix $(DST.dir)/,$(ADDITIONAL_EXECUTABLES)) $(DEP.dirs) $(DST.lib) $(DST.exe)
ifneq ($(DST.inc),)
	make DST.dir=$(DST.dir) include
endif
	@echo make: $(DST.lib)$(DST.exe) is up-to-date

.PHONY: coverage_clean run_tests release debug coverage

coverage_clean:
	@echo ===== coverage_clean
	$(COVERAGE_INIT)

run_tests : $(DST.oks)

test:				coverage_clean run_tests
	@$(MAKE_PERL_ECHO) "make: tests complete"
ifdef DO_COVERAGE
ifneq ($(strip $(DST.oks)),)
	@$(COVERAGE_REAP)
	@$(COVERAGE_CHECK)
	@$(DEL) $(DST.dir)$(DIR_SEP)*.c
endif
endif

check :
	@$(MAKE_PERL_ECHO) "make: pre-submit check"
	$(MAKE) clean
	$(MAKE) convention
	$(MAKE) release test
	$(MAKE) debug test
ifneq ($(OS),Windows_NT)
	$(MAKE) coverage test
endif
	@echo
	@echo All pre-submit automated tests completed successfully!
	@echo Please have your source code changes reviewed before submit!
	@echo

.SECONDARY:

$(DST.dir)/test/%.d:
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(MKDIR) $(call OSPATH,$(DST.dir)/test) $(TO_NUL) $(FAKE_PASS)
	$(PERL) $(TOP.dir)/mak/bin/mkdeps.pl -d $(OS_class) -o $(DST.dir) $(IFLAGS) $(IFLAGS_TEST) test/$*.c

$(DST.dir)/%.d:
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(PERL) $(TOP.dir)/mak/bin/mkdeps.pl -d $(OS_class) -o $(DST.dir) $(IFLAGS) $*.c

# If we are making, not cleaning, include dependency files, silently ignoring failure to include them.
#
ifdef DST.dir
sinclude $(DST.d)
endif

# Third party wrapper makefiles must define their own rules.
#
ifndef THIRD_PARTY.dir

$(DST.lib) : $(DST.obj) $(SRC.lib)
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(LIB_CMD) $(LIB_FLAGS) $(LIB_OUT)$@ $(DST.obj) $(call OSPATH,$(SRC.lib))

# dump lib contents hint:
# winnt: lib.exe /list foo.lib
# linux: nm foo.lib

# Flags must be last on Windows
$(DST.exe) : $(DST.obj) $(DEP.libs)
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(LINK) $(LINK_OUT)$@ $^ $(COVERAGE_LIBS) $(LINK_FLAGS)

endif

# Pattern rule to make OS dependent variants of a module.
$(DST.dir)/%$(EXT.obj):	$(OS_class)/%.c
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(CC) $(CFLAGS) $(CFLAGS_PROD) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@

$(DST.dir)/%$(EXT.obj):	%.c
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(CC) $(CFLAGS) $(CFLAGS_PROD) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@

# NOTE: when above the more generic DST.dir/%.c rule == checked 1st
#$(DST.dir)/test-%$(EXT.obj) : test/test-%.c
#	@$(MAKE_PERL_ECHO) "make: building: $@"
#	$(CC) $(CFLAGS) $(CFLAGS_TEST) $< $(CC_OUT)$@

$(DST.dir)/test/%$(EXT.obj) : test/%.c
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(CC) $(CFLAGS) $(CFLAGS_TEST) $< $(CC_OUT)$@

# Flags must be last on Windows
$(DST.dir)/test-%.t:	$(DST.dir)/test/test-%$(EXT.obj) $(COMMON_TEST.objs) $(DST.lib) $(DEP.libs)
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(LINK) $(LINK_OUT)$@ $^ $(TEST_LIBS) $(COVERAGE_LIBS) $(LINK_FLAGS)

$(DST.dir)/%.ok:		$(DST.dir)/%.t
	@echo ===== $(DST.dir)/$*.ok
	@$(MAKE_PERL_ECHO) "make: running: $^"
	cd $(call OSPATH,$(dir $<)) && $(call OSPATH,./$(notdir $<))
	@touch $@

$(DST.dir)/%.ok:		./test/%.pl $(DST.exe)  $(DST.lib)
	@echo ===== $(DST.dir)/$*.ok
	@$(MAKE_PERL_ECHO) "make: running: $^"
	cd $(DST.dir) && $(PERL) ../test/$(call OSPATH,$(notdir $<))
	@touch $@

$(DST.dir)/%-proto.h:   %.c
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(PERL) $(TOP.dir)/mak/bin/genxface.pl -d -o $(DST.dir) $*.c

clean::
ifdef THIRD_PARTY.dir
	@$(MAKE_PERL_ECHO) "make: cleaning; ignore third party module (did you want make realclean?)"
else
	@$(MAKE_PERL_ECHO) "make: cleaning"
	$(RMDIR) $(wildcard build-$(OS_name)-$(OS_bits)-*) $(TO_NUL) $(FAKE_PASS)
endif

realclean::
	@$(MAKE_PERL_ECHO) "make: real cleaning"
	$(RMDIR) $(wildcard build-$(OS_name)-$(OS_bits)-*) $(TO_NUL) $(FAKE_PASS)
