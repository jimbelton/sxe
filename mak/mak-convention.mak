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
#	- @$(MAKE_PERL_LIST_NON-THIRD-PARTY-FILES)
#	- Notes:
#	  - The strategy is to list files where:
#		- The folder already contains a 'GNUmakefile'
#		- Or the folder is the 'test' folder
#		- We look to a depth of two folders
#		  - i.e. Works in root or [exe|lib|mak]-* folder
#

PACKAGES_AND_TESTS := ./GNUmakefile ./test/
ifneq ($(OS),Windows_NT)
PACKAGES_AND_TESTS += ./*/GNUmakefile ./*/test/
endif

define MAKE_PERL_LIST_NON-THIRD-PARTY-FILES
$(PERL) -e $(OSQUOTE) \
	@dirs=glob(q[$(PACKAGES_AND_TESTS)]); \
	foreach $$dir (@dirs) { \
		$$dir =~ s~[^\\\/]*$$~*~; \
		printf STDERR qq[make: .pl: non-third party folder: $(OSPC)s\n], $$dir if exists $$ENV{MAKE_DEBUG}; \
		push @files, glob $$dir; \
	} \
	foreach (@files) { \
		printf qq[$(OSPC)s ], $$_; \
	} \
	$(OSQUOTE)
endef

NON-THIRD-PARTY-FILES := $(filter-out \
    $(addsuffix %, $(abspath $(addprefix $(COM.dir)/, $(CONVENTION_OPTOUT_LIST)))), \
    $(abspath $(shell $(MAKE_PERL_LIST_NON-THIRD-PARTY-FILES))) \
)

#
# - Example usage:
#	- @$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(pl|pm|t)$$" "(?s-xim:^(.*)$$)" present "(?m-ix:(no_plan|skip_all))" exit1 $(FILES)
#	- Where:
#	  - Argument #1: Regex to filter files to be loaded
#	  - Argument #2: Regex to split loaded file into parts
#		- E.g. "(?s-xim:^(.*)$$)" means one part -- the whole file
#	  - Argument #3: How argument #4 is interpreted (present|missing)
#	  - Argument #4: Regex to be performed on each part
#	  - Argument #5: How to exit if argument #3 hits (exit0|exit1)
#	  - Argument #6+: File names to grep
#

define MAKE_PERL_GREP3_WITHOUT_ARGUMENT
$(PERL) -e $(OSQUOTE) \
	$$rs=shift @ARGV; \
	$$r1=shift @ARGV; \
	$$r2_cond=shift @ARGV; \
	$$r2=shift @ARGV; \
	$$e1=shift @ARGV; \
	for $$s(sort @ARGV){ \
		next if($$s!~m~$$rs~); \
		next if(not -f $$s); \
		open(IN,q[<],$$s); \
		sysread(IN,$$f,9_999_999); \
		close(IN); \
		next if($$e1 eq q{exit1} && $$f=~m~[C]ONVENTION EXCLUSION~); \
		$$f=~s~(\n\r|\n)~\r\n~gis; \
		if(0){`echo insert file name & line numbers`} \
		$$n=0; \
		$$f=~s~^(.*)(?{$$n++})$$~$$s:$$n: $$1~gim; \
		$$f=~s~\r~~gis; \
		printf qq{make: .pl: scanning: $(OSPC)4d lines: $(OSPC)s\n},$$n,$$s if $$ENV{MAKE_DEBUG} ; \
		while($$f=~m~$$r1~g) { \
			@p=split m~\n~,$$1; \
			@l=eval qq[grep m~$$r2~,\@p]; \
			if($$r2_cond=~m~present~i) { \
				foreach(@l){ \
					$$match_count ++; \
					printf qq{$(OSPC)s (match #$(OSPC)d)\n},$$_,$$match_count;\
				} \
			} elsif($$r2_cond=~m~missing~i) { \
				if(0 == scalar @l) { \
					$$match_count ++; \
					printf qq{$(OSPC)s (something is missing before this line!) (match #$(OSPC)d)\n},$$p[$$#p],$$match_count; \
				} \
			} else { \
				die qq[ERROR: please specify 'present' or 'missing']; \
			}\
		} \
	} \
	exit 1 if(($$e1 eq q[exit1]) && $$match_count); \
	$(OSQUOTE)
endef

.PHONY : \
	convention_no_debug_instrumentation_with_goto_out_block \
	convention_no_instrumentation_or_goto_in_lock \
	convention_uppercase_hash_define_and_typedef \
	convention_exit_preceded_by_entry \
	convention_entry_followed_by_exit \
	convention_no_sprintf_in_c_files \
	convention_no_eol_whitespace \
	convention_no_indented_label \
	convention_uppercase_label \
	convention_no_hash_if_0 \
	convention_convention_exclusion \
	convention_no_fixme \
	convention_no_tab \
	convention_usage \
	convention

convention_usage :
	@echo usage: make convention_no_debug_instrumentation_with_goto_out_block
	@echo usage: make convention_no_instrumentation_or_goto_in_lock
	@echo usage: make convention_uppercase_hash_define_and_typedef
	@echo usage: make convention_exit_preceded_by_entry
	@echo usage: make convention_entry_followed_by_exit
	@echo usage: make convention_no_sprintf_in_c_files
	@echo usage: make convention_convention_exclusion
	@echo usage: make convention_no_eol_whitespace
	@echo usage: make convention_no_indented_label
	@echo usage: make convention_uppercase_label
	@echo usage: make convention_no_hash_if_0
	@echo usage: make convention_no_fixme
	@echo usage: make convention_no_tab
	@echo usage: make convention_to_do
	@echo usage: make convention

#	convention_to_do			- doesn't exit1

# NOTE: convention_no_fixme is the last check so that code containing 'fixme' may be checked for convention failures before it is ready to commit.
convention : \
	convention_no_debug_instrumentation_with_goto_out_block \
	convention_no_instrumentation_or_goto_in_lock \
	convention_uppercase_hash_define_and_typedef \
	convention_exit_preceded_by_entry \
	convention_entry_followed_by_exit \
	convention_no_sprintf_in_c_files \
	convention_no_eol_whitespace \
	convention_no_indented_label \
	convention_uppercase_label \
	convention_no_hash_if_0 \
	convention_no_tab \
	convention_no_fixme \
	convention_convention_exclusion
	@echo make: all convention checks passed!

convention_no_debug_instrumentation_with_goto_out_block:
	@$(MAKE_PERL_ECHO) "make: checking convention that .c files should avoid block with debug log and goto OUT"
	@$(MAKE_PERL_ECHO) "hint: consider indenting goto OUT in its own block if you are really sure!"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(c)$$" "(?s-xim:(\{[^\{\}]+?goto\s+OUT[^\{\}]+\}))" present "(?m-ix:(SXEL6))" exit1 $(NON-THIRD-PARTY-FILES)

convention_no_sprintf_in_c_files:
	@$(MAKE_PERL_ECHO) "make: checking convention that .c files should avoid sprintf"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(c|h)$$" "(?s-xim:^(.*)$$)" present "(?m-ix:([^a-z_]sprintf))" exit1 $(NON-THIRD-PARTY-FILES)

convention_entry_followed_by_exit:
	@$(MAKE_PERL_ECHO) "make: checking convention that SXEE?? macro followed by SXER?? macro"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(c)$$" "(?s-xim:SXEE[0-9][0-9](.+?)SXER[0-9][0-9])" present "(?m-ix:SXE[ER][0-9][0-9])" exit1 $(NON-THIRD-PARTY-FILES)

convention_exit_preceded_by_entry:
	@$(MAKE_PERL_ECHO) "make: checking convention that SXER?? macro preceded by SXEE?? macro"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(c)$$" "(?s-xim:(.+?)\s+SXER[0-9][0-9])" missing "(?mx-i:SXE[E][0-9][0-9])" exit1 $(NON-THIRD-PARTY-FILES)

convention_no_indented_label:
	@$(MAKE_PERL_ECHO) "make: checking convention for no indented labels"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(c|h)$$" "(?s-xim:^(.*)$$)" present "(?m-ix:^[^:]+:\d+:[ ][ \t]+[A-Z0-9_]+:)" exit1 $(NON-THIRD-PARTY-FILES)

convention_uppercase_label:
	@$(MAKE_PERL_ECHO) "make: checking convention for incorrect case for label"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(c|h)$$" "(?s-xim:^(.*)$$)" present "(?m-ix:^[^:]+:\d+:[ ]+[a-z0-9_]+:)(?=default:)" exit1 $(NON-THIRD-PARTY-FILES)

convention_uppercase_hash_define_and_typedef:
	@$(MAKE_PERL_ECHO) "make: checking convention for incorrect case for hash define or typedef"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(c|h)$$" "(?s-xim:^(.*)$$)" present "(?m-ix:(#[ ]*define|typedef[ ]+(struct|enum))[ ]+[a-z][a-z0-9_]+)" exit1 $(NON-THIRD-PARTY-FILES)

convention_no_fixme:
	@$(MAKE_PERL_ECHO) "make: checking convention for fixme"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(c|h|pl|pm|t)$$" "(?s-xim:^(.*)$$)" present "(?im-x:fixme)" exit1 $(NON-THIRD-PARTY-FILES)

convention_no_hash_if_0:
	@$(MAKE_PERL_ECHO) "make: checking convention for hash if 0"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(c|h)$$" "(?s-xim:^(.*)$$)" present "(?m-ix:^[^:]+:\d+:[ ]+#[ ]*if[ ]+0)" exit1 $(NON-THIRD-PARTY-FILES)

convention_no_tab:
	@$(MAKE_PERL_ECHO) "make: checking convention for tabs"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(c|h|pl|pm|t)$$" "(?s-xim:^(.*)$$)" present "(?m-ix:[\x09]+)" exit1 $(NON-THIRD-PARTY-FILES)

convention_no_eol_whitespace:
	@$(MAKE_PERL_ECHO) "make: checking convention for end-of-line whitespace"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(c|h|pl|pm|t)$$" "(?s-xim:^(.*)$$)" present "(?m-ix:^[^:]+:\d+:[ ]+[^ ].+[ ]+$$)" exit1 $(NON-THIRD-PARTY-FILES)

convention_no_instrumentation_or_goto_in_lock:
	@$(MAKE_PERL_ECHO) "make: checking convention for use of log instrumentation or goto while locking"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.(c)$$" "(?s-xim:SXE_OEM_MACRO_SPINLOCK(?:_QUIET_|_)LOCK(.+?)SXE_OEM_MACRO_SPINLOCK(?:_QUIET_|_)UNLOCK)" present "(?m-ix:(SXE[ELRA][0-9][0-9]|goto))" exit1 $(NON-THIRD-PARTY-FILES)

convention_to_do:
	@$(MAKE_PERL_ECHO) "make: checking convention for [t]odo"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.*$$" "(?s-xim:^(.*)$$)" present "(?im-x:[t]odo)" exit0 $(NON-THIRD-PARTY-FILES)

convention_convention_exclusion:
	@$(MAKE_PERL_ECHO) "make: finding convention exclusions"
	@$(MAKE_PERL_GREP3_WITHOUT_ARGUMENT) "\.*$$" "(?s-xim:^(.*)$$)" present "(?im-x:[C]ONVENTION\sEXCLUSION)" exit0 $(NON-THIRD-PARTY-FILES)
