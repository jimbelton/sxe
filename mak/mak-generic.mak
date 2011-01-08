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

TOP.dir := ..

ifndef MAKE_DIR
MAKE.dir := $(TOP.dir)
else
MAKE.dir := $(MAKE_DIR)
endif

ifeq ($(OS),Windows_NT)
    include $(TOP.dir)/mak/mak-winnt.mak
else
    include $(TOP.dir)/mak/mak-unix.mak
endif

.PHONY: release debug coverage check help realclean

help::
	@echo $(ECHOQUOTE)make: usage: [SXE_LOG_LEVEL=7] [MAKE_DEBUG=1] [MAKE_DIR=/mnt/rd] make release$(ECHOESCAPE)|debug$(ECHOESCAPE)|coverage$(ECHOESCAPE)|check help$(ECHOESCAPE)|realclean$(ECHOESCAPE)|clean$(ECHOESCAPE)|<target>$(ECHOQUOTE)

all: help

include $(TOP.dir)/mak/mak-convention.mak

release:
	@echo $(ECHOQUOTE)make: building: folder    : $(MAKE.dir)/$(DST.dir)$(ECHOQUOTE)

debug:
	@echo $(ECHOQUOTE)make: building: folder    : $(MAKE.dir)/$(DST.dir)$(ECHOQUOTE)

coverage:
	@echo $(ECHOQUOTE)make: building: folder    : $(MAKE.dir)/$(DST.dir)$(ECHOQUOTE)

check:
	@echo $(ECHOQUOTE)make: building: pre-submit check$(ECHOQUOTE)
ifneq ($(OS),Windows_NT)
	$(MAKE)      -f GNUmakefile.mak coverage realclean
endif
	$(MAKE)      -f GNUmakefile.mak debug    realclean
	$(MAKE)      -f GNUmakefile.mak release  realclean
ifneq ($(OS),Windows_NT)
	$(MAKE) -j 2 -f GNUmakefile.mak coverage     tests-for-libsxe
	$(MAKE)      -f GNUmakefile.mak coverage run-tests-for-libsxe
endif
	$(MAKE) -j 2 -f GNUmakefile.mak debug        tests-for-libsxe
	$(MAKE)      -f GNUmakefile.mak debug    run-tests-for-libsxe
	$(MAKE) -j 2 -f GNUmakefile.mak release      tests-for-libsxe
	$(MAKE)      -f GNUmakefile.mak release  run-tests-for-libsxe
	@echo
	@echo $(ECHOQUOTE)All pre-submit automated tests completed successfully!$(ECHOQUOTE)
	@echo $(ECHOQUOTE)Please have your source code changes reviewed before submit!$(ECHOQUOTE)
	@echo

ifneq ($(filter release,$(MAKECMDGOALS)),)
    DEBUG           := 0
    COVERAGE        := 0
    RELEASE_TYPE    := release
    DST.dir         := build-$(OS_name)-$(OS_bits)-$(RELEASE_TYPE)
else ifneq ($(filter debug,$(MAKECMDGOALS)),)
    DEBUG           := 1
    COVERAGE        := 0
    RELEASE_TYPE    := debug
    DST.dir         := build-$(OS_name)-$(OS_bits)-$(RELEASE_TYPE)
    CFLAGS          += $(CC_DEF)SXE_DEBUG=1 $(CFLAGS_DEBUG)
else ifneq ($(filter coverage,$(MAKECMDGOALS)),)
    DEBUG           := 0
    COVERAGE        := 1
    RELEASE_TYPE    := release-coverage
    DST.dir         := build-$(OS_name)-$(OS_bits)-release-coverage
    COVERAGE_LIBS   := $(COV_LFLAGS)
    COVERAGE_CFLAGS := $(COV_CFLAGS)
    COVERAGE_INIT   := $(COV_INIT)
    COVERAGE_REAP   := $(COV_REAP)
    COVERAGE_CHECK  := $(MAKE_PERL_COVERAGE_CHECK) $(DST.dir)/*.c.gcov
else
#del $(error make: error: need to specify one of the following: release, debug, or coverage)
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

# - Example usage:
#   - @$(MAKE_PERL_ECHO) "make: building: $@"

define MAKE_PERL_ECHO_BOLD
$(PERL) -e $(OSQUOTE) \
	$$text  = shift @ARGV; \
    $$text .= qq[ ]; \
    $$text .= qq[=] x ( 128 - length ($$text) ); \
    printf qq[$(OSPC)s $(OSPC)s\n], $$text, scalar localtime; \
	exit 0; \
	$(OSQUOTE)
endef

# - Example usage:
#   - @$(MAKE_PERL_ECHO_BOLD) "make: building: $@"

define MAKE_PERL_ECHO
$(PERL) -e $(OSQUOTE) \
	$$text  = shift @ARGV; \
    $$text .= qq[ ]; \
    $$text .= qq[-] x ( 128 - length ($$text) ); \
    printf qq[$(OSPC)s $(OSPC)s\n], $$text, scalar localtime; \
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
	printf qq{%s}, $$o; \
	printf qq{Coverage is insufficient\n}, if ( $$r ); \
	exit $$r; \
	$(OSQUOTE)
endef

CFLAGS += -DMOCK=1

# Note: Never remove the next safety ifdef or loss of source code can result!
ifdef DST.dir
realclean::
	@$(MAKE_PERL_ECHO) "make: real cleaning $(MAKE.dir)/$(DST.dir)"
	$(RMDIR) $(call OSPATH,$(MAKE.dir)/$(DST.dir)) $(TO_NUL) $(FAKE_PASS)
endif

ifdef RELEASE_TYPE
ifeq ($(filter check,$(MAKECMDGOALS)),)
ifeq ($(filter realclean,$(MAKECMDGOALS)),)
ifndef PARCEL_DEPEND_LIST_NOT_CREATED
export PARCEL_DEPEND_LIST_NOT_CREATED := 1
DUMMY := $(shell $(PERL) $(TOP.dir)/mak/bin/find-c-dependencies.pl $(DST.dir) $(TOP.dir) $(OS_class) $(OS_name) $(MAKE.dir) EXT.obj=$(EXT.obj) EXT.lib=$(EXT.lib) EXT.exe=$(EXT.exe) $(CC_INC) DEBUG=$(MAKE_DEBUG))
ifeq ($(filter created-dependencies,$(DUMMY)),)
$(error make: error: failed to create dependencies; see error above)
endif
endif
include $(MAKE.dir)/$(DST.dir)/dependencies.d
endif
endif
endif

#old # usage: mak should be a top level component. The higher level makefile is expected to define:
#old #   COM.dir                = The directory which is the root of the component being built (usually "." or "..")
#old #   TOP.dir                = The directory which includes "mak" (usually ".." or "../..")
#old #   LIB_DEPENDENCIES       = List of all the package names (removing lib-) of the libraries that the package depends on
#old #   CONVENTION_OPTOUT_LIST = List of the packages to opt out of convention checks (e.g: lib-mock lib-port)
#old #   COVERAGE_OPTOUT_LIST   = List of the packages to opt out of coverage   checks (e.g: lib-mock lib-port)
#old
#old ifeq ($(OS),Windows_NT)
#old     include $(TOP.dir)/mak/mak-winnt.mak
#old else
#old     include $(TOP.dir)/mak/mak-unix.mak
#old endif
#old
#old # Preserve intermediate files.
#old #
#old #fixme .SECONDARY:
#old
#old #
#old # - Example usage:
#old #   - @$(MAKE_PERL_ECHO) "make: building: $@"
#old #
#old
#old define MAKE_PERL_ECHO_BOLD
#old $(PERL) -e $(OSQUOTE) \
#old 	$$text  = shift @ARGV; \
#old     $$text .= qq[ ]; \
#old     $$text .= qq[=] x ( 128 - length ($$text) ); \
#old     printf qq[$(OSPC)s $(OSPC)s\n], $$text, scalar localtime; \
#old 	exit 0; \
#old 	$(OSQUOTE)
#old endef
#old
#old define MAKE_PERL_ECHO
#old $(PERL) -e $(OSQUOTE) \
#old 	$$text  = shift @ARGV; \
#old     $$text .= qq[ ]; \
#old     $$text .= qq[-] x ( 128 - length ($$text) ); \
#old     printf qq[$(OSPC)s $(OSPC)s\n], $$text, scalar localtime; \
#old 	exit 0; \
#old 	$(OSQUOTE)
#old endef
#old
#old define MAKE_PERL_COVERAGE_CHECK
#old $(PERL) -e $(OSQUOTE) \
#old 	for $$g ( @ARGV ) { \
#old 		next if $$g =~ /debug/; \
#old 		$$f =  `$(CAT) $$g`; \
#old 		if ( $$f =~ m~(/\* FILE COVERAGE EXCLUSION.*?\*/)~is ) { \
#old 			push @x, sprintf q{%s: %s}, $$g, $$1; \
#old 			next; \
#old 		} \
#old 		$$g =~ s~^(.*)/$(DST.dir)/(.*)\.gcov$$~$$1/$$2~; \
#old 		$$f =~ s~^[^\#:]+:.*$$~~gm; \
#old 		$$f =~ s~^\n+~~s; \
#old 		foreach ( split(m~\n{2,}~s,$$f) ) { \
#old 			$$c = sprintf q{%s:%d}, $$g, m~:\s*(\d+)~; \
#old 			if ( m~(/\* COVERAGE EXCLUSION.*?\*/)~is ) { \
#old 				push @x, sprintf q{%s: %s}, $$c, $$1; \
#old 				next; \
#old 			} \
#old 			$$r  = 1; \
#old 			$$o .= sprintf qq{%s: Error: Insufficient coverage: \n$$_\n}, $$c; \
#old 		} \
#old 	} \
#old 	print join(qq{\n},@x) . qq{\n} if @x; \
#old 	printf qq{%sCoverage is %s\n}, $$o, $$r ? q{insufficient} : q{acceptable}; \
#old 	exit $$r; \
#old 	$(OSQUOTE)
#old endef
#old
#old ifneq ($(filter release,$(MAKECMDGOALS)),)
#old     DEBUG    := 0
#old     COVERAGE := 0
#old     RELEASE_TYPE := release
#old     DST.dir  := build-$(OS_name)-$(OS_bits)-$(RELEASE_TYPE)
#old else ifneq ($(filter debug,$(MAKECMDGOALS)),)
#old     DEBUG    := 1
#old     COVERAGE := 0
#old     RELEASE_TYPE := debug
#old     DST.dir  := build-$(OS_name)-$(OS_bits)-$(RELEASE_TYPE)
#old     CFLAGS   += $(CC_DEF)SXE_DEBUG=1 $(CFLAGS_DEBUG)
#old else ifneq ($(filter coverage,$(MAKECMDGOALS)),)
#old     DEBUG    := 0
#old     COVERAGE := 1
#old     RELEASE_TYPE := release-coverage
#old     DST.dir  := build-$(OS_name)-$(OS_bits)-release-coverage
#old endif
#old
#old ifneq ($(filter release,$(MAKECMDGOALS)),)
#old     ifneq ($(filter debug,$(MAKECMDGOALS)),)
#old         $(error make: do not specify both release and debug goals)
#old     endif
#old     ifneq ($(filter coverage,$(MAKECMDGOALS)),)
#old         $(error make: do not specify both release and coverage goals)
#old     endif
#old endif
#old
#old ifneq ($(filter debug,$(MAKECMDGOALS)),)
#old     ifneq ($(filter coverage,$(MAKECMDGOALS)),)
#old         $(error make: do not specify both debug and coverage goals)
#old     endif
#old endif
#old
#old ifdef DST.dir
#old     ifneq ($(filter clean,$(MAKECMDGOALS)),)
#old         $(error make: error: do not specify clean goal with release, debug or coverage goals)
#old     endif
#old
#old     ifneq ($(filter realclean,$(MAKECMDGOALS)),)
#old         $(error make: error: do not specify realclean goal with release, debug or coverage goals)
#old     endif
#old
#old     ifneq ($(COVERAGE),0)
#old         COVERAGE_LIBS    := $(COV_LFLAGS)
#old         COVERAGE_CFLAGS  := $(COV_CFLAGS)
#old         COVERAGE_INIT    := $(COV_INIT)
#old         COVERAGE_REAP    := $(COV_REAP)
#old         COVERAGE_CHECK   := $(MAKE_PERL_COVERAGE_CHECK) $(DST.dir)/*.c.gcov
#old     endif
#old
#old     $(info make: ensure folder exists: $(DST.dir))
#old     DUMMY := $(shell $(MKDIR) $(DST.dir) $(REDIRECT))
#old     ifdef THIRD_PARTY.dir
#old         $(info make: replicating 3rd party: $(COPYDIR) $(call OSPATH,$(THIRD_PARTY.dir)) $(DST.dir)$(DIR_SEP)$(notdir $(THIRD_PARTY.dir)))
#old         DUMMY := $(shell $(COPYDIR) $(call OSPATH,$(THIRD_PARTY.dir)) $(DST.dir)$(DIR_SEP)$(notdir $(THIRD_PARTY.dir)))
#old 		ifdef THIRD_PARTY.del
#old 			DUMMY := $(shell $(DEL) $(DST.dir)$(DIR_SEP)$(THIRD_PARTY.dir)$(DIR_SEP)$(THIRD_PARTY.del))
#old 		endif
#old     endif
#old else
#old     ifneq ($(filter test,$(MAKECMDGOALS)),)
#old         $(error make: error: do not specify test goal without release, debug or coverage goals)
#old     endif
#old     ifneq ($(filter prove,$(MAKECMDGOALS)),)
#old         $(error make: error: do not specify prove goal without release, debug or coverage goals)
#old     endif
#old endif
#old
#old ifneq ($(filter test,$(MAKECMDGOALS)),)
#old 	DST.d            += $(patsubst %,$(DST.dir)/%,$(subst .c,.d,$(wildcard test/*.c)))
#old endif
#old
#old all : convention_usage
#old 	@echo $(ECHOQUOTE)usage: make realclean$(ECHOESCAPE)|clean$(ECHOESCAPE)|(release$(ECHOESCAPE)|debug$(ECHOESCAPE)|coverage [test])$(ECHOQUOTE)
#old 	@echo $(ECHOQUOTE)usage: note: use MAKE_DEBUG=1 for make file instrumentation$(ECHOQUOTE)
#old 	@echo $(ECHOQUOTE)usage: note: use MAKE_PEER_DEPENDENTS=0 for no recursive make$(ECHOQUOTE)
#old 	@echo $(ECHOQUOTE)usage: note: use SXE_LOG_LEVEL=7 to filter lib-sxe-* logging$(ECHOQUOTE)
#old 	@echo $(ECHOQUOTE)usage: note: hudson url: http://ci-controller-ubuntu-32.gw.catest.sophos:8080/$(ECHOQUOTE)
#old
#old include $(TOP.dir)/mak/mak-convention.mak
#old
#old # Function to reverse a list
#old #
#old reverse  = $(if $(1),$(call reverse,$(wordlist  2,$(words $(1)),$(1)))) $(firstword $(1))
#old
#old #CFLAGS   +=-DEV_MULTIPLICITY=0 # Do we need this?
#old
#old DEP.lib_pkgs := $(notdir $(wildcard $(foreach PACKAGE, $(LIB_DEPENDENCIES), $(COM.dir)/lib-$(PACKAGE))))
#old DEP.dll_pkgs := $(notdir $(wildcard $(foreach PACKAGE, $(DLL_DEPENDENCIES), $(COM.dir)/dll-$(PACKAGE))))
#old ifeq ($(COM.dir),.)
#old DEP.exe_pkgs := $(notdir $(wildcard $(foreach PACKAGE, $(EXE_DEPENDENCIES), $(COM.dir)/exe-$(PACKAGE))))
#old endif
#old
#old DEP.includes := $(foreach PACKAGE, $(DEP.lib_pkgs) $(DEP.dll_pkgs), $(COM.dir)/$(PACKAGE))
#old
#old IFLAGS      = $(CC_INC). $(CC_INC)$(OS_class) $(foreach DIR,$(DEP.includes),$(CC_INC)$(DIR))
#old #IFLAGS     += $(if $(findstring port,$(LIB_DEPENDENCIES)),$(CC_INC)$(COM.dir)/lib-port/$(OS_class),)
#old #IFLAGS     += $(if $(findstring tap,$(LIB_DEPENDENCIES)),$(CC_INC)$(COM.dir)/lib-tap/$(DST.dir),)
#old IFLAGS_TEST = $(CC_INC)$(COM.dir)/../libsxe/lib-tap/$(DST.dir) $(CC_INC)./test
#old #fixme CFLAGS     += -DMOCK=1 $(IFLAGS) $(CC_INC)$(DST.dir) $(foreach DIR, $(DEP.includes), $(CC_INC)$(DIR)/$(DST.dir))
#old CFLAGS     += -DMOCK=1
#old CFLAGS_TEST = $(IFLAGS_TEST)
#old SRC.c      := $(wildcard *.c) $(subst $(OS_class)/,,$(wildcard $(OS_class)/*.c))
#old DST.d      += $(SRC.c:%.c=$(DST.dir)/%.d) $(patsubst %,$(DST.dir)/%,$(subst .c,.d,$(wildcard test/*.c)))
#old DST.lib    ?= $(foreach LIBRARY,$(LIBRARIES),$(DST.dir)/$(LIBRARY)$(EXT.lib))
#old DST.exe     = $(foreach EXECUTABLE,$(EXECUTABLES),$(DST.dir)/$(EXECUTABLE)$(EXT.exe))
#old DST.oks     = $(patsubst %.c,$(DST.dir)/%.ok,$(notdir $(wildcard test/test-*.c))) $(patsubst %.pl,$(DST.dir)/%.ok,$(notdir $(wildcard test/test-*.pl))) $(if $(DST_TESTS),$(foreach TEST,$(DST_TESTS),$(DST.dir)/test-$(TEST).ok),)
#old DST.obj    ?= $(SRC.c:%.c=$(DST.dir)/%$(EXT.obj)) $(if $(DST_OBJ),$(foreach OBJ,$(DST_OBJ),$(DST.dir)/$(OBJ)$(EXT.obj)),)
#old # look for other libraries in: libname/[release/debug]/libname$(EXT.lib)
#old DEP.libs    = $(foreach PACKAGE,$(DEP.lib_pkgs),$(COM.dir)/$(PACKAGE)/$(DST.dir)/$(patsubst lib-%,%,$(PACKAGE))$(EXT.lib))
#old DEP.dlls    = $(foreach PACKAGE,$(DEP.dll_pkgs),$(COM.dir)/$(PACKAGE)/$(DST.dir)/$(patsubst dll-%,%,$(PACKAGE))$(EXT.dll))
#old DEP.dirs    = $(call reverse, $(foreach PACKAGE, $(DEP.exe_pkgs) $(DEP.dll_pkgs) $(DEP.lib_pkgs),$(COM.dir)/$(PACKAGE)))
#old
#old #TEST_LIBS   = $(COM.dir)/lib-tap/$(DST.dir)/tap$(EXT.lib)
#old
#old # Only do coverage analysis if doing a coverage build and not in the opt-out list
#old #
#old ifneq ($(filter-out $(abspath $(addprefix $(COM.dir)/, $(COVERAGE_OPTOUT_LIST))), $(shell $(PWD))),)
#old DO_COVERAGE = $(COVERAGE)
#old endif
#old
#old ALL_TEST.srcs    := $(wildcard test/*.c)
#old ACTUAL_TEST.srcs := $(wildcard test/test-*.c)
#old PERL_TEST.srcs   := $(wildcard test/test-*.pl)
#old COMMON_TEST.srcs := $(filter-out $(ACTUAL_TEST.srcs),$(ALL_TEST.srcs))
#old COMMON_TEST.objs := $(patsubst test/%.c,$(DST.dir)/test/%$(EXT.obj),$(COMMON_TEST.srcs))
#old
#old # Walk sideways if there is no MAKE_PEER_DEPENDENTS defined
#old #
#old #fixme .PHONY:				$(DEP.dirs)
#old
#old $(DEP.dirs):
#old ifndef MAKE_PEER_DEPENDENTS
#old 	@$(MAKE_PERL_ECHO_BOLD) "make: building: peer dependent: $@"
#old ifeq ($(COM.dir),.)
#old 	$(MAKE) -C $@ MAKE_PEER_DEPENDENTS=1 $(MAKECMDGOALS)
#old else
#old 	$(MAKE) -C $@ MAKE_PEER_DEPENDENTS=1 $(filter-out test,$(MAKECMDGOALS))
#old endif
#old endif
#old
#old ifdef MAKE_DEBUG
#old $(info make: debug: RELEASE_TYPE      : $(RELEASE_TYPE))
#old $(info make: debug: TOP.dir           : $(TOP.dir))
#old $(info make: debug: COM.dir           : $(COM.dir))
#old $(info make: debug: SRC.c             : $(SRC.c))
#old $(info make: debug: OS_class          : $(OS_class))
#old $(info make: debug: OS_name           : $(OS_name))
#old $(info make: debug: OS_bits           : $(OS_bits))
#old $(info make: debug: MAKECMDGOALS      : $(MAKECMDGOALS))
#old $(info make: debug: LIB_DEPENDENCIES  : $(LIB_DEPENDENCIES))
#old $(info make: debug: DLL_DEPENDENCIES  : $(DLL_DEPENDENCIES))
#old $(info make: debug: EXE_DEPENDENCIES  : $(EXE_DEPENDENCIES))
#old $(info make: debug: ADDITIONAL_EXECUTABLES : $(addprefix $(DST.dir)/,$(ADDITIONAL_EXECUTABLES)))
#old $(info make: debug: CFLAGS            : $(CFLAGS))
#old $(info make: debug: IFLAGS            : $(IFLAGS))
#old $(info make: debug: THIRD_PARTY.dir   : $(THIRD_PARTY.dir))
#old $(info make: debug: DEP.dirs          : $(DEP.dirs))
#old $(info make: debug: DEP.includes      : $(DEP.includes))
#old $(info make: debug: DEP.libs          : $(DEP.libs))
#old $(info make: debug: DST.d             : $(DST.d))
#old $(info make: debug: DST.inc           : $(DST.inc))
#old $(info make: debug: DST.lib           : $(DST.lib))
#old $(info make: debug: DST.exe           : $(DST.exe))
#old $(info make: debug: DST.obj           : $(DST.obj))
#old $(info make: debug: DST.oks           : $(DST.oks))
#old $(info make: debug: SRC.lib           : $(SRC.lib))
#old $(info make: debug: COMMON_TEST.objs  : $(COMMON_TEST.objs))
#old $(info make: debug: build test objects: $(DST.dir)/test-%$(EXT.obj) : test/test-%.c)
#old $(info make: debug: build test exes   : $(DST.dir)/test-%.t: $(DST.dir)/test-%$(EXT.obj) $(DST.lib) $(DEP.libs))
#old $(info make: debug: run   test exes   : $(DST.dir)/%.ok: $(DST.dir)/%.t)
#old endif
#old
#old release debug coverage:  $(addprefix $(DST.dir)/,$(ADDITIONAL_EXECUTABLES)) $(DEP.dirs) $(DST.lib) $(DST.exe)
#old ifneq ($(DST.inc),)
#old 	make DST.dir=$(DST.dir) include
#old endif
#old 	@echo make: building: $(DST.lib)$(DST.exe) is up-to-date
#old
#old .PHONY: coverage_clean run_tests release debug coverage
#old
#old coverage_clean:
#old 	@echo ===== coverage_clean
#old 	$(COVERAGE_INIT)
#old
#old run_tests : $(DST.oks)
#old
#old test:				coverage_clean run_tests
#old 	@$(MAKE_PERL_ECHO) "make: tests complete"
#old ifdef DO_COVERAGE
#old ifneq ($(strip $(DST.oks)),)
#old 	@$(COVERAGE_REAP)
#old 	@$(COVERAGE_CHECK)
#old 	@$(DEL) $(DST.dir)$(DIR_SEP)*.c
#old endif
#old endif
#old
#old check :
#old 	@$(MAKE_PERL_ECHO) "make: pre-submit check"
#old 	$(MAKE) clean
#old 	$(MAKE) convention
#old 	$(MAKE) release test
#old 	$(MAKE) debug test
#old ifneq ($(OS),Windows_NT)
#old 	$(MAKE) coverage test
#old endif
#old 	@echo
#old 	@echo All pre-submit automated tests completed successfully!
#old 	@echo Please have your source code changes reviewed before submit!
#old 	@echo
#old
#old .SECONDARY:
#old
#old $(DST.dir)/test/%.d:
#old 	@$(MAKE_PERL_ECHO) "make: building: $@"
#old 	$(MKDIR) $(call OSPATH,$(DST.dir)/test) $(TO_NUL) $(FAKE_PASS)
#old 	$(PERL) $(TOP.dir)/mak/bin/mkdeps.pl -d $(OS_class) -o $(DST.dir) $(IFLAGS) $(IFLAGS_TEST) test/$*.c
#old
#old $(DST.dir)/%.d:
#old 	@$(MAKE_PERL_ECHO) "make: building: $@"
#old 	$(PERL) $(TOP.dir)/mak/bin/mkdeps.pl -d $(OS_class) -o $(DST.dir) $(IFLAGS) $*.c
#old
#old # If we are making, not cleaning, include dependency files, silently ignoring failure to include them.
#old #
#old ifdef DST.dir
#old ifndef PARCEL_DEPEND_LIST_NOT_CREATED
#old export PARCEL_DEPEND_LIST_NOT_CREATED := 1
#old DUMMY := $(shell $(PERL) $(TOP.dir)/mak/bin/find-c-dependencies.pl $(DST.dir) $(TOP.dir) $(OS_class) $(OS_name) $(OS_bits) $(EXT.obj) $(EXT.lib) $(EXT.exe) $(CC_INC))
#old ifeq ($(filter created-dependencies,$(DUMMY)),)
#old $(error make: error: failed to create dependencies; see error above)
#old endif
#old endif
#old $(info make: debug: include $(COM.dir)/$(DST.dir)/dependencies.d)
#old include $(COM.dir)/$(DST.dir)/dependencies.d
#old #fixme del sinclude $(DST.d)
#old endif
#old
#old # Third party wrapper makefiles must define their own rules.
#old #
#old ifndef THIRD_PARTY.dir
#old
#old $(DST.lib) : $(DST.obj) $(SRC.lib)
#old 	@$(MAKE_PERL_ECHO) "make: building: $@"
#old 	$(LIB_CMD) $(LIB_FLAGS) $(LIB_OUT)$@ $(DST.obj) $(call OSPATH,$(SRC.lib))
#old
#old # dump lib contents hint:
#old # winnt: lib.exe /list foo.lib
#old # linux: nm foo.lib
#old
#old # Flags must be last on Windows
#old $(DST.exe) : $(DST.obj) $(DEP.libs)
#old 	@$(MAKE_PERL_ECHO) "make: building: $@"
#old 	$(LINK) $(LINK_OUT)$@ $^ $(COVERAGE_LIBS) $(LINK_FLAGS)
#old
#old endif
#old
#old # Pattern rule to make OS dependent variants of a module.
#old $(DST.dir)/%$(EXT.obj):	$(OS_class)/%.c
#old 	@$(MAKE_PERL_ECHO) "make: building: $@"
#old 	$(CC) $(CFLAGS) $(CFLAGS_PROD) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@
#old
#old #fixme $(DST.dir)/%$(EXT.obj):	%.c
#old #fixme 	@$(MAKE_PERL_ECHO) "make: building: $@"
#old #fixme 	$(CC) $(CFLAGS) $(CFLAGS_PROD) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@
#old
#old # NOTE: when above the more generic DST.dir/%.c rule == checked 1st
#old #$(DST.dir)/test-%$(EXT.obj) : test/test-%.c
#old #	@$(MAKE_PERL_ECHO) "make: building: $@"
#old #	$(CC) $(CFLAGS) $(CFLAGS_TEST) $< $(CC_OUT)$@
#old
#old $(DST.dir)/test/%$(EXT.obj) : test/%.c
#old 	@$(MAKE_PERL_ECHO) "make: building: $@"
#old 	$(CC) $(CFLAGS) $(CFLAGS_TEST) $< $(CC_OUT)$@
#old
#old # Flags must be last on Windows
#old $(DST.dir)/test-%.t:	$(DST.dir)/test/test-%$(EXT.obj) $(COMMON_TEST.objs) $(DST.lib) $(DEP.libs)
#old 	@$(MAKE_PERL_ECHO) "make: building: $@"
#old 	$(LINK) $(LINK_OUT)$@ $^ $(TEST_LIBS) $(COVERAGE_LIBS) $(LINK_FLAGS)
#old
#old $(DST.dir)/%.ok:		$(DST.dir)/%.t
#old 	@echo ===== $(DST.dir)/$*.ok
#old 	@$(MAKE_PERL_ECHO) "make: running: $^"
#old 	cd $(call OSPATH,$(dir $<)) && $(call OSPATH,./$(notdir $<))
#old 	@$(TOUCH) $@
#old
#old $(DST.dir)/%.ok:		./test/%.pl $(DST.exe)  $(DST.lib)
#old 	@echo ===== $(DST.dir)/$*.ok
#old 	@$(MAKE_PERL_ECHO) "make: running: $^"
#old 	cd $(DST.dir) && $(PERL) ../test/$(call OSPATH,$(notdir $<))
#old 	@$(TOUCH) $@
#old
#old #fixme $(DST.dir)/%-proto.h:   %.c
#old #fixme 	@$(MAKE_PERL_ECHO) "make: building: $@"
#old #fixme 	$(PERL) $(TOP.dir)/mak/bin/genxface.pl -d -o $(DST.dir) $*.c
#old
#old clean::
#old ifdef THIRD_PARTY.dir
#old 	@$(MAKE_PERL_ECHO) "make: cleaning; ignore third party module (did you want make realclean?)"
#old else
#old 	@$(MAKE_PERL_ECHO) "make: cleaning"
#old 	$(RMDIR) $(wildcard build-$(OS_name)-$(OS_bits)-*) $(TO_NUL) $(FAKE_PASS)
#old endif
#old
#old realclean::
#old 	@$(MAKE_PERL_ECHO) "make: real cleaning"
#old 	$(RMDIR) $(wildcard build-$(OS_name)-$(OS_bits)-*) $(TO_NUL) $(FAKE_PASS)
#old
