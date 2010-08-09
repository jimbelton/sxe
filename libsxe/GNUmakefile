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

$(info make: TODO: fix so that "make clean ; make debug test" works; not make debug ; make debug test)

# Extra stuff to build external libraries

DST.lib    += $(DST.dir)/libsxe$(EXT.lib)

libsources  = $(wildcard $(1)/*.c) $(subst $(OS_class)/,,$(wildcard $(1)/$(OS_class)/*.c))
libobjects  = $(foreach SOURCE,$(call libsources,$(1)),$(patsubst $(1)/%.c,$(1)/$(DST.dir)/%$(EXT.obj),$(SOURCE)))
DST.obj     = $(foreach PACKAGE,$(ALL_LIBRARIES),$(call libobjects,$(COM.dir)/lib-$(PACKAGE)))
DST.obj    += $(patsubst $(COM.dir)/lib-tap/tap/%.c,$(COM.dir)/lib-tap/$(DST.dir)/%$(EXT.obj),$(wildcard $(COM.dir)/lib-tap/tap/*.c))

ifeq ($(OS),Windows_NT)
DST.obj    += $(COM.dir)/lib-ev/$(DST.dir)/ev$(EXT.obj)
else
DST.obj    += $(COM.dir)/lib-ev/$(DST.dir)/libev/ev$(EXT.obj) $(COM.dir)/lib-ev/$(DST.dir)/libev/event$(EXT.obj)
endif

libheaders  = $(wildcard $(1)/*.h) $(wildcard $(1)/$(DST.dir)/*.h) $(wildcard $(1)/$(OS_class)/*.h) $(wildcard $(1)/$(OS_class)/sys/*.h)
SRC.inc     = $(filter-out %-private.h,$(foreach PACKAGE,$(ALL_LIBRARIES),$(call libheaders,$(COM.dir)/lib-$(PACKAGE))))
DST.inc     = $(filter-out %-private.h,$(foreach PACKAGE,$(ALL_LIBRARIES),$(subst /$(OS_class),,$(subst ./lib-$(PACKAGE),$(DST.dir)/include,$(call libheaders,$(COM.dir)/lib-$(PACKAGE))))))

include dependencies.mak

ifdef MAKE_DEBUG
$(info make: debug: SRC.inc           : $(SRC.inc))
endif

$(DST.dir)/include/%.h:	$(SRC.inc)
	-@$(MKDIR) $(call OSPATH,$(DST.dir)/include/sys) $(TO_NUL) $(FAKE_PASS)
	$(COPY) $(call OSPATH,$(wildcard */$*.h) $(wildcard */$(OS_class)/$*.h) $(wildcard */$(OS_class)/sys/$*.h)) $(call OSPATH,$(subst /$(DST.dir),,$@))

include:       $(DST.inc)

clean::        $(DEP.dirs)

realclean::    $(DEP.dirs)
