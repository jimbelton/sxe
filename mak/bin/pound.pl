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
use English;

my $test_pattern = "build-linux-32-debug/*.t";

if ($OSNAME eq "MSWin32") {
    $test_pattern = "build-winnt-64-debug\\*.t";
}

rename("pound.log", "pound.log.bak");

if (system("make debug test >> pound.log 2>&1") != 0) {
    die("It's a one-hit wonder!\n");
}

my @tests = glob($test_pattern);

if (scalar(@tests) == 0) {
    die("You don't have any test programs matching '$test_pattern'\n");
}

while (1) {
    for my $file (@tests) {
        if (system("$file >> pound.log 2>&1") != 0) {
            die("Boom!\n");
        }
    }
}
