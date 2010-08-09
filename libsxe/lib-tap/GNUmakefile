# Wrapper makefile for the third party package: tap

LIBRARIES    = tap
DST_OBJ	     = tap tap-dup tap-ev
DST_TESTS    = run tap-ev

include ../dependencies.mak

$(info make: TODO: make coverage test should not complain about lack of .c.gcov files, since there are no tests being run)

release debug coverage:	$(DST.dir)/tap.h

$(DST.dir)/tap.h:					tap/tap.h
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(COPY) $(call OSPATH,$^) $(call OSPATH,$@)

$(DST.dir)/%$(EXT.obj):				tap/%.c $(DST.dir)/tap.h
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(CC) $(CFLAGS) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@

$(DST.dir)/run$(EXT.obj):			tap/test/run.c
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(CC) $(CFLAGS) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@

$(DST.dir)/test-run.t:				$(DST.dir)/run$(EXT.obj) $(DST.lib) $(DEP.libs)
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(LINK) $^ $(COVERAGE_LIBS) $(LINK_OUT)$@ $(LINK_FLAGS)

$(DST.dir)/test-tap-ev$(EXT.obj):	tap/test/test-tap-ev.c
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(CC) $(CFLAGS) $(RELEASE_CFLAGS) $(COVERAGE_CFLAGS) $< $(CC_OUT)$@

$(DST.dir)/test-tap-ev.t:			$(DST.dir)/test-tap-ev$(EXT.obj) $(DST.lib) $(DEP.libs)
	@$(MAKE_PERL_ECHO) "make: building: $@"
	$(LINK) $^ $(COVERAGE_LIBS) $(LINK_OUT)$@ $(LINK_FLAGS)
