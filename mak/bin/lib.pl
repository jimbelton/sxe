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
use File::Path;

if ($^O eq "MSWin32") {
    exit(system("lib ".join(' ', @ARGV)));
}

while ((scalar(@ARGV) > 1) && ($ARGV[scalar(@ARGV) - 1] =~ /.\.a$/)) {
    my $lib = pop(@ARGV);
    print "lib: library '$lib'\n";

    -d "tmp-lib-explosion" or mkdir("tmp-lib-explosion") or die("lib.pl: Can't make directory tmp-lib-explosion\n");
    chdir("tmp-lib-explosion")                           or die("lib.pl: Can't change to directory tmp-lib-explosion\n");
    system("ar x ../$lib") == 0                          or die("lib.pl: Can't explode library $lib\n");
    chdir("..")                                          or die("lib.pl: Can't change back to directory ..\n");
}

my $lib_cmd = "ar csr ".join(' ', @ARGV)." ".join(' ',glob("tmp-lib-explosion/*.o"));
system($lib_cmd) == 0       or die("lib.pl: Archive command failed: ".$lib_cmd);

if (-d "tmp-lib-explosion") {
    rmtree("tmp-lib-explosion") or die("lib.pl: Can't recursively remove directory tmp-lib-explosion\n");
}
