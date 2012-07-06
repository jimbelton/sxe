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
use English;

my $build_type = exists $ENV{POUND_BUILD_TYPE} ? $ENV{POUND_BUILD_TYPE} : 'debug';
warn "Build type is '$build_type' (n.b. change using e.g. set POUND_BUILD_TYPE=release)\n";

my @tests = @ARGV;
if (scalar(@ARGV) == 0) {
    my $test_pattern = $OSNAME eq "MSWin32" ? "build-winnt-64-$build_type\\*.t" : "build-linux-32-$build_type/*.t";
    @tests = glob($test_pattern);
    scalar(@tests) > 0 or die("You don't have any test programs matching '$test_pattern'\n");
}

my $count = 0;
while (1) {
    for my $file (@tests) {
        my ($dst_dir,$dst_file) = $file =~ m~^(.+)[\\/]([^\\/]+)$~;
        my $command_test = "cd $dst_dir && ./$dst_file > pound-test.log 2>&1";
        if ($count == 0) {
            rename("$dst_dir/pound.log", "$dst_dir/pound.log.bak");
            $ENV{POUND_JFDI} or do {
                my $command_build = "make $build_type test > $dst_dir/pound-build.log 2>&1";
                warn "Running '$command_build' (n.b. set POUND_JFDI to skip this step!)\n";
                system($command_build) == 0 or die("It's a one-hit wonder!\n");
                warn "Finished; now pounding to pound.log...\n";
            };
            warn "Running '$command_test'\n";
        }
        if (system($command_test) != 0) {
            die("Boom!\n");
        }

        print STDERR (".");
        $count ++;
    }
}
