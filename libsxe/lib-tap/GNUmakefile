# Wrapper makefile for the third party package: tap

LIBRARIES    = tap
DST_OBJ	     = tap tap-dup tap-ev
DST_TESTS    = run tap-ev

include ../dependencies.mak

release debug coverage:	$(DST.dir)/tap.h

$(DST.dir)/tap.h:				tap/tap.h
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	$(MAKE_RUN) $(COPY) $(call OSPATH,$^) $(call OSPATH,$@)

$(DST.dir)/%$(EXT.obj):				tap/%.c $(DST.dir)/tap.h
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	@echo       $(CC) $(CFLAGS) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@ >  $(call OSPATH,$@.out) 2>&1
	$(MAKE_RUN) $(CC) $(CFLAGS) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@ >> $(call OSPATH,$@.out) 2>&1 $(CC_OUT_ON_ERROR)

$(DST.dir)/run$(EXT.obj):			tap/test/run.c
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	@echo       $(CC) $(CFLAGS) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@ >  $(call OSPATH,$@.out) 2>&1
	$(MAKE_RUN) $(CC) $(CFLAGS) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@ >> $(call OSPATH,$@.out) 2>&1 $(CC_OUT_ON_ERROR)

$(DST.dir)/test-run.t:				$(DST.dir)/run$(EXT.obj) $(DST.lib) $(DEP.libs)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	@echo                     $(LINK) $^ $(COVERAGE_LIBS) $(LINK_OUT)$@ $(LINK_FLAGS) >  $(call OSPATH,$@.out) 2>&1
	$(MAKE_RUN) $(LINK_CHECK) $(LINK) $^ $(COVERAGE_LIBS) $(LINK_OUT)$@ $(LINK_FLAGS) >> $(call OSPATH,$@.out) 2>&1 $(CC_OUT_ON_ERROR)

$(DST.dir)/test-tap-ev$(EXT.obj):	tap/test/test-tap-ev.c
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	@echo       $(CC) $(CFLAGS) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@ >  $(call OSPATH,$@.out) 2>&1
	$(MAKE_RUN) $(CC) $(CFLAGS) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@ >> $(call OSPATH,$@.out) 2>&1 $(CC_OUT_ON_ERROR)

$(DST.dir)/test-tap-ev.t:			$(DST.dir)/test-tap-ev$(EXT.obj) $(DST.lib) $(DEP.libs)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	@echo                     $(LINK) $^ $(COVERAGE_LIBS) $(LINK_OUT)$@ $(LINK_FLAGS) >  $(call OSPATH,$@.out) 2>&1
	$(MAKE_RUN) $(LINK_CHECK) $(LINK) $^ $(COVERAGE_LIBS) $(LINK_OUT)$@ $(LINK_FLAGS) >> $(call OSPATH,$@.out) 2>&1 $(CC_OUT_ON_ERROR)
