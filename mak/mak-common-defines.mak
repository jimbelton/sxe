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

ifdef MAKE_DEBUG
                                             # Make more verbose if  MAKE_DEBUG is set
else
MAKE_RUN=@                                   # Shut things up unless MAKE_DEBUG is set
MAKE_DEBUG_SUB_MAKE_DIR=--no-print-directory # Shut things up unless MAKE_DEBUG is set
endif

#
# - Example usage:
#   - @$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
#

define MAKE_PERL_ECHO_ERROR
$(PERL) -e $(OSQUOTE) \
    $$text  = shift @ARGV; \
    $$text .= qq[ ]; \
    $$text .= qq[!] x ( 128 - length ($$text) ); \
    print $$text . qq[ ] . scalar ( localtime ) . qq[\n]; \
    exit 0; \
    $(OSQUOTE)
endef

define MAKE_PERL_ECHO_STAR
$(PERL) -e $(OSQUOTE) \
    $$text  = shift @ARGV; \
    $$text .= qq[ ]; \
    $$text .= qq[#] x ( 128 - length ($$text) ); \
    print $$text . qq[ ] . scalar ( localtime ) . qq[\n]; \
    exit 0; \
    $(OSQUOTE)
endef

define MAKE_PERL_ECHO_BOLD
$(PERL) -e $(OSQUOTE) \
    $$text  = shift @ARGV; \
    $$text .= qq[ ]; \
    $$text .= qq[=] x ( 128 - length ($$text) ); \
    print $$text . qq[ ] . scalar ( localtime ) . qq[\n]; \
    exit 0; \
    $(OSQUOTE)
endef

define MAKE_PERL_ECHO
$(PERL) -e $(OSQUOTE) \
    $$text  = shift @ARGV; \
    $$text .= qq[ ]; \
    $$text .= qq[-] x ( 128 - length ($$text) ); \
    print $$text . qq[ ] . scalar ( localtime ) . qq[\n]; \
    exit 0; \
    $(OSQUOTE)
endef

define MAKE_PERL_LIB
$(PERL) -e $(OSQUOTE) \
	use strict; \
	use warnings; \
	use File::Path; \
	if (qq[$(MAKE_MINGW)] eq qq[1]) { \
	} \
	else { \
		if ($$^O eq qq[MSWin32]) { \
			my $$lib_cmd = qq[lib.exe ] . join(q[ ], @ARGV); \
			printf qq[MAKE_PERL_LIB: debug: creating library: $(OSPC)s\n], $$lib_cmd if ( $(MAKE_DEBUG) ); \
			exit(system($$lib_cmd)); \
		} \
	} \
    my $$tmp_lib_dir = qq[$(DST.dir)/tmp-lib-explosion]; \
    while (@ARGV > 1 && $$ARGV[-1] =~ m~\.a$$~) { \
		my $$lib = pop(@ARGV); \
		printf qq[MAKE_PERL_LIB: debug: expanding .a: $(OSPC)s\n], $$lib if ( $(MAKE_DEBUG) ); \
		-d $$tmp_lib_dir or mkdir($$tmp_lib_dir) or die(qq[MAKE_PERL_LIB: Cannot make directory $$tmp_lib_dir\n]); \
		system(qq[cp $$lib $$tmp_lib_dir/.]) == 0 or die(qq[MAKE_PERL_LIB: Cannot copy library $$lib into $$tmp_lib_dir\n]); \
		my @expand_libs_with_path = glob(qq[$$tmp_lib_dir/*$(EXT.lib)]); \
		foreach my $$expand_lib_with_path (@expand_libs_with_path) { \
			my ( $$expand_lib ) = $$expand_lib_with_path =~ m~([^\\\/]+)$$~; \
			my $$lib_cmd = qq[cd $$tmp_lib_dir && ar x $$expand_lib]; \
			printf qq[MAKE_PERL_LIB: debug: expanding library: $(OSPC)s\n], $$lib_cmd if ( $(MAKE_DEBUG) ); \
			system($$lib_cmd) == 0 or die(qq[MAKE_PERL_LIB: Cannot explode library $$_\n]); \
		} \
	} \
	my $$lib_cmd = qq[ar csr ] . join(q[ ], @ARGV) . qq[ ] . join(q[ ],glob(qq[$$tmp_lib_dir/*$(EXT.obj)])); \
	printf qq[MAKE_PERL_LIB: debug: creating library: $(OSPC)s\n], $$lib_cmd if ( $(MAKE_DEBUG) ); \
	system($$lib_cmd) == 0 or die(qq[MAKE_PERL_LIB: Archive command failed: $$lib_cmd]); \
	if (-d $$tmp_lib_dir) { \
		rmtree($$tmp_lib_dir) or die(qq[MAKE_PERL_LIB: Cannot recursively remove directory $$tmp_lib_dir\n]); \
	} \
    $(OSQUOTE)
endef

define MAKE_PERL_DLL
$(PERL) -e $(OSQUOTE) \
	use strict; \
	use warnings; \
	use File::Path; \
	if (qq[$(MAKE_MINGW)] eq qq[1]) { \
	} \
	else { \
		if ($$^O eq qq[MSWin32]) { \
			my $$dll_cmd = qq[lib.exe ] . join(q[ ], @ARGV); \
			printf qq[MAKE_PERL_LIB: debug: creating shared library: $(OSPC)s\n], $$dll_cmd if ( $(MAKE_DEBUG) ); \
			exit(system($$dll_cmd)); \
		} \
	} \
    my $$tmp_dll_dir = qq[$(DST.dir)/tmp-dll-explosion]; \
    while (@ARGV > 1 && $$ARGV[-1] =~ m~\.so$$~) { \
		my $$dll = pop(@ARGV); \
		printf qq[MAKE_PERL_LIB: debug: expanding .so: $(OSPC)s\n], $$dll if ( $(MAKE_DEBUG) ); \
		-d $$tmp_dll_dir or mkdir($$tmp_dll_dir) or die(qq[MAKE_PERL_LIB: Cannot make directory $$tmp_dll_dir\n]); \
		system(qq[cp $$dll $$tmp_dll_dir/.]) == 0 or die(qq[MAKE_PERL_LIB: Cannot copy shared library $$dll into $$tmp_dll_dir\n]); \
		my @expand_dlls_with_path = glob(qq[$$tmp_dll_dir/*$(EXT.dll)]); \
		foreach my $$expand_dll_with_path (@expand_dlls_with_path) { \
			my ( $$expand_dll ) = $$expand_dll_with_path =~ m~([^\\\/]+)$$~; \
			my $$dll_cmd = qq[cd $$tmp_dll_dir && ar x $$expand_dll]; \
			printf qq[MAKE_PERL_LIB: debug: expanding library: $(OSPC)s\n], $$dll_cmd if ( $(MAKE_DEBUG) ); \
			system($$dll_cmd) == 0 or die(qq[MAKE_PERL_LIB: Cannot explode shared library $$_\n]); \
		} \
	} \
	my $$dll_cmd = qq[$(CC) -fPIC -shared -o] . join(q[ ], @ARGV) . qq[ ] . join(q[ ],glob(qq[$$tmp_dll_dir/*$(EXT.obj)])); \
	printf qq[MAKE_PERL_LIB: debug: creating shared library: $(OSPC)s\n], $$dll_cmd if ( $(MAKE_DEBUG) ); \
	system($$dll_cmd) == 0 or die(qq[MAKE_PERL_LIB: Archive command failed: $$dll_cmd]); \
	if (-d $$tmp_dll_dir) { \
		rmtree($$tmp_dll_dir) or die(qq[MAKE_PERL_LIB: Cannot recursively remove directory $$tmp_dll_dir\n]); \
	} \
    $(OSQUOTE)
endef

define COPYFILES2DIR_SUB
	use File::Copy; \
	use File::stat; \
	sub copyfiles2dir_sub { \
		$$dst_folder = pop @_; \
		foreach $$src ( @_ ) { \
			($$file) = $$src =~ m~([^\\\/]+)$$~; \
			$$dst = $$dst_folder . q[/] . $$file; \
			die qq[cp: cannot stat `$$src`: No such file or directory] if ( not -e $$src ); \
			if ( ( not -e $$dst                                                    ) \
			||   ( File::stat::stat($$src)->mtime > File::stat::stat($$dst)->mtime ) ) { \
				printf qq[`$(OSPC)s` -> `$(OSPC)s/`\n], $$src, $$dst; \
				unlink $$dst; \
				File::Copy::copy ( $$src, $$dst ) || die qq[$$!]; \
				utime File::stat::stat($$src)->mtime, File::stat::stat($$src)->mtime, $$dst; \
			} \
		} \
		return 0; \
	}
endef

define MAKE_PERL_COVERAGE_CHECK
$(PERL) -e $(OSQUOTE) \
	( $$dst_dir, $$os_class ) = @ARGV; \
	$$sub_libs = qq[../$(LIBRARIES) ../$(DLLIBRARIES)]; \
	if ( -e q[$(COM.dir)/test] ) { \
		if ( q[$(COM.dir)] eq q[..] ) { \
			printf qq[make[$(MAKELEVEL)]: coverage: NOTE: top level $(COM.dir)/test/ folder exists; delaying final coverage check for @sub_libs\n]; \
			exit 0; \
		} \
		$$sub_libs = qq[$(filter-out $(COVERAGE_OPTOUT_LIST), $(EXECUTABLES:%=exe-%) $(ALL_LIBRARIES:%=lib-%))]; \
	} \
	@sub_libs = split ( m~\s+~, $$sub_libs ); \
	printf qq[make[$(MAKELEVEL)]: coverage: checking coverage in subdirectories: @sub_libs\n] if ( $(MAKE_DEBUG) ); \
	undef @x; \
	foreach $$sub_lib ( sort @sub_libs ) { \
		$$sub_lib =~ s~\b([^ ]+)~lib-$$1~ if ( ! -e $$sub_lib ); \
		printf qq[make[$(MAKELEVEL)]: coverage: checking coverage in subdirectory: $(OSPC)s\n], $$sub_lib if ( $(MAKE_DEBUG) ); \
		printf qq[make[$(MAKELEVEL)]: coverage: exists? $$sub_lib/$$os_class/\n] if ( $(MAKE_DEBUG) ); \
		undef $$gcov_output; \
		if ( -d qq[$$sub_lib/$$os_class] ) { \
			printf qq[make[$(MAKELEVEL)]: coverage: creating $(OSPC)s/$(OSPC)s/$(OSPC)s/\n], $$sub_lib, $$dst_dir, $$os_class if ( $(MAKE_DEBUG) ); \
			mkdir qq[$$sub_lib/$$dst_dir/$$os_class]; \
			@os_class_c_to_copy = glob ( qq[$$sub_lib/$$os_class/*.c $$sub_lib/$$os_class/*.cpp] ); \
			printf qq[make[$(MAKELEVEL)]: coverage: updating: @os_class_c_to_copy\n] if ( $(MAKE_DEBUG) ); \
			copyfiles2dir_sub ( @os_class_c_to_copy, qq[$$sub_lib/$$dst_dir/$$os_class] ); \
			my @files = map { m{\Q$$sub_lib/$$dst_dir\E/(.+)}; $$1 } <$$sub_lib/$$dst_dir/$$os_class/*.c $$sub_lib/$$dst_dir/$$os_class/*.cpp>; \
			$$gcov_cmd = qq[cd $$sub_lib/$$dst_dir && $(COV) @files 2>&1]; \
			printf qq[make[$(MAKELEVEL)]: coverage: running gcov; $(OSPC)s\n], $$gcov_cmd if ( $(MAKE_DEBUG) ); \
			$$gcov_output = `$$gcov_cmd`; \
		} \
		@c_to_copy = glob ( qq[$$sub_lib/*.c $$sub_lib/*.cpp] ); \
		if ( 0 == scalar @c_to_copy ) { \
			printf qq[make[$(MAKELEVEL)]: coverage: found no source files; skipping subdirectory $: $(OSPC)s\n], $$sub_lib; \
			next; \
		} \
		printf qq[make[$(MAKELEVEL)]: coverage: updating: @c_to_copy\n] if ( $(MAKE_DEBUG) ); \
		copyfiles2dir_sub ( @c_to_copy, qq[$$sub_lib/$$dst_dir] ); \
		unlink ( qq[$$sub_lib/$$dst_dir/gcov-stdout.log] ); \
		unlink ( qq[$$sub_lib/$$dst_dir/gcov-stderr.log] ); \
		my @files = map { m{.*/(.+)}; $$1 } <$$sub_lib/$$dst_dir/*.c $$sub_lib/$$dst_dir/*.cpp>; \
		for my $$file (@files) { \
		    $$gcov_cmd = qq[cd $$sub_lib/$$dst_dir && $(COV) $$file >>gcov-stdout.log 2>>gcov-stderr.log]; \
		    printf qq[make[$(MAKELEVEL)]: coverage: running gcov; $(OSPC)s\n], $$gcov_cmd if ( $(MAKE_DEBUG) ); \
		    `$$gcov_cmd`; \
		} \
		open ( IN, q[<], qq[$$sub_lib/$$dst_dir/gcov-stdout.log] ) || die qq[make[$(MAKELEVEL)]: coverage: ERROR: cannot open file: gcov-stdout.log]; \
		sysread ( IN, $$tmp_stdout, 9_999_999 ); \
		close ( IN ) ; \
		open ( IN, q[<], qq[$$sub_lib/$$dst_dir/gcov-stderr.log] ) || die qq[make[$(MAKELEVEL)]: coverage: ERROR: cannot open file: gcov-stderr.log]; \
		sysread ( IN, $$tmp_stderr, 9_999_999 ); \
		close ( IN ) ; \
		$$gcov_output  =  $$tmp_stdout . $$tmp_stderr; \
		$$gcov_output2 =  $$gcov_output; \
		$$gcov_output  =~ s~`~\x27~g; \
		$$gcov_output  =~ s~^/.*~~gm; \
		$$gcov_output  =~ s~[^\s]+\.h:cannot open source file[\r\n]+~~gs; \
		$$gcov_output  =~ s~^\s*\x27[^\r\n]+\.h\.gcov\x27\s*~~gm; \
		$$gcov_output  =~ s~No executable lines~~gm; \
		$$gcov_output  =~ s~[^\s]+\.gcno:no functions found[\r\n]+~~gs; \
		$$gcov_output  =~ s~File [^\r\n]+[\r\n]+~~gs; \
		$$gcov_output  =~ s~Lines executed:[^\r\n]+[\r\n]+~~gs; \
		$$gcov_output  =~ s~[^\s]+:creating [^\r\n]+[\r\n]+~~gs; \
		$$gcov_output  =~ s~[^\s]+source file is newer than graph file [^\r\n]+[\r\n]+~~gs; \
		$$gcov_output  =~ s~[^\s]+cannot open graph file[\r\n]+~~gs; \
		$$gcov_output  =~ s~[^\s]+\:cannot open data file, assuming not executed[\r\n]+~~gs; \
		$$gcov_output  =~ s~Creating [^\r\n]+[\r\n]+~~gs; \
		$$gcov_output  =~ s~Cannot open source file [^\r\n]+[\r\n]+~~gs; \
		$$gcov_output  =~ s~Removing [^\r\n]+[\r\n]+~~gs; \
		$$gcov_output  =~ s~\(the message is only displayed one per source file\)[\r\n]+~~gs; \
		$$gcov_output  =~ s~../lib-boost/[^\r\n]+:cannot open source file[\r\n]+~~gs; \
		while ( $$gcov_output =~ m~([^\s]+)\.gcda\:cannot open data file[\r\n]+~gs ) { \
			my @fn = <$$1.c $$1.cpp>; \
			printf qq[make[$(MAKELEVEL)]: coverage: WARNING: no coverage for file: @fn\n]; \
		} \
		$$gcov_output =~ s~[^\s]+\:cannot open data file[\r\n]+~~gs; \
		if ( $$gcov_output !~ m~^\s*$$~s ) { \
			printf qq[make[$(MAKELEVEL)]: coverage: WARNING: unexpected output found in lines:\n$(OSPC)s\n], $$gcov_output2; \
			die(  qq[make[$(MAKELEVEL)]: coverage: FATAL: unexpected gcov line(s) (in above):\n$$gcov_output\n]); \
		} \
		printf qq[make[$(MAKELEVEL)]: coverage: showing exclusions\n] if ( $(MAKE_DEBUG) ); \
		@g = glob ( qq[$$sub_lib/$$dst_dir/*.c.gcov $$sub_lib/$$dst_dir/*.cpp.gcov] ); \
		if ( 0 == scalar @g ) { \
			printf qq[make[$(MAKELEVEL)]: coverage: WARNING: no *.gcov files found in $$sub_lib\n]; \
			next if ($$sub_lib !~ m~lib-~); \
			@f = glob ( qq[$$sub_lib/$$dst_dir/*.c $$sub_lib/$$dst_dir/*.cpp] ); \
			die( qq[make[$(MAKELEVEL)]: coverage: FATAL: no *.gcov files found and no c files found\n]) if ( 0 == scalar @f ); \
			foreach $$f ( @f ) { \
				open ( IN, q[<], $$f ) || die qq[make[$(MAKELEVEL)]: coverage: ERROR: cannot open c file: $$f]; \
				sysread ( IN, $$fb, 9_999_999 ); \
				close ( IN ) ; \
				die( qq[make[$(MAKELEVEL)]: coverage: FATAL: no *.gcov files found but found coverage exclusion in c file: $$f\n]) if ( $fb =~ m~COVERAGE\s+EXCLUSION~i ); \
			} \
		} \
		undef $$total_lines_covered; \
		undef $$total_lines_covered_not; \
		undef $$total_lines_excluded; \
		undef $$total_lines_tolerated; \
		undef $$o; \
		undef @make_tolerate_uncovered_lines; \
		$$summary_file = sprintf qq[./$(OSPC)s/$(OSPC)s/coverage-summary.mak], $$sub_lib, $$dst_dir; \
		open ( SUM, qq[>] . $$summary_file ); \
		for $$g ( @g ) { \
			next if $$g =~ /debug/; \
			printf qq[make[$(MAKELEVEL)]: coverage: processing: $(OSPC)s\n], $$g if ( $(MAKE_DEBUG) ); \
			open ( IN, q[<], $$g ) || die qq[make[$(MAKELEVEL)]: coverage: ERROR: cannot open gcov file: $$g]; \
			sysread ( IN, $$f, 9_999_999 ); \
			close ( IN ) ; \
			if ( $$f =~ m~(/\* FILE COVERAGE EXCLUSION.*?\*/)~is ) { \
				printf qq[make[$(MAKELEVEL)]: coverage: excluding: $(OSPC)s\n], $$g if ( $(MAKE_DEBUG) ); \
				push @x, sprintf q{$(OSPC)s: $(OSPC)s}, $$g, uc $$1; \
				next; \
			} \
			$$g =~ s~^(.*)/$(DST.dir)/(.*)\.gcov$$~$$1/$$2~; \
			while ( $$f =~ s~^\s+(?:[\-]+\:.*|[\#]+\:\s*\d+\:\s*[A-Z_]+\:\s*)$$~~gm ) {}; \
			      $$these_lines_covered      = $$f =~ s~^\s+[0-9]+\*?\:.*$$~~gm; \
			      $$these_lines_covered_not  = $$f =~ s~^(\s+[\#]+\:.*)$$~$$1~gm; \
			      $$total_lines_covered     += $$these_lines_covered; \
			      $$total_lines_covered_not += $$these_lines_covered_not; \
			undef $$these_lines_excluded; \
			$$f =~ s~^[\r\n]+~~s; \
			foreach $$block ( split(m~\n{2,}~s,$$f) ) { \
				printf qq[make[$(MAKELEVEL)]: coverage: line:\n$(OSPC)s\n], $$block if ( $(MAKE_DEBUG) ); \
				$$c_file = sprintf q{$(OSPC)s:$(OSPC)d}, $$g, $$block =~ m~:\s*(\d+)~; \
				if ( $$block =~ m~(/\* COVERAGE EXCLUSION.*?\*/)~is ) { \
					push @x, sprintf q{make[$(MAKELEVEL)]: coverage: $(OSPC)s: lines: $(OSPC)s}, $$c_file, uc $$1; \
					$$lines_in_block = $$block =~ s~([\r\n]+)~$1~gs; \
					$$these_lines_excluded    += ( 1 + $$lines_in_block ); \
					$$these_lines_covered_not -= ( 1 + $$lines_in_block ); \
					$$total_lines_covered_not -= ( 1 + $$lines_in_block ); \
					next; \
				} \
				$$r  = 1; \
				$$block =~ s~^(.*)$$~make[$(MAKELEVEL)]: coverage: $$1~gm; \
				$$o .= sprintf qq{make[$(MAKELEVEL)]: coverage: $(OSPC)s: WARN: Inadequate coverage: \n$(OSPC)s\n}, $$c_file, $$block; \
			} \
			{ \
				     $$lines_total_to_divide   =  $$these_lines_covered + $$these_lines_covered_not; \
				     $$lines_total_to_divide   =  0 == $$lines_total_to_divide ? 1 : $$lines_total_to_divide; \
				my   $$file_as_env             =  uc $$g; \
				     $$file_as_env             =~ s~[^A-Z0-9]~_~g; \
				     $$file_as_env             =~ s~^[_]+~~g; \
				my        $$tolerate_lines     =  q[$(MAKE_TOLERATE_UNCOVERED_LINES)]; \
				my ( $$these_lines_tolerated ) =  $$tolerate_lines =~ m~\b$$file_as_env=(\d+)~; \
				     $$total_lines_excluded   += $$these_lines_excluded; \
				     $$total_lines_tolerated  += $$these_lines_tolerated; \
				     $$r                       =  0 if ( $$these_lines_tolerated >= $$these_lines_covered_not ); \
				printf qq[make[$(MAKELEVEL)]: coverage: tolerance: $(OSPC)s=$(OSPC)d\n], $$file_as_env, $$these_lines_tolerated if ( $(MAKE_DEBUG) ); \
				push @make_tolerate_uncovered_lines, sprintf qq[MAKE_TOLERATE_UNCOVERED_LINES += $(OSPC)s=$(OSPC)d], $$file_as_env, $$these_lines_covered_not; \
				$$sum = sprintf qq{make[$(MAKELEVEL)]: coverage: Coverage is $(OSPC)s ($(OSPC)5.1f$(OSPC)$(OSPC); lines: $(OSPC)4d excluded, $(OSPC)4d covered, $(OSPC)4d not covered, $(OSPC)4d tolerated) for $$g\n}, $$r ? q{INADEQUATE} : q{acceptable}, 100 - $$these_lines_covered_not / $$lines_total_to_divide * 100, , $$these_lines_excluded, $$these_lines_covered, $$these_lines_covered_not, $$these_lines_tolerated; \
				$$o .= $$sum; \
				$$r_ever ++       if ( $$r ); \
				push @g_ever, $$g if ( $$r ); \
			} \
		} \
		print     join ( qq{\n}, @x ) . qq{\n} if @x; \
		print SUM join ( qq{\n}, @x ) . qq{\n} if @x; \
		$$lines_total_to_divide =  $$total_lines_covered + $$total_lines_covered_not; \
		$$lines_total_to_divide =  0 == $$lines_total_to_divide ? 1 : $$lines_total_to_divide; \
		$$r                     =  1 if ( 0 == $$total_lines_covered ); \
		$$sum = sprintf qq{$(OSPC)smake[$(MAKELEVEL)]: coverage: Coverage lib totals    ($(OSPC)5.1f$(OSPC)$(OSPC); lines: $(OSPC)4d excluded, $(OSPC)4d covered, $(OSPC)4d not covered, $(OSPC)4d tolerated) for $$sub_lib\n}, $$o, 100 - $$total_lines_covered_not / $$lines_total_to_divide * 100, $$total_lines_excluded, $$total_lines_covered, $$total_lines_covered_not, $$total_lines_tolerated; \
		print       $$sum; \
		print   SUM $$sum; \
		$$tolerance_file = sprintf qq[./$(OSPC)s/$(OSPC)s/coverage-tolerances.mak], $$sub_lib, $$dst_dir; \
		open  ( TOL, qq[>] . $$tolerance_file ); \
		printf  TOL qq{# this file generated by release coverage build at $(OSPC)s\n}, scalar localtime; \
		printf  TOL qq{# copy out of build folder and include from dependencies.mak to use during build\n}; \
		printf  TOL qq{# do not use this file if 100$(OSPC)$(OSPC) code coverage is desired!\n}; \
		printf  TOL qq{#        use this file to maintain a base line coverage with *legacy* code bases\n}; \
		printf  TOL qq{# lines below show non-covered line toleration counts per file\n}; \
		printf  TOL qq{\n}; \
		print   TOL join ( qq{\n}, @make_tolerate_uncovered_lines ) . qq{\n}; \
		close ( TOL ); \
		close ( SUM ); \
		$$summary_file   =~ s~^./$(COM.dir)/~./~; \
		$$tolerance_file =~ s~^./$(COM.dir)/~./~; \
		printf qq{make[$(MAKELEVEL)]: coverage: note: coverage tolerances written to: $(OSPC)s\n}, $$tolerance_file; \
		printf qq{make[$(MAKELEVEL)]: coverage: note: coverage summary    written to: $(OSPC)s\n}, $$summary_file; \
		printf qq{make[$(MAKELEVEL)]: coverage: note: coverage gcov files written to: ./$(OSPC)s/$(OSPC)s/*.c.gcov\n}, $$sub_lib, $$dst_dir; \
		printf qq{make[$(MAKELEVEL)]: coverage: note: example      permanent exclusion comment: /* COVERAGE EXCLUSION: impossible to reach here because <add reason here> */\n}; \
		printf qq{make[$(MAKELEVEL)]: coverage: note: example semi-permanent exclusion comment: /* COVERAGE EXCLUSION: todo: need to add coverage for this block */\n}; \
		printf qq{make[$(MAKELEVEL)]: coverage: note: search  semi-permanent exclusion comments: make convention_to_do | grep -i exclusion\n}; \
		if ( $$r_ever ) { \
		    $$r = 1 ; \
		    printf qq{make[$(MAKELEVEL)]: coverage: ERROR: coverage failed for $(OSPC)d files in $(OSPC)s: @g_ever\n}, $$r_ever, $$sub_lib; \
		} \
		else { \
		    printf qq{make[$(MAKELEVEL)]: coverage: Coverage is acceptable for all files in $(OSPC)s\n}, $$sub_lib; \
		} \
		last if ( $$r ); \
	} \
	exit $$r; \
	$(COPYFILES2DIR_SUB) \
	$(OSQUOTE)
endef

define CC_OUT_ON_ERROR
|| (    $(MAKE_PERL_ECHO_ERROR) "make[$(MAKELEVEL)]: ERROR: Dumping $(call OSPATH,$@.out)" \
     && $(CAT) $(call OSPATH,$@.out) \
     && $(MAKE_PERL_ECHO_ERROR) "make[$(MAKELEVEL)]: ERROR: Dumped $(call OSPATH,$@.out)" \
     && exit 1 \
   )
endef

# This macro used to filter the output through MAKE_PERL_GREP_NOT_OK, hiding what was going on.
#
define TEST_OUT_ON_ERROR
|| (    $(MAKE_PERL_ECHO_ERROR) "make[$(MAKELEVEL)]: ERROR: Dumping full log $(call OSPATH,$(DST.dir)/$(notdir $<).out)" \
     && $(CAT) $(notdir $<).out \
     && $(MAKE_PERL_ECHO_ERROR) "make[$(MAKELEVEL)]: ERROR: Dumped full log $(call OSPATH,$(DST.dir)/$(notdir $<).out)" \
     && exit 1 \
   )
endef

