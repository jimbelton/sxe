ifeq ($(filter remote,$(MAKECMDGOALS)),)

UREMOTE_H_ := $(ECHOESCAPE)<remote-host$(ECHOESCAPE)>
UU_        := $(ECHOESCAPE)<user$(ECHOESCAPE)>
UM_        := $(ECHOESCAPE)<make$(ECHOESCAPE)>

.PHONY : usage

usage : convention_usage
	@echo $(ECHOQUOTE)usage: make [remote] realclean$(ECHOESCAPE)|clean$(ECHOESCAPE)|(release$(ECHOESCAPE)|debug$(ECHOESCAPE)|coverage [test [fulllog]])$(ECHOESCAPE)|coverage_gitdiff|check$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: make  remote  shell$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: check:          winnt   winnt mingw $(ECHOESCAPE)<- winnt: gcc $(ECHOESCAPE)& cl$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: check:          mingw   mingw mingw $(ECHOESCAPE)<- winnt: gcc$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: check:          linux   linux linux $(ECHOESCAPE)<- linux: gcc$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: check:          rhes5   rhes5 rhes5 $(ECHOESCAPE)<- rhes5: gcc$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set CFLAGS_EXTRA="<flags>"        to add custom flags to the make$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set MAKE_DEBUG=1                  to enable make file instrumentation$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set MAKE_PEER_DEPENDENTS=0        to disable recursive make$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set MAKE_FORCE_32BIT=1            to force 32bit build on 64bit platform (todo: winnt)$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set MAKE_DEPLOY_TYPE="production" to override default "experimental" (todo: winnt)$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set MAKE_DEBS=1                   to build .debs for executables$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set MAKE_RUN_STRESS_TESTS=1       to enable test-stress-*.c tests$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set MAKE_RUN_LEGACY_TESTS=.*      to run legacy tests (listed in $$(LEGACY_TEST_LIST))$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set MAKE_CACHE_THIRD_PARTY=1      to cache third party builds based upon git hashes (experimental; no real clean; todo: winnt)$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set SXE_DISABLE_OPENSSL=1         to disable openssl support *1$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set SXE_EXTERNAL_OPENSSL=1        to use an external openssl library instead of the bundled openssl *1$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set SXE_LOG_LEVEL=7               to filter lib-sxe-* logging$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set SXE_WINNT_ASSERT_MSGBOX=1     to enable the messagebox on assert (to use the Microsoft visual debugger)$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set REMOTE_HOST=localhost         to build in a different path on the local box:  eg: make remote check REMOTE_HOST=localhost$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set REMOTE_HOST=$(UREMOTE_H_)     to build on a remote box:                       eg: make remote check REMOTE_HOST=mybox$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set REMOTE_PROXY=                 to build on a remote box without a SOCKS proxy: eg: make remote check REMOTE_HOST=mybox REMOTE_PROXY=$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set REMOTE_PATH=/path             to override the remote build path:              eg: make remote check REMOTE_PATH=$(REMOTE_PATH)$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set REMOTE_USER=$(UU_)            to build as a different user on a remote box:   eg: make remote check REMOTE_USER=$(USER)$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set REMOTE_MAKE=$(UM_)            to specify the remote GNU make command:         eg: make remote check REMOTE_MAKE=$(MAKE)$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: set REMOTE_CMDEXTRA=              to specify extra remote GNU make parameters:    eg: make remote check REMOTE_CMDEXTRA=MAKE_DEBUG=1$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: add path to mingw32-gcc.exe to enable mingw build$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: add __debugbreak(); to breakpoint into MSVC debugger$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: *1 openssl is experimental (memory handling) and not available on winnt yet$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: hudson url: http://ci-controller-ubuntu-32.gw.catest.sophos:8080/$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: remote windows sshd: http://en.wikipedia.org/wiki/CopSSH$(ECHOQUOTE)
	@echo $(ECHOQUOTE)usage: note: Building C++ is not yet supported under windows$(ECHOQUOTE)
	@$(PERL) -e $(OSQUOTE)$$_=q[usage: note: 64bit Ubuntu users building 32bit might want to: sudo apt-get install libc6-dev-i386 gcc-multilib g++-multilib]; s~DQ~\x22~g; s~SQ~\x27~g; print $$_ . qq[\n];$(OSQUOTE)
	@$(PERL) -e $(OSQUOTE)$$_=q[usage: note: 64bit Ubuntu users building 32bit might want to: install libpcap step 1: cd /tmp && wget http://ubuntu.media.mit.edu/ubuntu//pool/main/libp/libpcap/libpcap0.8_1.1.1-2_i386.deb]; s~DQ~\x22~g; s~SQ~\x27~g; print $$_ . qq[\n];$(OSQUOTE)
	@$(PERL) -e $(OSQUOTE)$$_=q[usage: note: 64bit Ubuntu users building 32bit might want to: install libpcap step 2: cd /tmp && wget http://ubuntu.media.mit.edu/ubuntu//pool/main/libp/libpcap/libpcap0.8-dev_1.1.1-2_i386.deb]; s~DQ~\x22~g; s~SQ~\x27~g; print $$_ . qq[\n];$(OSQUOTE)
	@$(PERL) -e $(OSQUOTE)$$_=q[usage: note: 64bit Ubuntu users building 32bit might want to: install libpcap step 3: dpkg-deb -x libpcap0.8_1.1.1-2_i386.deb /tmp/extract/]; s~DQ~\x22~g; s~SQ~\x27~g; print $$_ . qq[\n];$(OSQUOTE)
	@$(PERL) -e $(OSQUOTE)$$_=q[usage: note: 64bit Ubuntu users building 32bit might want to: install libpcap step 4: dpkg-deb -x libpcap0.8-dev_1.1.1-2_i386.deb /tmp/extract/]; s~DQ~\x22~g; s~SQ~\x27~g; print $$_ . qq[\n];$(OSQUOTE)
	@$(PERL) -e $(OSQUOTE)$$_=q[usage: note: 64bit Ubuntu users building 32bit might want to: install libpcap step 5: file /tmp/extract/usr/lib/*]; s~DQ~\x22~g; s~SQ~\x27~g; print $$_ . qq[\n];$(OSQUOTE)
	@$(PERL) -e $(OSQUOTE)$$_=q[usage: note: 64bit Ubuntu users building 32bit might want to: install libpcap step 6: cp /tmp/extract/usr/lib/* /usr/lib32/]; s~DQ~\x22~g; s~SQ~\x27~g; print $$_ . qq[\n];$(OSQUOTE)
	@$(PERL) -e $(OSQUOTE)$$_=q[usage: note: MAKE_DEBS=1 on Ubuntu might require: sudo apt-get install debhelper]; s~DQ~\x22~g; s~SQ~\x27~g; print $$_ . qq[\n];$(OSQUOTE)
	@$(PERL) -e $(OSQUOTE)$$_=q[usage: note: svn users: ignore all build folders: svn propset -R svn:ignore SQbuild-*SQ .]; s~DQ~\x22~g; s~SQ~\x27~g; print $$_ . qq[\n];$(OSQUOTE)
	@$(PERL) -e $(OSQUOTE)$$_=q[usage: note: linux   remote example: make remote debug test REMOTE_HOST=rhes53-32]; s~DQ~\x22~g; s~SQ~\x27~g; print $$_ . qq[\n];$(OSQUOTE)
	@$(PERL) -e $(OSQUOTE)$$_=q[usage: note: windows remote example: make remote debug test REMOTE_HOST=win32-xp REMOTE_PATH=/cygdrive/c/temp/$$USER/build REMOTE_CMDPREFIX=SQcmd /C DQvsvars32.bat && SQ REMOTE_CMDSUFFIX=SQDQSQ REMOTE_SETENV=SQPATH=DQ/cygdrive/c/Program Files/Microsoft Visual Studio 9.0/Common7/Tools:$$$${PATH/:\/bin/}DQSQ REMOTE_CMDEXTRA=SQ && echo doneSQ]; s~DQ~\x22~g; s~SQ~\x27~g; print $$_ . qq[\n];$(OSQUOTE)

endif

