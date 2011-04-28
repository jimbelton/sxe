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

# usage: mak should be a top level component. The higher level makefile is expected to define:
#   COM.dir                = The directory which is the root of the component being built (usually "." or "..")
#   TOP.dir                = The directory which includes "mak" (usually ".." or "../..")
#   LIB_DEPENDENCIES       = List of all the package names (removing lib-) of the libraries that the package depends on
#   CONVENTION_OPTOUT_LIST = List of the packages to opt out of convention checks (e.g: lib-mock lib-port)
#   COVERAGE_OPTOUT_LIST   = List of the packages to opt out of coverage   checks (e.g: lib-mock lib-port)

# See http://www.electric-cloud.com/blog/2009/08/19/makefile-performance-built-in-rules/
# No builtin implicit rules:
.SUFFIXES:
MAKEFLAGS = --no-builtin-rules

ifeq ($(OS),Windows_NT)
    ifdef MAKE_DEBUG
        $(info make[$(MAKELEVEL)]: debug: include $(TOP.dir)/mak/mak-winnt.mak)
    endif
    include $(TOP.dir)/mak/mak-winnt.mak
else
    ifdef MAKE_DEBUG
        $(info make[$(MAKELEVEL)]: debug: include $(TOP.dir)/mak/mak-unix.mak)
    endif
    include $(TOP.dir)/mak/mak-unix.mak
endif

ifdef MAKE_DEBUG
    $(info make[$(MAKELEVEL)]: debug: include $(TOP.dir)/mak/mak-common-defines.mak)
endif
include $(TOP.dir)/mak/mak-common-defines.mak

# Preserve intermediate files.
#
.SECONDARY:

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
        COVERAGE_CHECK   := $(MAKE_PERL_COVERAGE_CHECK) $(call OSPATH,$(DST.dir) $(OS_class))
    endif

    # vvv - This code was disabled when building peers - that broke things, so I reverted; TBD: why was this done?

    ifdef MAKE_DEBUG
        $(info make[$(MAKELEVEL)]: ensure folder exists: $(DST.dir))
    endif

    DUMMY := $(shell $(MKDIR) $(DST.dir) $(REDIRECT))

    ifdef THIRD_PARTY.dir
        DUMMY := $(shell $(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: 3rdparty: replicating: $(COPYDIR) $(call OSPATH,$(THIRD_PARTY.dir)) $(DST.dir)$(DIR_SEP)$(notdir $(THIRD_PARTY.dir))")
        $(info $(DUMMY))
        DUMMY := $(shell $(COPYDIR) $(call OSPATH,$(THIRD_PARTY.dir)) $(DST.dir)$(DIR_SEP)$(notdir $(THIRD_PARTY.dir)))
        ifdef THIRD_PARTY.del
           DUMMY := $(shell $(DEL) $(DST.dir)$(DIR_SEP)$(THIRD_PARTY.dir)$(DIR_SEP)$(THIRD_PARTY.del))
        endif
    endif

    # ^^^ - This code was disabled when building peers - that broke things, so I reverted; TBD: why was this done?
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
	@echo $(ECHOQUOTE)usage: make realclean$(ECHOESCAPE)|clean$(ECHOESCAPE)|(release$(ECHOESCAPE)|debug$(ECHOESCAPE)|coverage [test])$(ECHOESCAPE)|check$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: check:          winnt   winnt mingw $(ECHOESCAPE)<- winnt: gcc $(ECHOESCAPE)& cl$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: check:          mingw   mingw mingw $(ECHOESCAPE)<- winnt: gcc$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: check:          linux   linux linux $(ECHOESCAPE)<- linux: gcc$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: check:          rhes5   rhes5 rhes5 $(ECHOESCAPE)<- rhes5: gcc$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set MAKE_DEBUG=1 for make file instrumentation$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set MAKE_PEER_DEPENDENTS=0 for no recursive make$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set SXE_LOG_LEVEL=7 to filter lib-sxe-* logging$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: add path to mingw32-gcc.exe to enable mingw build$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: add __debugbreak(); to breakpoint into MSVC debugger$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: hudson url: http://ci-controller-ubuntu-32.gw.catest.sophos:8080/$(ECHOQUOTE)

#http://tdm-gcc.tdragon.net/download

include $(TOP.dir)/mak/mak-convention.mak

# Function to reverse a list
#
reverse  = $(if $(1),$(call reverse,$(wordlist  2,$(words $(1)),$(1)))) $(firstword $(1))

#CFLAGS   +=-DEV_MULTIPLICITY=0 # Do we need this?

DEP.lib_pkgs := $(notdir $(wildcard $(foreach PACKAGE, $(LIBRARIES) $(LIB_DEPENDENCIES), $(COM.dir)/lib-$(PACKAGE))))
DEP.dll_pkgs := $(notdir $(wildcard $(foreach PACKAGE,              $(DLL_DEPENDENCIES), $(COM.dir)/dll-$(PACKAGE))))
ifeq ($(COM.dir),.)
DEP.exe_pkgs := $(notdir $(wildcard $(foreach PACKAGE,              $(EXE_DEPENDENCIES), $(COM.dir)/exe-$(PACKAGE))))
endif

DEP.includes := $(foreach PACKAGE, $(DEP.lib_pkgs) $(DEP.dll_pkgs), $(COM.dir)/$(PACKAGE))

TOP.path    = $(realpath $(TOP.dir))/
# CUR.dir is the path to the current directory with the path to the top of the project removed, leaving e.g. "libsxe/lib-sxe"
CUR.dir     = $(subst $(TOP.path),,$(realpath .))
IFLAGS      = $(CC_INC). \
              $(CC_INC)$(OS_class) \
              $(foreach DIR,$(DEP.includes),$(CC_INC)$(DIR)) \
              $(foreach DIR,$(DEP.includes),$(CC_INC)$(DIR)/$(DST.dir))
# TODO: Export lib-tap headers with libsxe.a or have a way of automatically adding lib-tap (below)
IFLAGS_TEST = $(CC_INC)./test \
              $(CC_INC)$(COM.dir)/../libsxe/lib-tap/$(DST.dir)
CFLAGS     += -DSXE_FILE=\"$(CUR.dir)/$<\" -DMOCK=1 $(IFLAGS) $(CC_INC)$(DST.dir)
CFLAGS_TEST+= $(IFLAGS_TEST)
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
ifneq ($(COVERAGE),0)
ifneq ($(filter-out $(abspath $(addprefix $(COM.dir)/, $(COVERAGE_OPTOUT_LIST))), $(shell $(PWD))),)
DO_COVERAGE = coverage_clean
endif
endif

ALL_TEST.srcs    := $(wildcard test/*.c)
ACTUAL_TEST.srcs := $(wildcard test/test-*.c)
PERL_TEST.srcs   := $(wildcard test/test-*.pl)
COMMON_TEST.srcs := $(filter-out $(ACTUAL_TEST.srcs),$(ALL_TEST.srcs))
COMMON_TEST.objs := $(patsubst test/%.c,$(DST.dir)/test/%$(EXT.obj),$(COMMON_TEST.srcs))

# Walk sideways if there is no MAKE_PEER_DEPENDENTS defined
#
.PHONY:		$(DEP.dirs)

$(DEP.dirs):
ifndef MAKE_PEER_DEPENDENTS
	@$(MAKE_PERL_ECHO_BOLD) "make[$(MAKELEVEL)]: checking: $(DST.dir): peer dependent: $@"
	$(MAKE_RUN) $(MAKE) $(MAKE_DEBUG_SUB_MAKE_DIR) -C $@ MAKE_PEER_DEPENDENTS=1 $(MAKECMDGOALS)
endif

ifdef MAKE_DEBUG
$(info make[$(MAKELEVEL)]: debug: MAKECMDGOALS...........: $(MAKECMDGOALS))
$(info make[$(MAKELEVEL)]: debug: MAKE_PEER_DEPENDENTS...: $(MAKE_PEER_DEPENDENTS))
$(info make[$(MAKELEVEL)]: debug: MAKE_MINGW.............: $(MAKE_MINGW))
$(info make[$(MAKELEVEL)]: debug: DEBUG..................: $(DEBUG))
$(info make[$(MAKELEVEL)]: debug: COVERAGE...............: $(COVERAGE))
$(info make[$(MAKELEVEL)]: debug: RELEASE_TYPE...........: $(RELEASE_TYPE))
$(info make[$(MAKELEVEL)]: debug: TOP.dir................: $(TOP.dir))
$(info make[$(MAKELEVEL)]: debug: COM.dir................: $(COM.dir))
$(info make[$(MAKELEVEL)]: debug: CUR.dir................: $(CUR.dir))
$(info make[$(MAKELEVEL)]: debug: SRC.c..................: $(SRC.c))
$(info make[$(MAKELEVEL)]: debug: OS_class...............: $(OS_class))
$(info make[$(MAKELEVEL)]: debug: OS_name................: $(OS_name))
$(info make[$(MAKELEVEL)]: debug: OS_bits................: $(OS_bits))
$(info make[$(MAKELEVEL)]: debug: LIBRARIES..............: $(LIBRARIES))
$(info make[$(MAKELEVEL)]: debug: ALL_LIBRARIES..........: $(ALL_LIBRARIES))
$(info make[$(MAKELEVEL)]: debug: LIB_DEPENDENCIES.......: $(LIB_DEPENDENCIES))
$(info make[$(MAKELEVEL)]: debug: DLL_DEPENDENCIES.......: $(DLL_DEPENDENCIES))
$(info make[$(MAKELEVEL)]: debug: EXE_DEPENDENCIES.......: $(EXE_DEPENDENCIES))
$(info make[$(MAKELEVEL)]: debug: ADDITIONAL_EXECUTABLES.: $(addprefix $(DST.dir)/,$(ADDITIONAL_EXECUTABLES)))
$(info make[$(MAKELEVEL)]: debug: IFLAGS.................: $(IFLAGS))
$(info make[$(MAKELEVEL)]: debug: IFLAGS_TEST............: $(IFLAGS_TEST))
$(info make[$(MAKELEVEL)]: debug: THIRD_PARTY.dir........: $(THIRD_PARTY.dir))
$(info make[$(MAKELEVEL)]: debug: DEP.dir................: $(DEP.dir))
$(info make[$(MAKELEVEL)]: debug: DEP.dirs...............: $(DEP.dirs))
$(info make[$(MAKELEVEL)]: debug: DEP.includes...........: $(DEP.includes))
$(info make[$(MAKELEVEL)]: debug: DEP.libs...............: $(DEP.libs))
$(info make[$(MAKELEVEL)]: debug: DST.d..................: $(DST.d))
$(info make[$(MAKELEVEL)]: debug: DST.inc................: $(DST.inc))
$(info make[$(MAKELEVEL)]: debug: DST.lib................: $(DST.lib))
$(info make[$(MAKELEVEL)]: debug: DST.exe................: $(DST.exe))
$(info make[$(MAKELEVEL)]: debug: DST.obj................: $(DST.obj))
$(info make[$(MAKELEVEL)]: debug: DST.oks................: $(DST.oks))
$(info make[$(MAKELEVEL)]: debug: SRC.lib................: $(SRC.lib))
$(info make[$(MAKELEVEL)]: debug: COMMON_TEST.objs.......: $(COMMON_TEST.objs))
$(info make[$(MAKELEVEL)]: debug: COVERAGE_OPTOUT_LIST...: $(COVERAGE_OPTOUT_LIST))
$(info make[$(MAKELEVEL)]: debug: DO_COVERAGE............: $(DO_COVERAGE))
$(info make[$(MAKELEVEL)]: debug: build.test.objects.....: $(DST.dir)/test-%$(EXT.obj) : test/test-%.c)
$(info make[$(MAKELEVEL)]: debug: build.test.exes........: $(DST.dir)/test-%.t: $(DST.dir)/test-%$(EXT.obj) $(DST.lib) $(DEP.libs))
$(info make[$(MAKELEVEL)]: debug: run...test.exes........: $(DST.dir)/%.ok: $(DST.dir)/%.t)
$(info make[$(MAKELEVEL)]: debug: CFLAGS.................: $(CFLAGS))
$(info make[$(MAKELEVEL)]: debug: CFLAGS_TEST............: $(CFLAGS_TEST))
else
MAKE_RUN=@                                   # Shut things up unless MAKE_DEBUG is set
MAKE_DEBUG_SUB_MAKE_DIR=--no-print-directory # Shut things up unless MAKE_DEBUG is set
endif

# Includes are martialed in a second pass that has the MAKE_PEER_DEPENDENTS flag
#
ifdef MAKE_PEER_DEPENDENTS
INCLUDES=$(DST.inc)
endif

# Dependency on DST.exe was made conditional on MAKE_PEER_DEPENDENCIES; this broke thing; reverted; TBD: why was this done?
release debug coverage:  $(addprefix $(DST.dir)/,$(ADDITIONAL_EXECUTABLES)) $(DEP.dirs) $(INCLUDES) $(DST.lib) $(DST.exe)
ifneq ($(DST.inc),)
    # come here if need to collect all include files for high level library; recurse for make wildcards
	$(MAKE_RUN) $(MAKE) $(MAKE_DEBUG_SUB_MAKE_DIR) DST.dir=$(DST.dir) include
endif
ifndef MAKE_PEER_DEPENDENTS
    # come here if       making peer dependents
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: built:    $(DST.lib)$(DST.exe) & all dependents $(DST.all)"
else
    # come here if *not* making peer dependents
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: built:    $(DST.lib)$(DST.exe)"
endif

.PHONY: coverage_clean run_tests release debug coverage

coverage_clean:
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: coverage: clean"
	$(COVERAGE_INIT)

run_tests: $(DST.oks)

# This code ws disabled when making peer dependents; this broke stuff; reverted it; TBD: why was this disabled?
.PHONY: test
test:				$(DO_COVERAGE) run_tests
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: ran:      $(DST.dir): tests complete"
ifdef DO_COVERAGE
ifneq ($(strip $(DST.oks)),)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: coverage: check"
	$(MAKE_RUN) $(COVERAGE_CHECK)
	$(MAKE_RUN) $(DEL) $(DST.dir)$(DIR_SEP)*.c
endif
endif

check :
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: pre-submit check"
	$(MAKE_RUN) $(MAKE) $(MAKE_DEBUG_SUB_MAKE_DIR) clean
	$(MAKE_RUN) $(MAKE) $(MAKE_DEBUG_SUB_MAKE_DIR) convention
	$(MAKE_RUN) $(MAKE) $(MAKE_DEBUG_SUB_MAKE_DIR) release test
	$(MAKE_RUN) $(MAKE) $(MAKE_DEBUG_SUB_MAKE_DIR) debug test
	$(MAKE_RUN) $(MAKE) $(MAKE_DEBUG_SUB_MAKE_DIR) coverage test
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]:"
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: All pre-submit automated tests completed successfully!"
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: Please have your source code changes reviewed before submit!"
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]:"

.SECONDARY:

clean::
ifdef THIRD_PARTY.dir
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: cleaning; ignore third party module (did you want make realclean?)"
else
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: cleaning"
	$(RMDIR) $(wildcard build-$(OS_name)-$(OS_bits)-*) $(TO_NUL) $(FAKE_PASS)
endif

realclean::
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: real cleaning"
	$(RMDIR) $(wildcard build-$(OS_name)-$(OS_bits)-*) $(TO_NUL) $(FAKE_PASS)

# If we are making, not cleaning, include dependency files, silently ignoring failure to include them.
# This code ws disabled when making peer dependents; this broke stuff; reverted it; TBD: why was this disabled?
#
ifdef DST.dir
    ifdef MAKE_DEBUG
        $(info make[$(MAKELEVEL)]: debug: sinclude $(DST.d))
    endif

    sinclude $(DST.d)
endif

$(DST.dir)/test/%.d:
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	$(MAKE_RUN) $(MKDIR) $(call OSPATH,$(DST.dir)/test) $(TO_NUL) $(FAKE_PASS)
	$(MAKE_RUN) $(PERL) $(TOP.dir)/mak/bin/mkdeps.pl -d $(OS_class) -o $(DST.dir) $(IFLAGS) $(IFLAGS_TEST) test/$*.c

$(DST.dir)/%.d:
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	$(MAKE_RUN) $(PERL) $(TOP.dir)/mak/bin/mkdeps.pl -d $(OS_class) -o $(DST.dir) $(IFLAGS) $*.c

$(DST.dir)/test/%$(EXT.obj) : test/%.c
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	@echo       $(CC) $(CFLAGS) $(CFLAGS_TEST) $< $(CC_OUT)$@ >  $(call OSPATH,$@.out) 2>&1
	$(MAKE_RUN) $(CC) $(CFLAGS) $(CFLAGS_TEST) $< $(CC_OUT)$@ >> $(call OSPATH,$@.out) 2>&1 $(CC_OUT_ON_ERROR)

# Pattern rule to make OS dependent variants of a module.
$(DST.dir)/%$(EXT.obj):	$(OS_class)/%.c
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	@echo       $(CC) $(CFLAGS) $(CFLAGS_PROD) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@ >  $(call OSPATH,$@.out) 2>&1
	$(MAKE_RUN) $(CC) $(CFLAGS) $(CFLAGS_PROD) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@ >> $(call OSPATH,$@.out) 2>&1 $(CC_OUT_ON_ERROR)

$(DST.dir)/%$(EXT.obj):	%.c
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	@echo       $(CC) $(CFLAGS) $(CFLAGS_PROD) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@ >  $(call OSPATH,$@.out) 2>&1
	$(MAKE_RUN) $(CC) $(CFLAGS) $(CFLAGS_PROD) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@ >> $(call OSPATH,$@.out) 2>&1 $(CC_OUT_ON_ERROR)

# Flags must be last on Windows
$(DST.dir)/test-%.t:	$(DST.dir)/test/test-%$(EXT.obj) $(COMMON_TEST.objs) $(DST.lib)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	@echo                     $(LINK) $(LINK_OUT)$@ $^ $(DEP.libs) $(TEST_LIBS) $(COVERAGE_LIBS) $(LINK_FLAGS) >  $(call OSPATH,$@.out) 2>&1
	$(MAKE_RUN) $(LINK_CHECK) $(LINK) $(LINK_OUT)$@ $^ $(DEP.libs) $(TEST_LIBS) $(COVERAGE_LIBS) $(LINK_FLAGS) >> $(call OSPATH,$@.out) 2>&1 $(CC_OUT_ON_ERROR)

$(DST.dir)/%.ok:		$(DST.dir)/%.t
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: running:  $^"
	$(MAKE_RUN) cd $(call OSPATH,$(dir $<)) && $(TEST_ENV_VARS) $(call OSPATH,./$(notdir $<)) $(TEST_OUT_ON_ERROR)
	@$(TOUCH) $@

$(DST.dir)/%.ok:		./test/%.pl $(DST.exe)  $(DST.lib)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: running:  $^"
	$(MAKE_RUN) cd $(DST.dir) && $(PERL) ../test/$(call OSPATH,$(notdir $<)) $(TEST_OUT_ON_ERROR)
	@$(TOUCH) $@

# Third party wrapper makefiles must define their own rules.
#
ifndef THIRD_PARTY.dir

$(DST.lib) : $(DST.obj) $(SRC.lib)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	-$(MAKE_RUN)$(MKDIR) $(call OSPATH,$(DST.dir)) $(TO_NUL) $(FAKE_PASS)
	$(MAKE_RUN) $(LIB_CMD) $(LIB_FLAGS) $(LIB_OUT)$@ $(DST.obj) $(call OSPATH,$(SRC.lib))

# dump lib contents hint:
# winnt: lib.exe /list foo.lib
# linux: nm foo.lib

# Flags must be last on Windows
$(DST.exe) : $(DST.obj) $(DEP.libs)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	@echo                     $(LINK) $(LINK_OUT)$@ $(call OSPATH,$^) $(COVERAGE_LIBS) $(LINK_FLAGS) >  $(call OSPATH,$@.out) 2>&1
	$(MAKE_RUN) $(LINK_CHECK) $(LINK) $(LINK_OUT)$@ $(call OSPATH,$^) $(COVERAGE_LIBS) $(LINK_FLAGS) >> $(call OSPATH,$@.out) 2>&1 $(CC_OUT_ON_ERROR)

endif
