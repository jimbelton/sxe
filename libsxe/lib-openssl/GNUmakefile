# Wrapper makefile for the third party package: openssl

LIB_DEPENDENCIES =
LIBRARIES        = openssl
THIRD_PARTY.dir  = ssl
# Disable parallel builds (for now) because they are unstable.
#OPENSSL_LOCKFILE?= $(shell tempfile)
#OPENSSL_FLOCK   ?= flock $(OPENSSL_LOCKFILE)
#OPENSSL_MAKE_J  ?= -j -l 8

# Disable parallel builds (for now) because they are unstable
#MAKE_J          := -j -l 8

include ../dependencies.mak

ifdef SXE_DISABLE_OPENSSL
include dummy.mak
else ifdef SXE_EXTERNAL_OPENSSL
include dummy.mak
else

ifeq ($(OS_name), darwin)
CONFIG_COMMAND   = ./Configure darwin64-x86_64-cc
else
CONFIG_COMMAND   = ./config
endif

# SXE's make flags interfere with OpenSSL's. Let it do its own thing.
unexport MAKEFLAGS

$(DST.dir)/openssl$(EXT.lib):	$(DST.dir)/$(THIRD_PARTY.dir)/libssl$(EXT.lib)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]:  copying: $@"
	$(MAKE_RUN) $(MKDIR) $(DST.dir)/openssl.d
	$(MAKE_RUN) $(COPY) $(DST.dir)/$(THIRD_PARTY.dir)/libssl$(EXT.lib) $(DST.dir)/openssl.d
	$(MAKE_RUN) cd $(DST.dir)/openssl.d && $(LIB_EXTRACT) libssl$(EXT.lib)
	$(MAKE_RUN) $(COPY) $(DST.dir)/$(THIRD_PARTY.dir)/libcrypto$(EXT.lib) $(DST.dir)/openssl.d
	$(MAKE_RUN) cd $(DST.dir)/openssl.d && $(LIB_EXTRACT) libcrypto$(EXT.lib)
	$(MAKE_RUN) cd $(DST.dir)/openssl.d && $(LIB_CMD) ../openssl$(EXT.lib) *.o
	$(MAKE_RUN) cd $(DST.dir) && $(LIB_INDEX) openssl$(EXT.lib)
	$(MAKE_RUN) $(MKDIR) $(DST.dir)/openssl
	$(MAKE_RUN) $(COPYFILES2DIR) $(DST.dir)/$(THIRD_PARTY.dir)/include/openssl/*.h $(DST.dir)/openssl

$(DST.dir)/$(THIRD_PARTY.dir)/libssl$(EXT.lib):   $(DST.dir)/$(THIRD_PARTY.dir)/config
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	$(MAKE_RUN) $(CHMOD_R_WRITABLE) $(DST.dir)/$(THIRD_PARTY.dir)
	$(MAKE_RUN) cd $(DST.dir)/$(THIRD_PARTY.dir) && $(CONFIG_COMMAND)
	+$(MAKE_RUN) $(MAKE) $(OPENSSL_MAKE_J) -C $(DST.dir)/$(THIRD_PARTY.dir) FLOCK='$(OPENSSL_FLOCK)'

endif
