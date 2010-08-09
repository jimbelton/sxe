#!/usr/bin/perl

use strict;
use warnings;
use Cwd;
use File::Path;
use FindBin qw($Bin);
use Test::More ('tests' => 7);

chdir("$Bin/lib-testcode") or die("mkdeps.t: error: Can't change to directory '$Bin/lib-testcode'\n");
rmtree("debug");

is(system("../../mkdeps.pl -o debug -I../libev testcode.c"), 0, "Ran ../../mkdeps.pl -o debug -I../libev testcode.c");
ok(-f "debug/testcode.d",                                       "Created debug/testcode.d");
my @output = `cat debug/testcode.d`;
is(scalar(@output),                                          5, "testcode.d contains 5 lines");
is($output[0], "./debug/lib-testcode-proto.h: \$(wildcard ./*.h) \$(wildcard ./*.c)\n",
                                                                "generated correct dependencies for lib-testcode-proto.h");
ok($output[2] =~ /genxface.pl (.*)$/,                           "generated a genxface.pl line for lib-testcode-proto.h");
is($1,         "-d -o debug",                                   "generated correct genxface.pl line for lib-testcode-proto.h");
is($output[4], "debug/testcode.o debug/testcode.d: ../libev/ev.h ./testcode.h ./debug/lib-testcode-proto.h testcode.c GNUmakefile\n",
                                                                "generated correct dependencies for testcode.c");
