#!/usr/bin/perl

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
use warnings;
use Cwd;
use English;
use File::Basename;

my $os_src_dir = ".";       # OS specific source file directory
my $output_dir = ".";
my @includes   = (".");
my $output_proto_rules = {};

sub usage
{
    my ($error) = @_;

    if ($error) {
        print STDERR ("mkdeps.pl: ".$error."\n");
    }

    print STDERR (<<"EOS");
usage: mkdeps.pl [-d <os_src_dir>] [-o <output-dir>] [[-/]I<include-dir> ...] file
EOS
   exit ($error ? 1 : 0);
}

sub opt_arg
{
    my ($option, $argv_ref, $val_ref) = @_;

    if (substr($$argv_ref[0], 0, 1) eq "/") {
        substr($$argv_ref[0], 0, 1) = "-";
    }

    if (substr($$argv_ref[0], 0, length($option)) ne $option) {
        return undef;
    }

    if (length($$argv_ref[0]) == length($option)) {
        shift(@{$argv_ref});
    }
    else {
        $$argv_ref[0] = substr($$argv_ref[0], length($option));
    }

    $$val_ref = $$argv_ref[0] or usage("$option option requires an argument");
    return $$val_ref;
}

my @file_stack = [];

sub source_scan
{
    my ($source_dir, $source_file) = @_;
    my $source_path = $source_dir."/".$source_file;
    my $output      = "";
    my $in;

    if (scalar(grep($_ eq $source_path, @file_stack)) > 0) {
        warn("mkdeps.pl: warning: Recursion detected in '$source_path'\n");
        return "";
    }

    if (!open($in, $source_path)) {
        warn("mkdeps.pl: warning: Can't open file '$source_path'\n");
        return "";
    }

    while (my $line = <$in>)
    {
        if ($line =~ /^\s*#\s*include\s*"([^"]+)/) {
            chomp $line;
            my $file = $1;

            for my $dir (@includes) {
                if (-e $dir."/".$file) {
                    printf qq[$0: DEBUG: include file     found: %s\n], $line if ( exists $ENV{MAKE_DEBUG} );
                    push(@file_stack, $source_path);
                    $output .= $dir."/".$file." ".source_scan($dir, $file);
                    $file    = "";
                    pop(@file_stack);
                    last;
                }
            }

            # Include file not found in the include path.
            #
            if ($file) {
                # Prototype file: Add to the dependencies, and add a rule to generate it
                # in the output subdirectory of the directory containing the file that included it.
                #
                if ($file =~ /^(.+)-proto.h$/) {
                    printf qq[$0: DEBUG: proto   file not found: %s\n], $line if ( exists $ENV{MAKE_DEBUG} );
                    my $src  = $1;
                    my $leaf = ($source_dir eq ".") ? basename(cwd()) : basename($source_dir);
                    $output .= $source_dir."/".$output_dir."/".$file." ";

                    if (-f "$source_dir/$src.c") {
                        my $tmp_target = "$source_dir/$output_dir/%-proto.h";
                        my $tmp_mak_macro = uc ( $tmp_target );
                           $tmp_mak_macro =~ s~\.~_DOT_~gs;
                           $tmp_mak_macro =~ s~[^a-z0-9]~_~gis;
                           $tmp_mak_macro =~ s~[^a-z0-9]~_~gis;
                        $output_proto_rules->{$file} =
                             "ifndef $tmp_mak_macro\n"
                            ."$tmp_mak_macro := 1\n"
                            ."$tmp_target: $source_dir/%.c\n"
                            ."\t\@\$(MAKE_PERL_ECHO) \"make[\$(MAKELEVEL)]: building: \$\@\"\n"
                            ."\t\$(MAKE_RUN) cd $source_dir && \$(PERL) \$(TOP.dir)/mak/bin/genxface.pl -o $output_dir \$(notdir \$*.c)\n"
                            ."endif\n\n";
                    }
                    elsif ($src eq $leaf) {
                        my $tmp_target = "$source_dir/$output_dir/$leaf-proto.h";
                        my $tmp_mak_macro = uc ( $tmp_target );
                           $tmp_mak_macro =~ s~\.~_DOT_~gs;
                           $tmp_mak_macro =~ s~[^a-z0-9]~_~gis;
                           $tmp_mak_macro =~ s~[^a-z0-9]~_~gis;
                        $output_proto_rules->{$file} =
                             "ifndef $tmp_mak_macro\n"
                            ."$tmp_mak_macro := 1\n"
                            ."$tmp_target: \$(wildcard $source_dir/*.h) \$(wildcard $source_dir/*.c)\n"
                            ."\t\@\$(MAKE_PERL_ECHO) \"make[\$(MAKELEVEL)]: building: \$\@\"\n"
                            ."\t\$(MAKE_RUN) cd $source_dir && \$(PERL) \$(TOP.dir)/mak/bin/genxface.pl -d -o $output_dir\n"
                            ."endif\n\n";
                    }
                    else {
                        warn("mkdeps.pl: warning: Can't generate prototype file '$file' in directory '$source_dir'");
                    }
                }

                # Not a generated file.  This is hokey, because this dependency just gets lost.
                #
                else {
                    printf qq[$0: DEBUG: include file not found:*%s\n], $line if ( exists $ENV{MAKE_DEBUG} );
                    warn("mkdeps.pl: warning: Can't find included file '$file'");
                }
            } # if ($file)
        } # if ($line =~ /^\s*#\s*include\s*"([^"]+)/)
    } # while (my $line = <$in>)

    return $output;
}

# Custom option processing.

while ($ARGV[0] && substr($ARGV[0], 0, 1) =~ /[-\/]/) {
    my $value;

    # Check for an OS specific subdirectory name
    if (opt_arg("-d", \@ARGV, \$os_src_dir)) {
    }
    elsif (opt_arg("-o", \@ARGV, \$output_dir)) {
        -d $output_dir or mkdir($output_dir) or die("mkdeps.pl: Error creating directory $output_dir: $!\n");
    }
    elsif (opt_arg("-I", \@ARGV, \$value)) {
        if (-d $value) {
            push(@includes, $value);
        }
        elsif ($value =~ m~dll\-.*/build\-~) {
            # don't worry if missing include folder is a dll
        }
        elsif ($value ne $os_src_dir) {
            # todo: figure out a way to make make stop on the following error (instead of flowery highlighting workaround)
            die <<EOL

vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
mkdeps.pl: Error: include directory $value: $!
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

EOL
        }
    }

    # Other options are silently ignored (but not bare arguments!)
    #
    shift(@ARGV);
}

$ARGV[0] or usage("error: no file specified");
my $source_file_stripped = $ARGV[0];
$source_file_stripped =~ s~\.c~~;

if (!-e $ARGV[0]) {
    if (-e $os_src_dir."/".$ARGV[0]) {
        $ARGV[0] = $os_src_dir."/".$ARGV[0];
    }
    else {
        usage("error: '".$ARGV[0]."' must be a file");
    }
}

# Generate dependency file; outputs: e.g.:
# build-debug/sxe.o build-debug/sxe.d: ../lib-ev/ev.h ../lib-mock/mock.h ./sxe.h ../lib-ev/ev.h ./build-debug/sxe-proto.h ./sxe-log.h ./build-debug/sxe-log-proto.h sxe.c GNUmakefile

my $ext_obj = ($OSNAME eq "MSWin32" ? "obj" : "o");
my $output  = source_scan(".", $ARGV[0]);
my $out_file;

open($out_file, ">$output_dir/$source_file_stripped.d")
    or die("mkdep.pl: Error: Unable to create file $output_dir/$source_file_stripped.d");
#print $out_file $output_proto_rules->{$_} foreach sort keys %$output_proto_rules;
foreach my $file ( sort keys %{ $output_proto_rules } ) {
    my $this_output_proto_rule = $output_proto_rules->{$file};
       $this_output_proto_rule =~ s~replace-with-this-d-file~$output_dir/$source_file_stripped.d~;
    print $out_file $this_output_proto_rule;
}
print $out_file ("$output_dir/$source_file_stripped.$ext_obj: \\\n");
{
    my $h;
    foreach my $dep ( split ( m~\s+~, $output ) ) {
        $h->{$dep} ++;
    }
    foreach my $dep ( sort keys %{ $h } ) {
        print $out_file ("    $dep \\\n");
    }
}
print $out_file ("    $ARGV[0] \\\n");
print $out_file ("    GNUmakefile\n");
print $out_file ("\n");

print $out_file ("$output_dir/$source_file_stripped.d:: \\\n");
{
    my $h;
    foreach my $dep ( split ( m~\s+~, $output ) ) {
        $h->{$dep} ++;
    }
    foreach my $dep ( sort keys %{ $h } ) {
        print $out_file ("    $dep \\\n");
    }
}
print $out_file ("    $ARGV[0] \\\n");
print $out_file ("    GNUmakefile\n");
print $out_file ("\t\$(MAKE_RUN) \$(TOUCH) $output_dir/$source_file_stripped.d\n");
close($out_file);
exit(0);
