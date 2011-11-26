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
	while ((scalar(@ARGV) > 1) && ($$ARGV[scalar(@ARGV) - 1] =~ m~\.a$$~)) { \
		my $$lib = pop(@ARGV); \
		printf qq[MAKE_PERL_LIB: debug: expanding .a: $(OSPC)s\n], $$lib if ( $(MAKE_DEBUG) ); \
		-d $$tmp_lib_dir or mkdir($$tmp_lib_dir)          or die(qq[MAKE_PERL_LIB: Cannot make directory $$tmp_lib_dir\n]); \
		system(qq[cp $$lib $$tmp_lib_dir/.]) == 0         or die(qq[MAKE_PERL_LIB: Cannot copy library $$lib into $$tmp_lib_dir\n]); \
		my @expand_libs_with_path = glob(qq[$$tmp_lib_dir/*$(EXT.lib)]); \
		foreach my $$expand_lib_with_path (@expand_libs_with_path) { \
			my ( $$expand_lib ) = $$expand_lib_with_path =~ m~([^\\\/]+)$$~; \
			my $$lib_cmd = qq[cd $$tmp_lib_dir && ar x $$expand_lib]; \
			printf qq[MAKE_PERL_LIB: debug: expanding library: $(OSPC)s\n], $$lib_cmd if ( $(MAKE_DEBUG) ); \
			system($$lib_cmd) == 0                        or die(qq[MAKE_PERL_LIB: Cannot explode library $$_\n]); \
		} \
	} \
	my $$lib_cmd = qq[ar csr ] . join(q[ ], @ARGV) . qq[ ] . join(q[ ],glob(qq[$$tmp_lib_dir/*$(EXT.obj)])); \
	printf qq[MAKE_PERL_LIB: debug: creating library: $(OSPC)s\n], $$lib_cmd if ( $(MAKE_DEBUG) ); \
	system($$lib_cmd) == 0 or die(qq[MAKE_PERL_LIB: Archive command failed: ] . $$lib_cmd); \
	if (-d $$tmp_lib_dir) { \
		rmtree($$tmp_lib_dir) or die(qq[MAKE_PERL_LIB: Cannot recursively remove directory $$tmp_lib_dir\n]); \
	} \
    $(OSQUOTE)
endef

#
# - Example usage:
#   - $(CAT) $(notdir $<).out | $(MAKE_PERL_GREP_NOT_OK)
#

define MAKE_PERL_GREP_NOT_OK
$(PERL) -lane $(OSQUOTE) \
    if ( m~^20\d{6}~ ) { \
        $$debug .= $$_ . qq[\n]; \
    } \
    elsif ( m~^ok ~ ) { \
        $$lines .= $$_ . qq[\n]; \
        undef $$debug; \
    } \
    else { \
        $$lines .= $$debug; \
        $$lines .= $$_ . qq[\n]; \
        undef $$debug; \
    } \
    sub END { \
        printf qq[$(OSPC)s$(OSPC)s\n], $$lines, $$debug; \
        exit 0; \
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
	delete $$ENV{CDPATH}; \
	$$sub_libs = qq[../$(LIBRARIES)]; \
	if ( -e q[$(COM.dir)/test] ) { \
		if ( q[$(COM.dir)] eq q[..] ) { \
			printf qq[make[$(MAKELEVEL)]: coverage: NOTE: top level $(COM.dir)/test/ folder exists; delaying final coverage check for @sub_libs\n]; \
			exit 0; \
		} \
		$$sub_libs = qq[$(ALL_LIBRARIES)]; \
	} \
	@sub_libs = split ( m~\s+~, $$sub_libs ); \
	printf qq[make[$(MAKELEVEL)]: coverage: checking coverage in sub libraries: @sub_libs\n] if ( $(MAKE_DEBUG) ); \
	foreach $$sub_lib ( sort @sub_libs ) { \
		$$sub_lib =~ s~\b([^ ]+)~lib-$$1~ if ( ! -e $$sub_lib ); \
		if ( $$sub_lib !~ m~lib-~ ) { \
			printf qq[make[$(MAKELEVEL)]: coverage: todo: add coverage for non-libraries\n]; \
			next; \
		} \
		printf qq[make[$(MAKELEVEL)]: coverage: checking coverage in sub library: $$sub_lib\n] if ( $(MAKE_DEBUG) ); \
		printf qq[make[$(MAKELEVEL)]: coverage: exists? $$sub_lib/$$os_class/\n] if ( $(MAKE_DEBUG) ); \
		undef $$gcov_output; \
		if ( -d qq[$$sub_lib/$$os_class] ) { \
			printf qq[make[$(MAKELEVEL)]: coverage: creating $$sub_lib/$$dst_dir/$$os_class/\n] if ( $(MAKE_DEBUG) ); \
			mkdir qq[$$sub_lib/$$dst_dir/$$os_class]; \
			@os_class_c_to_copy = glob ( qq[$$sub_lib/$$os_class/*.c] ); \
			printf qq[make[$(MAKELEVEL)]: coverage: updating @os_class_c_to_copy\n] if ( $(MAKE_DEBUG) ); \
			copyfiles2dir_sub ( @os_class_c_to_copy, qq[$$sub_lib/$$dst_dir/$$os_class] ); \
			$$gcov_cmd = qq[cd $$sub_lib/$$dst_dir && gcov $$os_class/*.c 2>&1]; \
			printf qq[make[$(MAKELEVEL)]: coverage: running gcov; $$gcov_cmd\n] if ( $(MAKE_DEBUG) ); \
			$$gcov_output = `$$gcov_cmd`; \
		} \
		@c_to_copy = glob ( qq[$$sub_lib/*.c] ); \
		printf qq[make[$(MAKELEVEL)]: coverage: updating @c_to_copy\n] if ( $(MAKE_DEBUG) ); \
		copyfiles2dir_sub ( @c_to_copy, qq[$$sub_lib/$$dst_dir] ); \
		unlink ( qq[$$sub_lib/$$dst_dir/gcov-stdout.log] ); \
		unlink ( qq[$$sub_lib/$$dst_dir/gcov-stderr.log] ); \
		foreach $$c_file ( @c_to_copy ) { \
			$$gcov_cmd = qq[cd $$sub_lib/$$dst_dir && gcov $$c_file >> gcov-stdout.log 2>> gcov-stderr.log]; \
			printf qq[make[$(MAKELEVEL)]: coverage: running gcov; $$gcov_cmd\n] if ( $(MAKE_DEBUG) ); \
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
		$$gcov_output  =~ s~File [^\r\n]+[\r\n]+~~gs; \
		$$gcov_output  =~ s~Lines executed:[^\r\n]+[\r\n]+~~gs; \
		$$gcov_output  =~ s~[^\s]+:creating [^\r\n]+[\r\n]+~~gs; \
		$$gcov_output  =~ s~[^\s]+source file is newer than graph file [^\r\n]+[\r\n]+~~gs; \
		$$gcov_output  =~ s~[^\s]+cannot open graph file[\r\n]+~~gs; \
		$$gcov_output  =~ s~[^\s]+\:cannot open data file, assuming not executed[\r\n]+~~gs; \
		$$gcov_output  =~ s~\(the message is only displayed one per source file\)[\r\n]+~~gs; \
		while ( $$gcov_output =~ m~([^\s]+)\.gcda\:cannot open data file[\r\n]+~gs ) { \
			print qq[make[$(MAKELEVEL)]: coverage: WARNING: no coverage for file: $$1.c\n]; \
		} \
		$$gcov_output =~ s~[^\s]+\:cannot open data file[\r\n]+~~gs; \
		if ( $$gcov_output !~ m~^\s*$$~s ) { \
			print qq[make[$(MAKELEVEL)]: coverage: WARNING: unexpected output found in lines:\n$$gcov_output2\n]; \
			die(  qq[make[$(MAKELEVEL)]: coverage: FATAL: unexpected gcov line(s) (in above):\n$$gcov_output\n]); \
		} \
		printf qq[make[$(MAKELEVEL)]: coverage: showing exclusions\n] if ( $(MAKE_DEBUG) ); \
		@g = glob ( qq[$$sub_lib/$$dst_dir/*.c.gcov] ); \
		if ( 0 == scalar @g ) { \
			print qq[make[$(MAKELEVEL)]: coverage: WARNING: no *.gcov files found!\n]; \
			@f = glob ( qq[$$sub_lib/$$dst_dir/*.c] ); \
			die( qq[make[$(MAKELEVEL)]: coverage: FATAL: no *.gcov files found and no c files found\n]) if ( 0 == scalar @f ); \
			foreach $$f ( @f ) { \
				open ( IN, q[<], $$f ) || die qq[make[$(MAKELEVEL)]: coverage: ERROR: cannot open c file: $$f]; \
				sysread ( IN, $$fb, 9_999_999 ); \
				close ( IN ) ; \
				die( qq[make[$(MAKELEVEL)]: coverage: FATAL: no *.gcov files found but found coverage exclusion in c file: $$f\n]) if ( $fb =~ m~COVERAGE\s+EXCLUSION~i ); \
			} \
		} \
		undef @x; \
		for $$g ( @g ) { \
			next if $$g =~ /debug/; \
			print qq[make[$(MAKELEVEL)]: coverage: processing: $$g\n] if ( $(MAKE_DEBUG) ); \
			open ( IN, q[<], $$g ) || die qq[make[$(MAKELEVEL)]: coverage: ERROR: cannot open gcov file: $$g]; \
			sysread ( IN, $$f, 9_999_999 ); \
			close ( IN ) ; \
			if ( $$f =~ m~(/\* FILE COVERAGE EXCLUSION.*?\*/)~is ) { \
				print qq[make[$(MAKELEVEL)]: coverage: excluding: $$g\n] if ( $(MAKE_DEBUG) ); \
				push @x, sprintf q{$(OSPC)s: $(OSPC)s}, $$g, uc $$1; \
				next; \
			} \
			$$g =~ s~^(.*)/$(DST.dir)/(.*)\.gcov$$~$$1/$$2~; \
			while ( $$f =~ s~^\s+[\-]+\:.*$$~~gm ) {}; \
			$$lines_covered     += $$f =~ s~^\s+[0-9]+\:.*$$~~gm; \
			$$lines_covered_not += $$f =~ s~^(\s+[\#]+\:.*)$$~$$1~gm; \
			$$f =~ s~^[\r\n]+~~s; \
			foreach $$line ( split(m~\n{2,}~s,$$f) ) { \
				print qq[make[$(MAKELEVEL)]: coverage: line:\n$$line\n] if ( $(MAKE_DEBUG) ); \
				$$c = sprintf q{$(OSPC)s:$(OSPC)d}, $$g, $$line =~ m~:\s*(\d+)~; \
				if ( $$line =~ m~(/\* COVERAGE EXCLUSION.*?\*/)~is ) { \
					push @x, sprintf q{make[$(MAKELEVEL)]: coverage: $(OSPC)s: lines: $(OSPC)s}, $$c, uc $$1; \
					next; \
				} \
				$$r  = 1; \
				$$line =~ s~^(.*)$$~make[$(MAKELEVEL)]: coverage: $$1~gm; \
				$$o .= sprintf qq{make[$(MAKELEVEL)]: coverage: $(OSPC)s: ERROR: Insufficient coverage: \n$(OSPC)s\n}, $$c, $$line; \
			} \
		} \
		print join(qq{\n},@x) . qq{\n} if @x; \
		$$lines_total_to_divide = $$lines_covered + $$lines_covered_not; \
		$$lines_total_to_divide = 0 == $$lines_total_to_divide ? 1 : $$lines_total_to_divide; \
		$$r = 1 if ( 0 == $$lines_covered ); \
		printf qq{$(OSPC)smake[$(MAKELEVEL)]: coverage: Coverage is $(OSPC)s ($(OSPC)5.1f$(OSPC)$(OSPC); lines: $(OSPC)4d covered, $(OSPC)4d not covered for $$sub_lib)\n}, $$o, $$r ? q{insufficient} : q{acceptable}, 100 - $$lines_covered_not / $$lines_total_to_divide * 100, $$lines_covered, $$lines_covered_not; \
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

ifeq ($(filter fulllog,$(MAKECMDGOALS)),)

define TEST_OUT_ON_ERROR
 >> $(notdir $<).out 2>&1 \
|| (    $(MAKE_PERL_ECHO_ERROR) "make[$(MAKELEVEL)]: ERROR: Dumping filtered log $(call OSPATH,$(DST.dir)/$(notdir $<).out)" \
	 && $(CAT) $(notdir $<).out | $(MAKE_PERL_GREP_NOT_OK) \
	 && $(MAKE_PERL_ECHO_ERROR) "make[$(MAKELEVEL)]: ERROR: Dumped filtered log $(call OSPATH,$(DST.dir)/$(notdir $<).out)" \
	 && exit 1 \
   )
endef

else

define TEST_OUT_ON_ERROR
 >> $(notdir $<).out 2>&1 \
|| (    $(MAKE_PERL_ECHO_ERROR) "make[$(MAKELEVEL)]: ERROR: Dumping full log $(call OSPATH,$(DST.dir)/$(notdir $<).out)" \
	 && $(CAT) $(notdir $<).out \
	 && $(MAKE_PERL_ECHO_ERROR) "make[$(MAKELEVEL)]: ERROR: Dumped full log $(call OSPATH,$(DST.dir)/$(notdir $<).out)" \
	 && exit 1 \
   )
endef

endif
