# GNUmakefile for building a dummy library

release debug coverage:		$(DST.dir)/$(LIBRARIES)$(EXT.lib)

$(DST.dir)/$(LIBRARIES)$(EXT.lib):	$(DST.dir)/$(LIBRARIES).d/dummy-$(LIBRARIES)$(EXT.obj)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	$(MAKE_RUN) $(LIB_CMD) $(LIB_FLAGS) $(LIB_OUT)$@ $^

$(DST.dir)/$(LIBRARIES).d/dummy-$(LIBRARIES)$(EXT.obj):	$(DST.dir)/$(THIRD_PARTY.dir)/dummy-$(LIBRARIES).c
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	$(MAKE_RUN) $(MKDIR) $(call OSPATH,$(DST.dir)/$(LIBRARIES).d)
	@echo       $(CC) $(CFLAGS) $< $(CC_OUT)$@ >  $(call OSPATH,$@.out) 2>&1
	$(MAKE_RUN) $(CC) $(CFLAGS) $< $(CC_OUT)$@ >> $(call OSPATH,$@.out) 2>&1 $(CC_OUT_ON_ERROR)

$(DST.dir)/$(THIRD_PARTY.dir)/dummy-$(LIBRARIES).c:
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: generating dummy C file: $@"
	$(MAKE_RUN) $(PERL) -le $(OSQUOTE)print q{/* This file intentionally left blank */};$(OSQUOTE) > $(call OSPATH,$@)
