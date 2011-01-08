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

THIRDPARTY_LIB_TAP_OBJECTS = \
    $(MAKE.dir)/$(DST.dir)/libsxe/lib-tap/tap/tap$(EXT.obj) \
    $(MAKE.dir)/$(DST.dir)/libsxe/lib-tap/tap/tap-ev$(EXT.obj) \
    $(MAKE.dir)/$(DST.dir)/libsxe/lib-tap/tap/tap-dup$(EXT.obj)

$(MAKE.dir)/$(DST.dir)/libsxe/lib-tap/tap/%$(EXT.obj): \
    $(TOP.dir)/libsxe/lib-tap/tap/tap.c \
    $(TOP.dir)/libsxe/lib-tap/tap/tap-ev.c \
    $(TOP.dir)/libsxe/lib-tap/tap/tap-dup.c \
    $(TOP.dir)/libsxe/lib-tap/tap/tap.h
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(COPYDIR) $(call OSPATH,$(TOP.dir)/libsxe/lib-tap) $(call OSPATH,$(MAKE.dir)/$(DST.dir)/libsxe/lib-tap)
	$(CC) $(CFLAGS) $(CC_INC)$(MAKE.dir)/$(DST.dir)/libsxe $(TOP.dir)/libsxe/lib-tap/tap/$(*F).c $(CC_OUT)$@
