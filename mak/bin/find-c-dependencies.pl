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

use strict;
#fixme del use FindBin qw($Bin);
#fixme del use File::Glob;
#old use Data::Dump;
use Time::HiRes;
use Cwd;
use File::Path;

$| ++;

#todo: build on linux ramdisk? http://www.vanemery.com/Linux/Ramdisk/ramdisk.html

#todo: build on win32 ramdisk? http://www.mydigitallife.info/2007/05/27/free-ramdisk-for-windows-vista-xp-2000-and-2003-server/

#fixme: todo: add fixme for makefiles to convention :-)

my   $dst_dir    = $ARGV[0];                     # e.g. build-linux-32-debug
my   $top_dir    = $ARGV[1];                     # e.g. ./.. or ../..
my   $os_class   = $ARGV[2];                     # e.g. any-unix
my   $os_name    = $ARGV[3];                     # e.g. linux
my   $make_dir   = $ARGV[4];                     # e.g. .. or /mnt/rd
my ( $ext_obj  ) = $ARGV[5]  =~ m~EXT.obj=(.*)~; # e.g. .obj
my ( $ext_lib  ) = $ARGV[6]  =~ m~EXT.lib=(.*)~; # e.g. .lib
my ( $ext_exe  ) = $ARGV[7]  =~ m~EXT.exe=(.*)~; # e.g. .exe
my   $cc_inc     = $ARGV[8];                     # e.g. /I
my ( $debug    ) = $ARGV[9]  =~ m~DEBUG=(.*)~;   # e.g. 0|1

my ( $coverage ) = $dst_dir =~ m~(coverage)~;    # e.g. undef|coverage

my $Bin = Cwd::getcwd;
   $Bin =~       s~/[^/]+$~~ if ( $top_dir eq    '..' ); # remove trailing folder
   $Bin =~ s~/[^/]+/[^/]+$~~ if ( $top_dir eq '../..' ); # remove trailing folder
my $env_log_levels;
my $env_log_level = 0;
#fixme delmy $h;
my $s;
my $mak;
my $mak_defs;
my $mak_all;
my $mak_folders;
my $mak_exe;
my $mak_dummy;
my $mak_inc;
my $mak_src;
my $tab = qq[\t];
my $deps;
my $mods; # list of            modules & parent parcel
my $m3rd; # list of thirdparty modules & parent parcel
my $syml; # symbolic link from file to path/file
my $thir; # is file third-party?
my $link; # .lib & .exe files to link
my $path; # $(MAKE.dir)/$(DST.dir)/... pathes to be created
my $incs; # remember how file included, i.e. with or without path, e.g. "ev.h" or <sys/time.h>
my $phony; # high level phony targets, e.g. libsxe, lib-sxe, lib-sxe-pool, etc
my $obj_deps_as_mak;
my $clean_lines;

printf STDERR qq[%f - %s v1.0; \$(DST.dir)=$dst_dir, \$(TOP.dir)=$top_dir \$(COVERAGE)=$coverage DEBUG=$debug\n], Time::HiRes::time, $0;

sub absolute_path_parts {
	my ( $absolute_path ) = @_;
	     $absolute_path =~ s~^$Bin/~~;
	my                                                                ( $parcel, $module, $file_stem, $file_extension );
	if    ( $absolute_path =~ m~^([^/]+)$~                        ) { ( $parcel                                       ) = ( $1             ); }
	elsif ( $absolute_path =~ m~^([^/]+)/([^/]+)$~                ) { ( $parcel, $module                              ) = ( $1, $2         ); }
	elsif ( $absolute_path =~ m~^([^/]+)/([^/]+)/([^\.]+)\.*(.*)$~) { ( $parcel, $module, $file_stem, $file_extension ) = ( $1, $2, $3, $4 ); }
	#else                            { ( $parcel, $module, $file_stem, $file_extension ) = $absolute_path =~ m~^(.*)/(.*)/([^\.]+)\.*(.*)$~; }
	#debug printf qq[parcel %s, module %s, stem %s, extension %s from %s\n], $parcel, $module, $file_stem, $file_extension, $absolute_path;
	my $file = sprintf qq[%s%s%s], $file_stem, $file_extension eq '' ? '' : '.',$file_extension;
	return $parcel, $module, $file_stem, $file_extension, $file;
}

#todo log level $env_log_levels->{SXE_LOG_LEVEL} = $env_log_level ++;
my $total_bytes_source;
my $total_lines_source;
my $total_lines_source_test;
my @absolute_parcels = sort grep -d, glob qq[$Bin/*];
#fixme foreach my $absolute_parcel ( @absolute_parcels ) {
foreach my $absolute_parcel ( 'libsxe' ) {
	my ( $parcel, undef, undef, undef ) = absolute_path_parts ( $absolute_parcel );

	my @absolute_modules = sort grep -d, glob qq[$Bin/$parcel/exe-* $Bin/$parcel/dll-* $Bin/$parcel/lib-*];
	foreach my $absolute_module ( @absolute_modules ) {
		my ( undef, $module, undef, undef ) = absolute_path_parts ( $absolute_module );

		my $source_standard           = sprintf qq[ $Bin/$parcel/$module/*.c      $Bin/$parcel/$module/*.h]                           ;
		my $source_platform_specific  = sprintf qq[ $Bin/$parcel/$module/%s/*.c   $Bin/$parcel/$module/%s/*.h]  , $os_class, $os_class;
		   $source_platform_specific .= sprintf qq[ $Bin/$parcel/$module/%s/*/*.c $Bin/$parcel/$module/%s/*/*.h], $os_class, $os_class;
		my $source_test               = sprintf qq[ $Bin/$parcel/$module/test/*.c $Bin/$parcel/$module/test/*.h]                      ; #  $Bin/$parcel/$module/test/*.pl

		my $source_third_party;
        my @absolute_thirdparties = grep -d, glob qq[$Bin/$parcel/$module/*];
        my $non_thirdparty_folders = "any-unix|any-winnt|test|build-[^/]+";
        foreach my $absolute_thirdparty ( @absolute_thirdparties ) {
            my ( undef, undef, undef, undef, $thirdparty ) = absolute_path_parts ( $absolute_thirdparty );
            next if ( $thirdparty =~ m~^($non_thirdparty_folders)$~ );
            #debug printf STDERR qq[%f - input:%7s bytes:%5s lines: %-15s %-20s - %s (thirdparty)\n], Time::HiRes::time, 'n/a', 'n/a', "/$parcel", "/$module", $thirdparty;
            $source_third_party .= sprintf qq[ $Bin/$parcel/$module/$thirdparty/*.c $Bin/$parcel/$module/$thirdparty/*.h];
            my @absolute_thirdparties_sub = grep -d, glob qq[$Bin/$parcel/$module/$thirdparty/*];
            foreach my $absolute_thirdparty_sub ( @absolute_thirdparties_sub ) {
                my ( undef, undef, undef, undef, $thirdparty_sub ) = absolute_path_parts ( $absolute_thirdparty_sub );
                #debug printf STDERR qq[%f - input:%7s bytes:%5s lines: %-15s %-20s - %s (thirdparty_sub)\n], Time::HiRes::time, 'n/a', 'n/a', "/$parcel", "/$module", $thirdparty_sub;
                $source_third_party .= sprintf qq[ $Bin/$parcel/$module/$thirdparty/$thirdparty_sub/*.c $Bin/$parcel/$module/$thirdparty/$thirdparty_sub/*.h];
            }
        } # foreach my $@absolute_thirdparty

		my @absolute_sources = sort glob $source_standard . $source_platform_specific . $source_test . $source_third_party;
		   @absolute_sources = sort glob qq[$Bin/$parcel/$module/GNUmakefile] if ( 0 == scalar @absolute_sources );
		foreach my $absolute_source ( @absolute_sources ) {
			my ( undef, undef, undef, undef, $file ) = absolute_path_parts ( $absolute_source );

						open    ( SOURCE, q[<], $absolute_source );
			my $bytes = sysread ( SOURCE, my $source, 999_999 );
						close   ( SOURCE );

			my $lines = $source =~ s~\n~\n~gs;

			$total_bytes_source      += $bytes;
			$total_lines_source      += $lines;
			$total_lines_source_test += $lines if ( $file =~ m~^test/~ );

			#todo log level my $env_log_level_key_1 = sprintf qq[SXE_LOG_LEVEL__%s]        , $parcel;
			#todo log level my $env_log_level_key_2 = sprintf qq[SXE_LOG_LEVEL__%s__%s]    , $parcel, $module;
			#todo log level my $env_log_level_key_3 = sprintf qq[SXE_LOG_LEVEL__%s__%s__%s], $parcel, $module, $file;
			#todo log level    $env_log_level_key_1 = uc ( $env_log_level_key_1 );
			#todo log level    $env_log_level_key_2 = uc ( $env_log_level_key_2 );
			#todo log level    $env_log_level_key_3 = uc ( $env_log_level_key_3 );
			#todo log level    $env_log_level_key_1 =~ s~[^a-z0-9\_]+~\_~gi;
			#todo log level    $env_log_level_key_2 =~ s~[^a-z0-9\_]+~\_~gi;
			#todo log level    $env_log_level_key_3 =~ s~[^a-z0-9\_]+~\_~gi;
			#todo log level $env_log_levels->{$env_log_level_key_1} = $env_log_level ++ if ( not exists $env_log_levels->{$env_log_level_key_1} );
			#todo log level $env_log_levels->{$env_log_level_key_2} = $env_log_level ++ if ( not exists $env_log_levels->{$env_log_level_key_2} );
			#todo log level $env_log_levels->{$env_log_level_key_3} = $env_log_level ++ if ( not exists $env_log_levels->{$env_log_level_key_3} );

            die qq[Error: duplicate source file detected: $file] if ( exists $deps->{$file} );
            $deps->{$file}{Parcel} = $parcel;
            $deps->{$file}{Module} = $module;

            if ( $file =~ m~/~ ) {
                # come here if file contains folder
                if ( $file =~ m~^($non_thirdparty_folders)/~ ) {
                    # come here if file is in one of our folders, e.g. test/foo.c
                }
                else {
                    # come here if file is in thirdparty folder, e.g. libev/ev.h
                    $thir->{$file} ++;
                }
            }

            $mods->{$module}{Parcel} = $parcel;
            $m3rd->{$parcel}{$module}{$file} ++ if ( exists $thir->{$file} );

            #del if ( $file =~ m~/([^/]+)$~ ) {
            #del     my ( $naked_file ) = ( $1 );
            #del     die qq[Error: duplicate source file detected: $naked_file (when creating naked symlink to $file)] if ( exists $syml->{$naked_file} );
            #del     $syml->{$naked_file} = $file;
            #del }
            my $file_tmp = $file;
            while ( $file_tmp =~ s~^[^/]+/~~ ) {
                die qq[Error: duplicate source file detected: $file_tmp (when creating naked symlink to $file)] if ( exists $syml->{$file_tmp} );
                $syml->{$file_tmp} = $file;
                #debug printf STDERR qq[foo: %s\n], $file_tmp;
            }

            my $quote_dependent_count = 0;
            while ( $source =~ m~#\s*include\s*[<"]([^">]+)~gs ) {
                my ( $dependency ) = ( $1 );
                #debug printf qq[%f         %7s       %5s        %-15s %-20s   - %s\n], Time::HiRes::time, '', '', '', '', $dependency;
                $quote_dependent_count ++;
                $deps->{$file}{Inc}{$dependency} ++;
                $incs->{$dependency} ++; # remember how file included, e.g. "ev.h" or <sys/time.h>
            }

			printf STDERR qq[%f - input:%7d bytes:%5d lines: %-15s %-20s - (%02d incs) %s%s\n], Time::HiRes::time, $bytes, $lines, "/$parcel", "/$module", $quote_dependent_count, $file, exists $thir->{$file} ? ' (3rd)' : '' if ( $debug );

		} # foreach my $absolute_source

		#del #if ( $absolute_sources[0] =~ m~GNUmakefile~i ) {
		#del 	my @absolute_thirdparties = grep -d, glob qq[$Bin/$parcel/$module/*];
		#del 	foreach my $absolute_thirdparty ( @absolute_thirdparties ) {
		#del 		my ( undef, undef, undef, undef, $file ) = absolute_path_parts ( $absolute_thirdparty );
        #del         next if ( $file =~ m~^(any-unix|any-winnt|test|build-.+)$~ );
		#del 		printf STDERR qq[%f - input:%7s bytes:%5s lines: %-15s %-20s - %s (thirdparty)\n], Time::HiRes::time, 'n/a', 'n/a', "/$parcel", "/$module", $file;
		#del 	} # foreach my $@absolute_thirdparty
		#del #}
	} # foreach my $absolute_module

    my $d = <<EOL;
MAKEFLAGS=--no-builtin-rules
.SUFFIXES:

EOL

    printf STDERR qq[- writing dependency rules for found .h files\n] if ( $debug );
    my $deps_unknown;
    foreach my $file ( sort keys %{ $deps } ) {
        next if ( $file !~ m~\.h$~ );
        my $parcel = $deps->{$file}{Parcel};
        my $module = $deps->{$file}{Module};
        printf STDERR qq[  - %-50s -> \$(MAKE.dir)/\$(DST.dir)/$parcel/$file\n], "$module/$file" if ( $debug );
        my $naked_file =  $file;
           $naked_file =~ s~^any\-[^/]+/~~; # remove any-unix or any-winnt
        if ( exists $incs->{$naked_file} ) {
            # come here and do nothing because e.g. #include "test/common.h" #include "sys/time.h" exists
        }
        else {
            # come here if e.g. #include "libev/ev.h" does not exist
            $naked_file =~ s~^.+/~~; # remove e.g. libev/ leaving just ev.h
        }
        if ( $naked_file =~ m~^(.+)/~ ) {
            # come here to add e.g. sys/ to $path hash for bulk mkdir later
            my ( $naked_path ) = ( $1 );
            $path->{"$make_dir/$dst_dir/$parcel/$naked_path"} ++;
            $clean_lines->{"\$(RMDIR) \$(call OSPATH,\$(MAKE.dir)/\$(DST.dir)/$parcel/$naked_path) \$(TO_NUL) \$(FAKE_PASS)"} ++ if ( not exists $m3rd->{$parcel}{$module} );
        }
        my $src_h = "\$(TOP.dir)/$parcel/$module/$file";
        my $dst_h = "\$(MAKE.dir)/\$(DST.dir)/$parcel/$naked_file";
        $d .= <<EOL;
$dst_h: $src_h
\t\@\$(MAKE_PERL_ECHO) "make: building: \$\@"
\t\$(COPY) \$(call OSPATH,$src_h) \$(call OSPATH,$dst_h)

EOL
        $clean_lines->{"\$(DEL) \$(call OSPATH,$dst_h) \$(TO_NUL) \$(FAKE_PASS)"} ++ if ( not exists $m3rd->{$parcel}{$module} );
    } # foreach my $file

    printf STDERR qq[- writing dependency rules for found .c files\n] if ( $debug );
    my $deps_unknown;
    foreach my $file ( sort keys %{ $deps } ) {
        next if ( exists $thir->{$file} ); # don't write rules for thirdparty files, e.g. libev, tap, etc
        next if ( $file =~ m~\.h$~ ); # don't write rules for *.h files (*-proto.h rules are handled separately below)
        my $module = $deps->{$file}{Module};
        printf STDERR qq[  - %s/%s\n], $module, $file if ( $debug );
        my $d_tmp;
        my $inc_unique;
        my @incs = keys %{ $deps->{$file}{Inc} };
        my $to_incs;
        foreach my $inc ( @incs ) {
            if ( not exists $inc_unique->{$inc} ) {
                $inc_unique->{$inc} ++;
                if ( not exists $deps->{$inc} ) {
                    if ( exists $syml->{$inc} ) {
                        #debug printf STDERR qq[DEBUG: mapping %s to %s\n], $inc, $syml->{$inc};
                        $inc = $syml->{$inc};
                    }
                }
                my $module;
                if ( exists $deps->{$inc} ) {
                    push @incs, keys %{ $deps->{$inc}{Inc} };
                    $module = $deps->{$inc}{Module};
                }
                else {
                    $deps_unknown->{$inc} ++;
                    if ( $inc =~ m~^(lib\-.+?)\-proto.h~ ) {
                        # come here if building *-proto.h file for a whole module
                        ( $module ) = ( $1 );
                        die qq[ERROR: discovered reference to lib-*-proto.h ($file) but associated module ($module) does not exist] if ( not exists $mods->{$module} );
                    } elsif ( $inc =~ m~^(.+?)\-proto.h~ ) {
                        # come here if building *-proto.h file for a single c file
                        my $c_file = $1 . '.c';
                        die qq[ERROR: discovered reference to *-proto.h ($file) but associated c file ($c_file) does not exist] if ( not exists $deps->{$c_file} );
                        $module = $deps->{$c_file}{Module};
                    } else {
                        next; # skip something not found, e.g. windows.h
                    }
                    $module .= qq[/\$(DST.dir)];
                }
                next if ( $inc eq 'config.h' ); # fixme: special case for libev/config.h
                printf STDERR qq[    -%s%s\n], exists $deps->{$inc} ? ' ' : '*', $inc if ( $debug );
                die qq[ERROR: discovered reference to unknown header file: $inc] if ( not defined $module );
                #del my $naked_inc =  $inc;
                #del    $naked_inc =~ s~^.*/~~; # remove optional path
                $to_incs->{$inc} ++;
            }
            $inc_unique->{$inc} ++;
        } # foreach my $inc
        foreach my $to_inc ( sort keys %{ $to_incs } ) {
            my $naked_to_inc =  $to_inc;
               $naked_to_inc =~ s~^any\-[^/]+/~~; # remove any-unix or any-winnt
            if ( exists $incs->{$naked_to_inc} ) {
                # come here and do nothing because e.g. #include "test/common.h" #include "sys/time.h" exists
            }
            else {
                # come here if e.g. #include "libev/ev.h" does not exist
                $naked_to_inc =~ s~^.+/~~; # remove e.g. libev/ leaving just ev.h
            }
            $d_tmp .= <<EOL;
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$naked_to_inc \\
EOL
        }
        my $file_as_obj = $file;
           $file_as_obj =~ s~\.c$~$ext_obj~; # e.g. sxe.c becomes sxe.obj
        $d_tmp =~ s~^(.+)\n$~\n$1~s;
        my $d_tmp_deps = <<EOL;
    \$(TOP.dir)/$parcel/$module/$file \\$d_tmp
    \$(TOP.dir)/$parcel/GNUmakefile
EOL
        $d_tmp_deps =~ s~\s+$~~;
        $obj_deps_as_mak->{$file_as_obj} =  $d_tmp_deps;
        $obj_deps_as_mak->{$file_as_obj} =~ s~    \$\(MAKE.dir\)~        \$\(MAKE.dir\)~gs; # cosmetic indent
        $obj_deps_as_mak->{$file_as_obj} =~ s~    \$\(TOP.dir\)~        \$\(TOP.dir\)~gs;   # cosmetic indent
#old         $d .= <<EOL;
#old \$(MAKE.dir)/\$(DST.dir)/$parcel/$module/$file_as_obj: \\
#old     \$(TOP.dir)/$parcel/$module/$file \\$d_tmp
#old     \$(TOP.dir)/$parcel/GNUmakefile
#old \t\@\$(MAKE_PERL_ECHO) "make: building: \$\@"
#old \t\$(CC) \$(CFLAGS) \$(CC_INC)\$(MAKE.dir)/\$(DST.dir)/$parcel \$(CC_INC)\$(MAKE.dir)/\$(DST.dir)/$parcel/$os_class \$(CC_INC)\$(MAKE.dir)/\$(DST.dir)/$parcel/test \$(CFLAGS_PROD) \$(RELEASE_CFLAGS) \$(COVERAGE_CFLAGS) \$(TOP.dir)/$parcel/$module/$file \$(CC_OUT)\$\@
#old
#old EOL
        if ( $file_as_obj =~ m~^test/~ ) {
            $d .= <<EOL;
\$(MAKE.dir)/\$(DST.dir)/$parcel/$module/$file_as_obj: \\
$d_tmp_deps
\t\@\$(MAKE_PERL_ECHO) "make: building: test obj  : \$\@"
\t\$(CC) \$(CFLAGS) \$(CC_INC)\$(MAKE.dir)/\$(DST.dir)/$parcel \$(CC_INC)\$(MAKE.dir)/\$(DST.dir)/$parcel/$os_class \$(CC_INC)\$(MAKE.dir)/\$(DST.dir)/$parcel/test \$(CFLAGS_PROD) \$(RELEASE_CFLAGS) \$(COVERAGE_CFLAGS) \$(TOP.dir)/$parcel/$module/$file \$(CC_OUT)\$\@

EOL
        }
        else {
            $d .= <<EOL;
\$(MAKE.dir)/\$(DST.dir)/$parcel/$module/$file_as_obj: \\
$d_tmp_deps
\t\@\$(MAKE_PERL_ECHO) "make: building: \$\@"
\t\$(CC) \$(CFLAGS) \$(CC_INC)\$(MAKE.dir)/\$(DST.dir)/$parcel \$(CC_INC)\$(MAKE.dir)/\$(DST.dir)/$parcel/$os_class \$(CC_INC)\$(MAKE.dir)/\$(DST.dir)/$parcel/test \$(CFLAGS_PROD) \$(RELEASE_CFLAGS) \$(COVERAGE_CFLAGS) \$(TOP.dir)/$parcel/$module/$file \$(CC_OUT)\$\@

EOL
        }
        $link->{$module}{$file_as_obj} ++;
        my ( $file_as_obj_path ) = $file_as_obj =~ m~^(.*)/~;
             $file_as_obj_path   = '/' . $file_as_obj_path if ( defined $file_as_obj_path );
        $path->{"$make_dir/$dst_dir/$parcel/$module$file_as_obj_path"} ++;
    } # foreach my $file



    printf STDERR qq[- writing rules for recreating *.[lib|exe] files\n] if ( $debug );
    my $lib_objs_parcel;
    my $parcel_as_lib = $parcel . $ext_lib;
    my $d_lib_parcel_thirdparty;
    my $d_lib_parcel = <<EOL;
\$(MAKE.dir)/\$(DST.dir)/$parcel/$parcel_as_lib: \\
EOL
    my $parcel_as_lib_phony =  $parcel_as_lib;
       $parcel_as_lib_phony =~ s~\.[^\.]+$~~; # remove .a or .lib
    my $d_lib_parcel_phony = <<EOL;
.PHONY: $parcel_as_lib_phony
$parcel_as_lib_phony: \\
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$parcel_as_lib \\
EOL
    $phony->{$parcel_as_lib_phony} ++;
    foreach my $module ( sort keys %{ $m3rd->{$parcel} } ) {
        my ( $module_type ) = $module =~ m~^(lib|exe)\-~;
        my $module_as_lib  =  $module;
           $module_as_lib  =~ s~^(lib|exe)\-~~;
           $module_as_lib .=  $ext_lib if ( $module_type eq 'lib' );
           $module_as_lib .=  $ext_exe if ( $module_type eq 'exe' );
        my @module_files   = keys %{ $m3rd->{$parcel}{$module} };
        my ( $module_in_mod ) = $module_files[0] =~ m~^([^/]+)~;
        printf STDERR qq[  - %s (3rd)\n], $module_as_lib if ( $debug );
        my $d_dst;
        my $naked_module =  uc ( $module );
           $naked_module =~ s~^lib\-~~i; # e.g. 'ev' or 'tap'
        my $thirdparty_lib_objects_key = qq[THIRDPARTY_LIB_${naked_module}_OBJECTS];
        my $thirdparty_makefile = qq[$top_dir/$parcel/$module/thirdparty.mak];
        open ( MAK, '<', $thirdparty_makefile ) || die qq[ERROR: can't open thirdparty makefile: $thirdparty_makefile];
        sysread ( MAK, my $mak, 999_999 );
        close ( MAK );
        $mak =~ s~[ ]+\\[\n\r]+[ ]+~ ~gs; # fold continued lines ending  in '\'
        my ( $thirdparty_lib_objects ) = $mak =~ m~$thirdparty_lib_objects_key\s*=\s*([^\n\r]+)~s;
        die qq[ERROR: can't extract value for $thirdparty_lib_objects_key in thirdparty makefle: $thirdparty_lib_objects_key] if ( not defined $thirdparty_lib_objects );
        $d_lib_parcel_thirdparty .= <<EOL;
    $thirdparty_lib_objects \\
EOL
        my $d_src;
        foreach my $file ( sort keys %{ $m3rd->{$parcel}{$module} } ) {
            printf STDERR qq[    - %s (3rd)\n], $file if ( $debug );
            $d_src .= <<EOL;
    \$(TOP.dir)/$parcel/$module/$file \\
EOL
            $d_dst .= <<EOL;
\$(MAKE.dir)/\$(DST.dir)/$parcel/$module/$file \\
EOL
        }
        $d_dst     =~ s~\s*\\\s*$~: \\\n~s;
        $d_src     =~ s~\s*\\\s*$~\n~s;
        $d_src    .=  <<EOL;
\t\@\$(MAKE_PERL_ECHO) "make: building: \$\@ (3rd)"
\t\$(COPYDIR) \$(call OSPATH,\$(TOP.dir)/$parcel/$module) \$(call OSPATH,\$(MAKE.dir)/\$(DST.dir)/$parcel/$module)

EOL
        $d .= $d_dst;
        $d .= $d_src;
        $path->{"$make_dir/$dst_dir/$parcel/$module"} ++;
    } # foreach my $module
    my $phony_parcel_tests;
    my $phony_parcel_runs;
    foreach my $module ( sort keys %{ $link } ) {
        my ( $module_type ) = $module =~ m~^(lib|exe)\-~;
        my $module_as_lib  =  $module;
           $module_as_lib  =~ s~^(lib|exe)\-~~;
           $module_as_lib .=  $ext_lib if ( $module_type eq 'lib' );
           $module_as_lib .=  $ext_exe if ( $module_type eq 'exe' );
        printf STDERR qq[  - %s\n], $module_as_lib if ( $debug );
        $clean_lines->{"\$(RMDIR) \$(call OSPATH,\$(MAKE.dir)/\$(DST.dir)/$parcel/$module) \$(TO_NUL) \$(FAKE_PASS)"} ++ if ( not exists $m3rd->{$parcel}{$module} );
        my $d_lib = <<EOL;
\$(MAKE.dir)/\$(DST.dir)/$parcel/$module_as_lib: \\
EOL
        my $d_lib_phony_target =  $module;
        my $d_lib_phony = <<EOL;
.PHONY: $d_lib_phony_target
$d_lib_phony_target: \\
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$module_as_lib \\
EOL
        $phony->{$d_lib_phony_target} ++;
        $d_lib_parcel .= <<EOL;
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$module_as_lib \\
EOL
        $d_lib_parcel_phony .= <<EOL;
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$module_as_lib \\
EOL
        my $lib_objs;
        my $lib_objs_test; # e.g. test/common.obj i.e. *not* test/test-* which creates a .exe
        foreach my $file_as_obj ( sort keys %{ $link->{$module} } ) {
            if ( $file_as_obj =~ m~^test/~ ) {
                if ( $file_as_obj !~ m~^test/test\-~ ) {
                    $lib_objs_test .= <<EOL;
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$module/$file_as_obj \\
$obj_deps_as_mak->{$file_as_obj} \\
EOL
                }
            }
            if ( $file_as_obj =~ m~^test/~ ) {
#del                 next if ( $file_as_obj !~ m~^test/test\-~ );
#del                 my $file_as_exe =  $file_as_obj;
#del                    $file_as_exe =~ s~$ext_obj~$ext_exe~;
#del                 printf STDERR qq[    ~ %s\n], $file_as_exe;
#del                 $d .= <<EOL;
#del \$(MAKE.dir)/\$(DST.dir)/$parcel/$file_as_exe: \\
#del     \$(MAKE.dir)/\$(DST.dir)/$parcel/$module/$file_as_obj \\
#del     \$(MAKE.dir)/\$(DST.dir)/$parcel/$parcel_as_lib
#del \t\@\$(MAKE_PERL_ECHO) "make: building: \$\@"
#del \t\$(LINK) \$(LINK_OUT)\$\@ \$^ \$(TEST_LIBS) \$(COVERAGE_LIBS) \$(LINK_FLAGS)
#del
#del EOL
                next;
            }
            printf STDERR qq[    - %s\n], $file_as_obj if ( $debug );
            $d_lib .= <<EOL;
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$module/$file_as_obj \\
EOL
            $d_lib_phony .= <<EOL;
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$module/$file_as_obj \\
EOL
            $lib_objs .= <<EOL;
        \$(call OSPATH,\$(MAKE.dir)/\$(DST.dir)/$parcel/$module/$file_as_obj) \\
EOL
            $lib_objs_parcel .= <<EOL;
        \$(call OSPATH,\$(MAKE.dir)/\$(DST.dir)/$parcel/$module/$file_as_obj) \\
EOL
        } # foreach my $file_as_obj
        #fixme or create .exe if exe
        $lib_objs  =~ s~\s*\\\s*$~~s;
        $d_lib     =~ s~\s*\\\s*$~\n~s;
        $d_lib    .=  <<EOL;
\t\@\$(MAKE_PERL_ECHO_BOLD) "make: building: \$\@"
\t\$(LIB_CMD) \$(LIB_FLAGS) \$(LIB_OUT)\$\@ \\
$lib_objs

EOL
        $d .= $d_lib;
        $d_lib_phony  =~ s~\s*\\\s*$~\n~s;
        $d_lib_phony .=  <<EOL;
\t\@\$(MAKE_PERL_ECHO_BOLD) "make: building: up-to-date: \$\@"

EOL
        $d .= $d_lib_phony;
        $lib_objs_test = qq[\n] . $lib_objs_test if ( defined $lib_objs_test );
        $lib_objs_test =~ s~\s+$~~               if ( defined $lib_objs_test );
        my $test_exes;
        my $test_runs;
        foreach my $file_as_obj ( sort keys %{ $link->{$module} } ) {
            if ( $file_as_obj =~ m~^test/~ ) {
                next if ( $file_as_obj !~ m~^test/test\-~ );
                my $file_as_exe =  $file_as_obj;
                   $file_as_exe =~ s~$ext_obj$~$ext_exe~; # e.g. test/test-sxe-pool-walker
                my $file_as_run =  $file_as_exe;
                   $file_as_run =~ s~^.*/~~;              # e.g.      test-sxe-pool-walker
                   $file_as_run =  'run-' . $file_as_run; # e.g.  run-test-sxe-pool-walker
                $test_exes->{"\$(MAKE.dir)/\$(DST.dir)/$parcel/$file_as_exe"} ++;
                $test_runs->{$file_as_run} ++;
                my $lib_objs_test_tmp = <<EOL;
$lib_objs_test
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$module/$file_as_obj \\
$obj_deps_as_mak->{$file_as_obj} \\
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$parcel_as_lib
EOL
                   $lib_objs_test_tmp =~ s~^[\r\n]+~~;
                   $lib_objs_test_tmp =~ s~\s*$~~;
                printf STDERR qq[    ~ %s\n], $file_as_exe if ( $debug );
                #    $(TOP.dir)/$(DST.dir)/libsxe/lib-sxe/test/common.o \
                #        $(TOP.dir)/libsxe/lib-sxe/test/common.c \
                #        ...
                #    $(TOP.dir)/$(DST.dir)/libsxe/lib-sxe/test/test-sxe-udp.o \
                #        $(TOP.dir)/libsxe/lib-sxe/test/test-sxe-udp.c \
                #        $(TOP.dir)/$(DST.dir)/libsxe/lib-sxe-util-proto.h \
                #        ...
                #    $(TOP.dir)/$(DST.dir)/libsxe/libsxe.a
                my $lib_objs_test_without_deps =  $lib_objs_test_tmp;
                   #del $lib_objs_test_without_deps =~ s~[ ]+[^\r\n]+\.[ch][ ]*\\[^\r\n]*~~gs; # remove .c & .h files
                   #del $lib_objs_test_without_deps =~ s~[ ]+[^\r\n]+GNUmakefile[ ]*\\[^\r\n]*~~gs; # remove GNUmakefile files
                   $lib_objs_test_without_deps =~ s~        [^\r\n]+[^\r\n]*~~gs; # remove indented deps
                   $lib_objs_test_without_deps =~ s~ \\[\r\n]+~ \\\n~gs; # remove blank lines
                   $lib_objs_test_without_deps =~ s~    ~        ~gs; # cosmetic indent
                my $output_folder = sprintf qq[%s/\$(DST.dir)], $make_dir =~ m~^[/\\]~ ? qq[\$(MAKE.dir)] : qq[../../\$(TOP.dir)];
                # Note: cl.exe wants $lib_objs_test_without_deps before $(LINK_FLAGS) !
                $d .= <<EOL;
\$(MAKE.dir)/\$(DST.dir)/$parcel/$file_as_exe: \\
$lib_objs_test_tmp
\t\@\$(MAKE_PERL_ECHO) "make: building: test      : \$\@"
\t\$(LINK) \$(LINK_OUT)\$(call OSPATH,\$\@) \$(TEST_LIBS) \$(COVERAGE_LIBS) \$(call OSPATH, \\
$lib_objs_test_without_deps) \$(LINK_FLAGS)
\t\@\$(DEL) \$(call OSPATH,$output_folder/$parcel/$file_as_exe.test-ran.ok) \$(TO_NUL) \$(FAKE_PASS)

\$(MAKE.dir)/\$(DST.dir)/$parcel/$file_as_exe.test-ran.ok: \\
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$file_as_exe \\
$lib_objs_test_tmp
\t\@\$(MAKE_PERL_ECHO) "make: building: running   : \$\@"
\tcd \$(call OSPATH,\$(TOP.dir)/$parcel/$module/test) && \$(call OSPATH,$output_folder/$parcel/$file_as_exe)
\t\@\$(TOUCH) \$(call OSPATH,\$(MAKE.dir)/\$(DST.dir)/$parcel/$file_as_exe.test-ran.ok)

.PHONY: $file_as_run
$file_as_run: \\
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$file_as_exe.test-ran.ok \\
    \$(MAKE.dir)/\$(DST.dir)/$parcel/$file_as_exe \\
$lib_objs_test_tmp
\t\@\$(MAKE_PERL_ECHO) "make: building: up-to-date: \$(MAKE.dir)/\$(DST.dir)/$parcel/$file_as_exe.test-ran.ok"

EOL
                next;
            }
        } # foreach my $file_as_obj
        if ( defined $test_exes ) {
            my $test_exes_as_mak;
            foreach my $test_exe ( sort keys %{ $test_exes } ) {
                $test_exes_as_mak .= <<EOL;
    $test_exe \\
EOL
            } # foreach my $test_exe
            $test_exes_as_mak =~ s~ \\\s+$~~;
            my $test_runs_as_mak;
            foreach my $test_run ( sort keys %{ $test_runs } ) {
                $test_runs_as_mak .= <<EOL;
    $test_run \\
EOL
            } # foreach my $test_run
            $test_runs_as_mak =~ s~ \\\s+$~~;
            my $test_module =     'tests-for-' . $module; # e.g.     tests-for-lib-sxe-pool-tcp
            my $run_module  = 'run-tests-for-' . $module; # e.g. run-tests-for-lib-sxe-pool-tcp
            $phony_parcel_tests->{$test_module} ++;
            $phony_parcel_runs->{$run_module} ++;
            my $test_runs_coverage = <<EOL;

ifneq (\$(filter $module,\$(COVERAGE_OPTOUT_LIST)),)
\t\@\$(MAKE_PERL_ECHO) "make: building: coverage  : ignored for module $module"
else
\t\@\$(MAKE_PERL_ECHO) "make: building: coverage  : reap"
\t\@cp -p -f \$(TOP.dir)/$parcel/$module/*.c \$(MAKE.dir)/\$(DST.dir)/$parcel/$module/. ; cd \$(MAKE.dir)/\$(DST.dir)/$parcel ; gcov ../$parcel/$module/*.c --object-directory ../$parcel/$module > /dev/null 2>&1 && cp -p -f *.c.gcov $module/. ; rm -f *.c.gcov
\t\@\$(MAKE_PERL_ECHO) "make: building: coverage  : check"
\t\@\$(MAKE_PERL_COVERAGE_CHECK) \$(MAKE.dir)/\$(DST.dir)/$parcel/$module/*.c.gcov
\t\@\$(MAKE_PERL_ECHO) "make: building: coverage  : acceptable for module $module"
endif
EOL
            undef $test_runs_coverage if ( not defined $coverage );
            $test_runs_coverage =~ s~\s+$~~s;
            $d .= <<EOL;
.PHONY: $test_module
$test_module: \\
$test_exes_as_mak
\t\@\$(MAKE_PERL_ECHO_BOLD) "make: building: up-to-date: \$\@"

.PHONY: $run_module
$run_module: \\
$test_runs_as_mak$test_runs_coverage
\t\@\$(MAKE_PERL_ECHO_BOLD) "make: building: up-to-date: \$\@"

EOL
        } # if ( defined $test_exes )
    } # foreach my $module
    my $phony_parcel_test_as_mak;
    foreach my $phony_parcel_test ( sort keys %{ $phony_parcel_tests } ) {
        $phony_parcel_test_as_mak .= <<EOL;
    $phony_parcel_test \\
EOL
    } # foreach my $phony_parcel_test
    my $test_parcel = 'tests-for-' . $parcel; # e.g. tests-for-libsxe
    $phony_parcel_test_as_mak =~ s~ \\\s+$~~;

    my $phony_parcel_runs_as_mak;
    foreach my $phony_parcel_run ( sort keys %{ $phony_parcel_runs } ) {
        $phony_parcel_runs_as_mak .= <<EOL;
    $phony_parcel_run \\
EOL
    } # foreach my $phony_parcel_run
    my $run_parcel = 'run-tests-for-' . $parcel; # e.g. run-tests-for-libsxe
    $phony_parcel_runs_as_mak =~ s~ \\\s+$~~;

    $d .= <<EOL;
.PHONY: $test_parcel
$test_parcel: \\
$phony_parcel_test_as_mak
\t\@\$(MAKE_PERL_ECHO_BOLD) "make: building: up-to-date: \$\@"

.PHONY: $run_parcel
$run_parcel: \\
$phony_parcel_runs_as_mak
\t\@\$(MAKE_PERL_ECHO_BOLD) "make: building: up-to-date: \$\@"

EOL
#old     foreach my $module ( sort keys %{ $m3rd->{$parcel} } ) {
#old         my $naked_module =  uc ( $module );
#old            $naked_module =~ s~^lib\-~~i;
#old         $lib_objs_parcel .= <<EOL;
#old         \$(call OSPATH,\$(THIRDPARTY_LIB_${naked_module}_OBJECTS)) \\
#old EOL
#old     }
    my $d_lib_parcel_thirdparty_objects = $d_lib_parcel_thirdparty;
       $d_lib_parcel_thirdparty_objects =~ s~    ~        ~gs;
       $d_lib_parcel_thirdparty_objects =~ s~\s*\\\s*$~~gs; # remove trailing " \<CR>"
       $lib_objs_parcel .= <<EOL;
        \$(call OSPATH, \\
$d_lib_parcel_thirdparty_objects)
EOL
    $lib_objs_parcel  =~ s~\s*\\\s*$~~s;
    $d_lib_parcel    .=  $d_lib_parcel_thirdparty;
    $d_lib_parcel     =~ s~\s*\\\s*$~\n~s;
    $d_lib_parcel    .=  <<EOL;
\t\@\$(MAKE_PERL_ECHO) "make: building: \$\@"
\t\$(LIB_CMD) \$(LIB_FLAGS) \$(LIB_OUT)\$\@ \\
$lib_objs_parcel

EOL
    $d .= $d_lib_parcel;
    $d_lib_parcel_phony .=  $d_lib_parcel_thirdparty;
    $d_lib_parcel_phony  =~ s~\s*\\\s*$~\n~s;
    $d_lib_parcel_phony .=  <<EOL;
\t\@\$(MAKE_PERL_ECHO_BOLD) "make: building: up-to-date: \$\@"

EOL
#del \t\$(LIB_CMD) \$(LIB_FLAGS) \$(LIB_OUT)\$\@ \\
#del $lib_objs_parcel
    $d .= $d_lib_parcel_phony;

    printf STDERR qq[- writing rules for recreating *-proto.h files\n] if ( $debug );
    foreach my $file ( sort keys %{ $deps_unknown } ) {
        next if ( $file eq 'config.h' ); # fixme: special case for libev/config.h
        my $h_class;
        if ( $file =~ m~^(lib\-.+?)\-proto.h~ ) {
            # come here if building *-proto.h file for a whole module
            $h_class = 'proto lib';
            my ( $module ) = ( $1 );
            die qq[ERROR: discovered reference to lib-*-proto.h ($file) but associated module ($module) does not exist] if ( not exists $mods->{$module} );
            my $output_folder = sprintf qq[%s/\$(DST.dir)/$parcel], $make_dir =~ m~^[/\\]~ ? qq[\$(MAKE.dir)] : '../..';
            $d .= <<EOL;
\$(MAKE.dir)/\$(DST.dir)/$parcel/$file: \$(wildcard \$(TOP.dir)/$parcel/$module/*.h) \$(wildcard \$(TOP.dir)/$parcel/$module/*.c)
\t\@\$(MAKE_PERL_ECHO) "make: building: \$\@"
\tcd \$(TOP.dir)/$parcel/$module && \$(PERL) ../../mak/bin/genxface.pl -d -o $output_folder

EOL
        } elsif ( $file =~ m~^(.+?)\-proto.h~ ) {
            # come here if building *-proto.h file for a single c file
            $h_class = 'proto c  ';
            my $c_file = $1 . '.c';
            die qq[ERROR: discovered reference to *-proto.h ($file) but associated c file ($c_file) does not exist] if ( not exists $deps->{$c_file} );
            my $module = $deps->{$c_file}{Module};
            my $output_folder = sprintf qq[%s/\$(DST.dir)/$parcel], $make_dir =~ m~^[/\\]~ ? qq[\$(MAKE.dir)] : '../..';
            $d .= <<EOL;
\$(MAKE.dir)/\$(DST.dir)/$parcel/$file: \$(TOP.dir)/$parcel/$module/$c_file
\t\@\$(MAKE_PERL_ECHO) "make: building: \$\@"
\tcd \$(TOP.dir)/$parcel/$module && \$(PERL) ../../mak/bin/genxface.pl -o $output_folder ../../$parcel/$module/$c_file

EOL
        } else {
            # come here if unknown header file
            $h_class = 'unknown  ';
        }
        printf STDERR qq[  - %s %s\n], $h_class, $file if ( $debug );
    }

    foreach my $parcel ( sort keys %{ $m3rd } ) {
        foreach my $module ( sort keys %{ $m3rd->{$parcel} } ) {
            $d .= <<EOL;
ifdef MAKE_DEBUG
\$(info make: debug: include \$(TOP.dir)/$parcel/$module/thirdparty.mak)
endif
include \$(TOP.dir)/$parcel/$module/thirdparty.mak

EOL
        }
    }

    printf STDERR qq[- creating build folders\n] if ( $debug );
    foreach my $dir ( sort keys %{ $path } ) {
        printf STDERR qq[  - creating build folder: %s\n], $dir if ( $debug );
        mkpath ( $dir );
    } # foreach my $dir

    printf STDERR qq[- creating phony target usage\n] if ( $debug );
    $d .= <<EOL;
help::
EOL
    foreach my $phony_target ( sort keys %{ $phony } ) {
        printf STDERR qq[  - creating usage for phony target: %s\n], $phony_target if ( $debug );
        $d .= <<EOL;
\t\@echo \$(ECHOQUOTE)make: usage: target: [[run-]tests-for-]$phony_target\$(ECHOQUOTE)
EOL
    } # foreach my $phony_target
    $d .= <<EOL;
\t\@echo \$(ECHOQUOTE)make: todo: implement building sibling parcels, e.g. libsxl3\$(ECHOQUOTE)
\t\@echo \$(ECHOQUOTE)make: todo: implement all/parcel/module/file/function instrumentation\$(ECHOQUOTE)
\t\@echo \$(ECHOQUOTE)make: todo: add clean- phony targets?\$(ECHOQUOTE)
\t\@echo \$(ECHOQUOTE)make: todo: fix test-*.exes to work with -j 2\$(ECHOQUOTE)
\t\@echo \$(ECHOQUOTE)make: todo: check for test-*.pl\$(ECHOQUOTE)
\t\@echo \$(ECHOQUOTE)make: todo: add local GNUmakefiles defaulting to local targets, e.g. run-tests-for-libsxe or run-tests-for-lib-sxe etc\$(ECHOQUOTE)
\t\@echo \$(ECHOQUOTE)make: todo: create mak test suite and refactor ugly Perl code\$(ECHOQUOTE)
\t\@echo \$(ECHOQUOTE)make: todo: remove building of lib-*.a files\$(ECHOQUOTE)
\t\@echo \$(ECHOQUOTE)make: todo: build test/test-*.o files in $parcel/test/ folder for better simonization\$(ECHOQUOTE)

EOL
    #fixme: todo: \t\@echo usage: todo: put in make usage: time make -f GNUmakefile.mak debug run-tests-for-libsxe 2>&1 | tee /tmp/debug.txt | egrep "(skew|building)"

    printf STDERR qq[- creating clean targets\n] if ( $debug );
    $d .= <<EOL;
.PHONY: clean
clean::
	\@\$(MAKE_PERL_ECHO) "make: cleaning (all but thirdparty)"
	\$(DEL) \$(call OSPATH,\$(MAKE.dir)/\$(DST.dir)/$parcel/*$ext_lib) \$(TO_NUL) \$(FAKE_PASS)
	\$(DEL) \$(call OSPATH,\$(MAKE.dir)/\$(DST.dir)/$parcel/*-proto.h) \$(TO_NUL) \$(FAKE_PASS)
EOL
    foreach my $clean_line ( sort keys %{ $clean_lines } ) {
        $d .= sprintf qq[\t%s\n], $clean_line;
    }
    $d .= qq[\n];

    my $d_file = qq[$make_dir/$dst_dir/dependencies.d];
    printf STDERR qq[%f - writing: %s\n], Time::HiRes::time, $d_file if ( $debug );
    open ( D, '>', $d_file ) || die qq[ERROR: cannot open dependency file for writing: $d_file];
    syswrite ( D, $d, length $d );
    close ( D );

} # foreach my $absolute_parcel
printf STDERR qq[%f - input:%7d bytes;%5d lines (%d for test) in total found in '%s' (excluding thirdparty modules)\n], Time::HiRes::time, $total_bytes_source, $total_lines_source, $total_lines_source_test, $Bin;

#todo log level foreach my $env_log_level_key ( sort keys %{ $env_log_levels } ) {
#todo log level 	printf qq[- todo: env var enum %3d, env var name %s\n], $env_log_levels->{$env_log_level_key}, $env_log_level_key;
#todo log level }
#todo log level printf qq[- todo: create .h files with these enumerations, e.g. log macros in sxe_list.c would compare their verbosity level with env var enums 0, 1, 29, & 30\n];

printf qq[created-dependencies];
