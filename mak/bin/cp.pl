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
use File::Basename;
use File::Copy;
use Getopt::Std;

sub usage
{
    if ($_[0]) {
        print STDERR ($@);
    }

    die("usage: cp.pl [-f] source ... destination");
}

my %opts;
getopts('f', \%opts)         or usage();
my $destination = pop(@ARGV) or usage("cp.pl: No destination specified\n");

for my $source (@ARGV) {
    my $file_name = basename($source);

    if (copy($source, $destination."/".$file_name)) {
        next;
    }

    if ($opts{f}) {
        unlink($destination."/".$file_name) or die("cp.pl: Unable to remove existing $destination/$file_name file: $!");

        if (copy($source, $destination."/".$file_name)) {
            next;
        }
    }

    die("cp.pl: Can't copy file $source to directory $destination: $!");
}

exit(0);

