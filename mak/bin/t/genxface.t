#!/usr/bin/perl

# genxface.t: Test genxface.pl, which generates interface (.h) files from C files
# Copyright (C) 2010 Jim Belton
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

use strict;

use Cwd;
use File::Path;
use FindBin qw($Bin);
use Test::More tests => 8;

my $slash = ($^O eq "MSWin32") ? "\\" : "/";
chdir($Bin);

unlink("libev/libev_proto.h");
unlink("libev/libev-proto.h");
is(system("..${slash}genxface.pl -d libev"),                                                 0, "Ran genxface.pl -d on libev");
is(system("diff expected-libev-proto.h libev/libev-proto.h"),                                0, "libev-proto.h as expected");

unlink("libperlike/list-proto.h");
is(system("..${slash}genxface.pl libperlike"),                                               0, "Ran genxface.pl on libperlike");
is(system("diff expected-list-proto.h libperlike/list-proto.h"),                             0, "list-proto.h as expected");

chdir("lib-testcode");
rmtree("build-linux-release");
is(system("$Bin$slash..${slash}genxface.pl -dm -o build-linux-release"),                     0, "Ran genxface.pl -dm -o on lib-testcode");
is(system("diff ../expected-lib-testcode-proto.h build-linux-release/lib-testcode-proto.h"), 0, "lib-testcode-proto.h as expected");
is(system("diff ../expected-testcode_mock__.c testcode_mock__.c"),                           0, "testcode_mock__.c as expected");

chdir("../lib-sxe") or die("Couldn't chdir to ../lib-sxe from ".cwd()."\n");
rmtree("build-linux-release");
is(system("$Bin$slash..${slash}genxface.pl -dw -o build-linux-release"),                     0, "Ran genxface.pl -dw -o on lib-sxe");
#is(system("diff ../expected-lib-sxe-public.h build-linux-release/lib-sxe-proto.h"),          0, "lib-sxe-public.h as expected");
