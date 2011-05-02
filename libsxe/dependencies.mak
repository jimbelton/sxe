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

# Find the relative path to the component directory and to the top of the hierarchy

COM.dir := $(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)), $(MAKEFILE_LIST))))
TOP.dir = $(COM.dir)/..

# List of the libraries in linker order.
# This is used by both the package GNUmakefiles and the top level GNUmakefile
#
remove_to = $(if $(filter $(1),$(2)),$(call remove_to,$(1),$(wordlist 2,$(words $(2)),$(2))),$(2))
ALL_LIBRARIES    = sxe-expose sxe-dirwatch sxe-ring-buffer sxe-lua sxe-httpd sxe-http sxe-sync-ev sxe-pool-tcp \
		   sxe-hash lookup3 sha1 sxe-spawn sxe sxe-pool sxe-thread sxe-mmap sxe-list sxe-socket sxe-test \
		   ev lua sxe-cstr sxe-util sxe-log mock port tap
LIB_DEPENDENCIES = $(call remove_to,$(LIBRARIES),$(ALL_LIBRARIES))

# Convention opt-out list
CONVENTION_OPTOUT_LIST = lib-lookup3 lib-mock lib-port

# Coverage opt-out list
COVERAGE_OPTOUT_LIST   = lib-lookup3 lib-mock lib-port lib-sha1 lib-tap

include $(TOP.dir)/mak/mak-common.mak

IFLAGS += $(if $(findstring port,$(LIB_DEPENDENCIES)),$(CC_INC)$(COM.dir)/lib-port/$(OS_class),)

ifeq ($(OS),Windows_NT)
ifdef MAKE_MINGW
    LINK_FLAGS += -lWinmm
else
    LINK_FLAGS += /DEFAULTLIB:Winmm.lib
endif
endif
