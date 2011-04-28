#!/usr/bin/perl

use strict;
use warnings;
use Test::More tests => 28;

require "genxface.pl";

my $line = "/*\n * This is a comment\n */\nnot = in(comment);\n";
is(getComment(\$line, undef), "/*\n * This is a comment\n */",  "comment is as expected");
$line = "//\n// This is a comment\n//\nnot = in(comment);\n";
is(getComment(\$line, undef), "//\n// This is a comment\n//\n", "C++ style comment is as expected");
$line = "'\\\\'"; # Double backslash between quotes (i.e. the quote character)
is(getString(\$line, undef), "'\\\\'",                          "backslash as a character constant");
$line = '"string ending in \\\\"'; # String ending in a backlash
is(getString(\$line, undef), '"string ending in \\\\"',           "backslash at the end of a string");

my $proto = "void abort(void)";
is(protoGetReturn($proto, "abort"),            "void", "abort returns void");
is(scalar(@{protoGetParams($proto, "abort")}), 0,      "abort has 0 parameters");

$proto = "char * itoa(int num)";
is(protoGetReturn($proto, "itoa"), "char *", "itoa returns char *");
my $params = protoGetParams($proto, "itoa");
is(scalar(@{$params}), 1,     "itoa has 1 parameter");
is($params->[0]->[0],  "int", "parameter type is int");
is($params->[0]->[1],  "num", "parameter name is num");

$proto = "int fputs(const char *S, FILE *FP)";
$params = protoGetParams($proto, "fputs");
is(scalar(@{$params}), 2,              "fputs has 2 parameters");
is($params->[0]->[0],  "const char *", "1st parameter type is const char *");
is($params->[0]->[1],  "S",            "1st parameter name is S");
is($params->[1]->[0],  "FILE *",       "2nd parameter type is FILE *");
is($params->[1]->[1],  "FP",           "2nd parameter name is FP");

$proto = "void ev_set_syserr_cb (void (*cb)(const char *msg))";
$params = protoGetParams($proto, "ev_set_syserr_cb");
is(scalar(@{$params}), 1,                           "ev_set_syserr_cb has 1 parameter");
is($params->[0]->[0],  "void (*)(const char *msg)", "parameter type is void (*)(const char *msg)");
is($params->[0]->[1],  "cb",                        "parameter name is cb");

$proto = "RETSIGTYPE (*setsignal (int sig, RETSIGTYPE (*func)(int)))(int)";
is(protoGetReturn($proto, "setsignal"), "RETSIGTYPE (*)(int)", "setsignal returns RETSIGTYPE (*)(int)");
$params = protoGetParams($proto, "setsignal");
is(scalar(@{$params}), 2,                     "setsignal has 2 parameters");
is($params->[0]->[0],  "int",                 "1st parameter type is int");
is($params->[0]->[1],  "sig",                 "1st parameter name is sig");
is($params->[1]->[0],  "RETSIGTYPE (*)(int)", "2nd parameter type is RETSIGTYPE (*)(int)");
is($params->[1]->[1],  "func",                "2nd parameter name is func");

# Make sure scope modifiers don't confuse void return detection
#
$proto = "void noinline ev_feed_event (EV_P_ void *w, int revents)";
is(protoGetReturn($proto, "ev_feed_event"), "void", "ev_feedevent returns void");

$proto = "List List_vnew(unsigned number, List_type elem, ...)";
$params = protoGetParams($proto, "List_vnew");
is(scalar(@{$params}), 3,     "List_vnew has 3 parameters");
is($params->[2]->[0],  "...", "3rd parameter type is ...");

my $comment = "/* A brief description\n * \nLonger description, blah, blah.\n * \@param parameter description of parameter\n".
              "* \@param par2 = Another parameter \n \@return 10 \@ a time\n \@note This is a note\n */";
my $docComment = docCommentParse($comment, "function");
is($docComment->{brief}, "A brief description", "Doc comment's brief description is 'A brief description'");

exit(0);
