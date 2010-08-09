#Copyright (c) 2009, Sophos Group. All rights reserved.
#
#Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
#
# - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
# - Neither the name of Sophos  nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
#
#THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

sinclude ../sophox-tools/makedefs/gmakedefs.mk

CC_OUT	    ?= -o
CFLAGS	    ?= -I. -Wall -c
DST.lib     ?= libtap.a
EXT.obj     ?= .o
INSTALL     := install -o 0 -g 0
LINK	    ?= cc
LINK_FLAGS  ?= $(LFLAGS)
LINK_OUT    ?= -o
DST.obj     ?= tap$(EXT.obj) tap-dup$(EXT.obj) tap-ev$(EXT.obj)

.PHONY: all clean install test

all:		libtap.so libtap.a

test:		test/run.t test/test-tap-ev.t
	test/run.t
	test/test-tap-ev.t

install:
	$(INSTALL) -d $(DESTDIR)/usr
	$(INSTALL) -d $(DESTDIR)/usr/include
	$(INSTALL) -d $(DESTDIR)/usr/lib
	$(INSTALL) libtap.so $(DESTDIR)/usr/lib
	$(INSTALL) libtap.a  $(DESTDIR)/usr/lib
	$(INSTALL) tap.h     $(DESTDIR)/usr/include

%$(EXT.obj):	%.c tap.h
	$(CC) -c $(CFLAGS) -o $@ -fPIC -DPIC $*.c

libtap.a:	$(DST.obj)
	ar cru $@ $^

libtap.so:	$(DST.obj)
	$(CC) -shared $^ -Wl,-soname -Wl,libtap.so -o libtap.so

test/%$(EXT.obj):       test/%.c
	$(CC) $(CFLAGS) $< $(CC_OUT)$@

test/%.t:	test/%$(EXT.obj) $(DST.lib) $(DST.deps)
	$(LINK) $(LINK_FLAGS) $^ $(COVERAGE_LIBS) $(LINK_OUT)$@

clean:
	rm -f *$(EXT.obj) libtap.a libtap.so .depend test/*$(EXT.obj) test/*.t

ifdef depend-target
$(eval $(depend-target))
include .depend
endif
