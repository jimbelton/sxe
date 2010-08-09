./debug/lib-testcode-proto.h: $(wildcard ./*.h) $(wildcard ./*.c)
	@$(MAKE_PERL_ECHO) "make: building: $@"
	cd . && $(PERL) $(TOP.dir)/mak/bin/genxface.pl -d -o debug

debug/testcode.o debug/testcode.d: ../libev/ev.h ./testcode.h ./debug/lib-testcode-proto.h testcode.c GNUmakefile
