#!/usr/bin/perl

# genxface.pl: Generate interface (.h) files from C files
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
use warnings;
use Carp;
use Cwd;
use File::Basename;
use Getopt::Std;

my $numBlocks = 0;
my %opts      = ('w' => 1);

sub getLine
{
    my ($in) = @_;

    return defined($in) ? <$in> : undef;
}

# For all parsing functions.
#
# $lineRef = Reference to a line buffer or undef on EOB/EOF.
# $in      = Reference to a file handle to use to refill the line buffer
#            or undef if only the buffer is to be parsed.
#
# Returns: Exactly what was parsed, or "" if nothing matched.
#

sub getWhiteSpace
{
    my ($lineRef, $in) = @_;

    if (!defined($$lineRef)) {
        return "";
    }

    my $ret = "";

    while ($$lineRef =~ /^\s*$/s) {
        $ret .= $$lineRef;

        if (!defined($$lineRef = getLine($in))) {
            return $ret;
        }
    }

    if ($$lineRef =~ /^(\s+)(.*)$/s) {
        $ret     .= $1;
        $$lineRef = $2;
    }

    print("~W~".$ret) if ($opts{v} && ($ret ne ""));
    return $ret;
}

sub getComment
{
    my ($lineRef, $in) = @_;
    my $ret = "";

    if (!defined($$lineRef)) {
        return "";
    }

    if (substr($$lineRef, 0, 2) eq "//") {
        while ($$lineRef =~ /^([ \t]*\/\/[^\n]*\n?)(.*)$/s) {
            $ret     .= $1;
            $$lineRef = $2;
        }

        goto EARLY_OUT;
    }

    if (substr($$lineRef, 0, 2) ne "/*") {
        return "";
    }

    while ($$lineRef !~ /\*\//) {
        $ret .= $$lineRef;

        if (!defined($$lineRef = getLine($in))) {
            warn("EOF inside a comment\n");
            goto EARLY_OUT;
        }
    }

    $$lineRef =~ /^(.*?\*\/)(.*)$/s;
    $ret     .= $1;
    $$lineRef = $2;

EARLY_OUT:
    print("~C~".$ret) if ($opts{v});
    return $ret;
}

sub getWhiteSpaceAndComments
{
    my ($lineRef, $in) = @_;
    my $ret = "";
    my $next;

    while ((($next = getWhiteSpace($lineRef, $in)) != "")
           || (($next = getComment($lineRef, $in)) != ""))
    {
        $ret .= $next;
    }

    return $ret;
}

# If the current line ends in a \, get all the continuations
#
sub getContinuation
{
    my ($lineRef, $in) = @_;
    my $ret = "";

    # Parse continuation lines.
    #
    while ($$lineRef =~ /\\$/) {
        $ret .= $$lineRef;

        if (!defined($$lineRef = getLine($in))) {
            warn("EOF inside a multiline preprocessor statement\n");
            return $ret;
        }
    }

    $ret     .= $$lineRef;
    $$lineRef = getLine($in);
    return $ret;
}

sub getPreProc
{
    my ($lineRef, $in) = @_;

    if (!defined($$lineRef) || substr($$lineRef, 0, 1) ne "#") {
        return "";
    }

    my $ret = getContinuation($lineRef, $in);

    # If this is an #if 0 directive
    #
    if ($ret =~ /^#\s*if\s+0\b/) {
        my $level = 1;

        while ($level > 0) {
            if (!defined($$lineRef)) {
                warn("EOF after #if 0 but before matching #endif\n");
                return $ret;
            }

            if ($$lineRef =~ /^\s*#endif\s*/) {
                $level--;
            }
            elsif ($$lineRef =~ /^\s*#if/) {
                $level++;
            }

            $ret .= getContinuation($lineRef, $in);
        }
    }

    print("~#~".$ret) if ($opts{v});
    return $ret;
}

sub getWhiteSpaceCommentsAndPreProcs
{
    my ($lineRef, $in) = @_;
    my $ret = "";
    my $next;

    while ((($next = getWhiteSpace($lineRef, $in)) ne "")
           || (($next = getComment($lineRef, $in)) ne "")
           || (($next = getPreProc($lineRef, $in)) ne ""))
    {
        $ret .= $next;
    }

    return $ret;
}

# Get an identifier.
#
sub getIdentifier
{
    my ($lineRef) = @_;

    if (!defined($$lineRef)) {
        return "";
    }

    if ($$lineRef =~ /^([A-Za-z_\$][A-Za-z0-9_\$]*)(.*)$/s) {
        $$lineRef = $2;
        print("~I~".$1) if ($opts{v});
        return $1;
    }

    return "";
}

# Get a C preprocessor number.
#
sub getNumber
{
    my ($lineRef) = @_;

    if (!defined($$lineRef)) {
        return "";
    }

    if ($$lineRef =~ /^(\.?[0-9]([A-Za-z0-9_\.]|[eEpP][+-])*)(.*)$/s) {
        $$lineRef = $3;
        print("~N~".$1) if ($opts{v});
        return $1;
    }

    return "";
}

# Get a string literal.
#
sub getString
{
    my ($lineRef, $in) = @_;

    if (!defined($$lineRef)) {
        return "";
    }

    my $let = substr($$lineRef, 0, 1);

    if ($let eq '"') {
        # Until the buffer starts with a string between double quotes that is
        # optionally empty, or does not end with a backslash.
        #
        while ($$lineRef !~ /^.*?[^\\](\\\\)*"/)
        {
            my $nextLine = getLine($in);

            if (!defined($nextLine)) {
                warn("EOF in a string literal '".substr($$lineRef, 0, 8)
                     .(length($$lineRef) > 8 ? "..." : "")."'\n");
                my $ret = $$lineRef;
                $$lineRef = undef;
                return $ret;
            }

            $$lineRef .= $nextLine;
        }

        $$lineRef =~ /^(.*?[^\\](\\\\)*")(.*)$/s;
        $$lineRef = $3;
        print('~"~'.$1) if ($opts{v});
        return $1;
    }

    if ($let eq "'") {
        while ($$lineRef !~ /^.*?[^\\](\\\\)*'/)
        {
            my $nextLine = getLine($in);

            if (!defined($nextLine)) {
                warn("EOF in a character literal\n");
                my $ret = $$lineRef;
                $$lineRef = undef;
                return $ret;
            }

            $$lineRef .= $nextLine;
        }

        $$lineRef =~ /^(.*?[^\\](\\\\)*')(.*)$/s;
        $$lineRef = $3;
        print("~'~".$1) if ($opts{v});
        return $1;
    }

    return "";
}

sub getExternC
{
    my ($lineRef, $in) = @_;
    my $prev;

    if (!defined($$lineRef)) {
        return "";
    }

    if (($prev = getIdentifier($lineRef, $in)) ne "extern") {
        $$lineRef = $prev.$$lineRef;
        return "";
    }

    my $ret = getWhiteSpaceCommentsAndPreProcs($lineRef, $in);
    $prev  .= $ret;

    if (($ret = getString($lineRef, $in)) ne '"C"') {
        $$lineRef = $prev.$ret.$$lineRef;
        return "";
    }

    $prev .= $ret;
    $ret   = getWhiteSpaceCommentsAndPreProcs($lineRef, $in);
    $prev .= $ret;

    if (!defined($$lineRef) || (substr($$lineRef, 0, 1) ne "{")) {
        $$lineRef = $prev;
        return "";
    }

    $$lineRef = substr($$lineRef, 1);
    $numBlocks++;
    return $prev."{";
}

# Get a C preprocessor token.
# See: http://gcc.gnu.org/onlinedocs/gcc-3.3.6/cpp/index.html
#
sub getToken
{
    my ($lineRef, $in) = @_;
    my $ret = "";

    if (!defined($$lineRef) || ($$lineRef eq "")) {
        return "";
    }

    # If its an identifier, return it.
    #
    if ($$lineRef =~ /^([A-Za-z_\$][A-Za-z0-9_\$]*)(.*)$/s) {
        $$lineRef = $2;
        print('~I~'.$1) if ($opts{v});
        return $1;
    }

    # If its a C preprocessor number, return it.
    #
    if ($$lineRef =~ /^(\.?[0-9]([A-Za-z0-9_\.]|[eEpP][+-])*)(.*)$/s) {
        $$lineRef = $3;
        print('~N~'.$1) if ($opts{v});
        return $1;
    }

    # If its a string literal, return it.
    #
    if (($ret = getString($lineRef, $in)) ne "") {
        return $ret;
    }

    # According to the spec, should now maximally match symbol tokens. For now,
    # just return them character by character.
    #
    $ret = substr($$lineRef, 0, 1);
    $$lineRef = substr($$lineRef, 1);

    if ($$lineRef eq "") {
        $$lineRef = getLine($in);
    }

    print('~T~'.$ret) if ($opts{v});
    return $ret;
}

# Get a block. Note that the opening "{" has already been parsed.
#
sub getBlock
{
    my ($lineRef, $in) = @_;
    defined($in) or croak("Bad file handle.\n");
    my $prev = "";

    $numBlocks++;

    while (defined($$lineRef)) {
        $prev .= getWhiteSpaceCommentsAndPreProcs($lineRef, $in);
        my $ret = getToken($lineRef, $in);

        if ($ret eq "") {
            last;
        }

        if ($ret eq "{") {
            $prev .= "{".getBlock($lineRef, $in);
            next;
        }

        if ($ret eq "}") {
            print "~B~".$prev."}" if ($opts{v});
            $numBlocks--;
            return $prev."}";
        }

        $prev .= $ret;
    }

    warn("Unexpected EOF in block (depth = $numBlocks)\n");
    exit(1) if ($opts{e});
    return $prev;
}

# Get an expression. Note that the opening "(" has already been parsed.
#
sub getExpr
{
    my ($lineRef, $in) = @_;
    my $prev = "";

    while (defined($$lineRef)) {
        $prev .= getWhiteSpaceCommentsAndPreProcs($lineRef, $in);
        my $ret = getToken($lineRef, $in);

        if ($ret eq "") {
            last;
        }

        if ($ret eq "(") {
            $prev .= "(".getExpr($lineRef, $in);
            next;
        }

        if ($ret eq ")") {
            print "~E~".$prev."}" if ($opts{v});
            return $prev.")";
        }

        $prev .= $ret;
    }

    warn("Unexpected EOF in expression\n");
    exit(1) if ($opts{e});
    return $prev;
}

sub protoGetReturn
{
    my ($proto, $func) = @_;

    $proto =~ /^\s*(.*\S)\s*\b$func\s*\((.*)$/
        or die("Function $func: Invalid return type in prototype: $proto\n");

    my $type   = $1;
    my $params = $2;

    $type =~ s/\s*\binline\b\s*//;
    $type =~ s/\s*\bnoinline\b\s*//;
    getExpr(\$params) or die("Function $func: Invalid return type in prototype: $proto\n");

    if (!defined($params) || ($params eq " ")) {
        $params = "";
    }

    return $type.$params;
}

sub getSimpleParam
{
    my ($def) = @_;

    if ($def =~ /^(.*?)\s*([A-Za-z_\$][A-Za-z0-9_\$]*)$/) {
        return [$1, $2];
    }

    die("Invalid parameter name in definition: '$def'");
}

##
# Given a prototype, get the parameters
#
# @param proto The prototype
# @param func  The function name
#
# @return A list of pairs of [type, parameter name]
#
sub protoGetParams
{
    my ($proto, $func) = @_;
#    print "protoGetParams(proto='$proto',func='$func')\n";

    $proto =~ /^.*\W$func\s*\((.*)\)\s*$/
        or die("Invalid parameter list in prototype for function $func: "
               .$proto."\n");

    my $def    = $1;
    my $params = [];

    # C special case (void) and C++ special case '()'
    #
    if (($def =~ /^\s*void\s*$/) || ($def =~ /^\s*$/)) {
        return $params;
    }

    # While there is more than one simple parameter
    #
    while ($def =~ /^(.*?)([,\(])\s*(.*)/) {
        my $type = $1;

        # If its a simple parameter, push it.
        #
        if ($2 eq ",") {
            $def = $3;
            push(@{$params}, getSimpleParam($type));
            next;
        }

        # Found a '('

        $type .= "(";
        $def   = $3;
        my $expr;

        # Not '(*'
        #
        if (substr($def, 0, 1) ne "*") {
            ($expr = getExpr(\$def)) or die("Function $func: Unmatched '(' in parameter list");

            warn("Function $func: '(' in parameter list is not followed by"
                 ." a function pointer");
            $type .= $expr;
            next;
        }

        # Any number of '*'s allowed.
        #
        do {
            $type .= "*";
            $def =~ s/^\*\s*//;
        } while (substr($def, 0, 1) eq "*");

        my $ident = getIdentifier(\$def);

        if ($ident eq "") {
            if (($expr = getExpr(\$def)) eq "") {
                die("Function $func: Unmatched '(' in parameter list after"
                    ." $type");
            }

            warn("Function $func: '$type' in parameter list is not followed by"
                 ." a function name");
            $type .= $expr;
            next;
        }

        my $token = getToken(\$def);

        if ($token ne ")") {
            if (($expr = getExpr(\$def)) eq "") {
                die("Function $func: Unmatched '(' in parameter list after"
                    ." $type");
            }

            warn("Function $func: '$type' in parameter list is not followed by"
                 ." a function name");
            $type .= $expr;
            next;
        }

        $type .= ")";
        $token = getToken(\$def);

        if ($token ne "(") {
            warn("Function $func: '$type' in parameter list is not followed by"
                 ." a parameter list");
            $type .= $token;
            next;
        }

        ($expr = getExpr(\$def)) or die("Function $func: Unmatched '(' in parameter list after $type");
        push(@{$params}, [$type."(".$expr, $ident]);

        # Closing parenthesis was stripped, so this may be it
        #
        if (!defined($def)) {
            return $params;
        }

        $token = getToken(\$def);

        # Return type must have a trailing parameter list
        #
        if ($token eq ")") {
            return $params;
        }

        ($token eq ",") or die("Function $func: parameter $ident followed by '$token', not ','");
    }

    if ($def =~ /\s*\.\.\.\s*/) {
        push(@{$params}, ["...", "..."]);
    }
    else {
        push(@{$params}, getSimpleParam($def));
    }

    return $params;
}

my %cTypes = ('char' => 'c', 'unsigned char' => 'c', 'signed char' => 'c', 'short' => 'hd',
              'unsigned short' => 'hu', 'int' => 'd', 'unsigned' => 'u', 'unsigned int' => 'u',
              'long' => 'ld', 'unsigned long' => 'lu', 'char *' => 's', double => 'f' );

sub typeToFormat
{
    my ($type) = @_;
    my $format = $cTypes{$type} || 'p';

    return "%".$format;
}

my $mockFile = "";
my $mockHand;

# write a mock. file = filename or "" to close last.
#
sub writeMock
{
    my ($file, $line, $func, $proto) = @_;

    if ($file ne $mockFile) {
        if ($mockFile ne "") {
            close($mockHand);
        }
    }

    if ($file eq "") {
        return;
    }

    if ($file ne $mockFile) {
        $mockFile = $file;
        open($mockHand, "> ".substr($file, 0, length($file) - 2)."_mock__.c")
            or die("genxface.pl: Can't open output file "
                   .substr($file, 0, length($file) - 2)."_mock.c\n");
        print $mockHand ("/*\n * Generated by genxface.pl - DO NOT EDIT!\n */\n\n");
    }

    print $mockHand ("extern ".$proto.";\n");
    my $return = protoGetReturn($proto, $func);
    my $params = protoGetParams($proto, $func);

    my $mock = $proto;
    $mock    =~ s/$func/(*${func}_mock__)/;
    print $mockHand ("#ifdef MOCK\n");
    print $mockHand ($mock." = ".$func.";\n");
    print $mockHand ("#endif\n");

    my $wrap = $proto;
    $wrap    =~ s/$func/${func}_wrap__/;
    print $mockHand ("#ifdef DEBUG\n");
    print $mockHand ($wrap."\n");
    print $mockHand ("{\n");

    if ($return ne "void") {
        print $mockHand ("    $return ret;\n");
    }

    print $mockHand ("    clog_enter(\"$func\", \"$file\", $line, CLOG_LEVEL_DEBUG, \"");

    my $args = "";

    if (scalar(@${params}) > 0) {
        my $param = shift(@${params});
        $args .= $param->[1];
        print $mockHand ($args."=".typeToFormat($param->[0]));

        foreach my $param (@${params}) {
            $args .= ", ".$param->[1];
            print $mockHand (",".$param->[1]."=".typeToFormat($param->[0]));
        }
    }

    print $mockHand ("\"".($args ne "" ? ", ".$args : "")."\)\n");
    print $mockHand ("#ifdef MOCK\n");
    print $mockHand ("    ".($return ne "void" ? "ret = " : "")."(*".$func."_mock__)(".$args.");\n");
    print $mockHand ("#else\n");
    print $mockHand ("    ".($return ne "void" ? "ret = " : "").$func."(".$args.");\n");
    print $mockHand ("#endif\n");

    if ($return ne "void") {
        print $mockHand ("    clog_return(\"$file\", $line, CLOG_LEVEL_DEBUG, \""
                         .typeToFormat($return)."\", ret);\n");
        print $mockHand ("    return ret;\n");
    }
    else {
        print $mockHand ("    clog_return(\"$file\", $line, CLOG_LEVEL_DEBUG, 0);\n");
    }

    print $mockHand ("}\n");
    print $mockHand ("#endif\n\n");
}

sub usage
{
    die("usage: genxface.pl [-dehmqv] [-o output-dir] [directory|file ...]\n".
        "  -d = generate interface file per directory instead of per file\n".
        "  -e = exit on errors.\n".
        "  -h = help\n".
        "  -m = generate mock functions in filename_mock__.c for .c files.\n".
        "  -o = subdirectory to write generated files to (default = .)\n".
        "  -q = don't print warning messages on errors.\n".
        "  -v = verbose output.\n");
}

my $curFile = "";
my $out;

# Start and stop generated header files
#
sub setFile
{
    my ($file) = @_;
    defined($file) or die("Must pass a file name or an empty string");

    # No change in file name
    #
    if ($curFile eq $file) {
        return;
    }

    my $dir = dirname($file);

    # Are we generating per directory with no change in the directory name?
    #
    if ($opts{d} && ($file ne "") && ($dir eq dirname($curFile))) {
        return;
    }

    # If there's already a prototype file in progress, close it.
    #
    if ($curFile) {
        print $out ("\n");
        print $out ("#ifdef __cplusplus\n");
        print $out ("}\n");
        print $out ("#endif\n");
        close($out);
    }

    $curFile = $file;

    # Are we done the last prototype file?
    #
    if ($file eq "") {
        return;
    }

    my $name = $dir;

    if ($opts{o}) {
        #old                           $name .= "/".$opts{o};
        if ( $opts{o} =~ m~^[/\\]~ ) { $name  =     $opts{o}; }
        else                         { $name .= "/".$opts{o}; }
        mkdir($name);
    }

    if ($opts{d}) {
        $name .= "/".basename($dir);
    }
    else {
        $name .= "/".basename($file, ".c");
    }

    $name .= "-proto.h";

    open($out, "> $name")
        or die("genxface.pl: Can't open output file '$name'.\n");

    print $out ("/*\n");
    print $out (" * Generated by genxface.pl - DO NOT EDIT OR COMMIT TO SOURCE CODE CONTROL!\n");
    print $out (" */\n");
    print $out ("#ifdef __cplusplus\n");
    print $out ("extern \"C\" {\n");
    print $out ("#endif\n");
    print $out ("\n");
}

# Main entry point.
#

my $prefix     = "genxface.pl";
my $input_file = "-";

$SIG{'__WARN__'} = sub {
    print STDERR ($prefix.":".$input_file.":".$..": ", @_) if $opts{w};
};

if (!getopts('dehmo:qv', \%opts)) {
    usage();
}

if ($opts{h}) {
    usage();
}

if ($opts{q}) {
    $opts{w} = undef;
}

my $dirname;

if (scalar(@ARGV) == 0) {
    $ARGV[0] = getcwd();
}

my @files = ();

foreach my $name (@ARGV)
{
    if (-d "$name") {
        if ($opts{d}) {
            push(@files, glob($name."/*.h"));    # Look for inline functions: They need prototypes too (wierdly enough)
        }

        push(@files, glob($name."/*.c"));
        next;
    }

    if (-f "$name") {
        $opts{d} and die("genxface.pl: -d specified but '$name' is a file.\n");
        push(@files, $name);
        next;
    }

    die("genxface.pl: '$name' is not a file or directory.\n");
}

if (scalar(@files) == 0) {
    print STDERR ("genxface.pl: No .c files found in (".join(',', @ARGV).").\n");
    return 1;
}

my %symbolTable = ();
my $lastIdent   = undef;
my $lastLineNum = undef;
my $foundProto  = undef;

foreach my $file (@files) {
    if ($file =~ /_mock__\.c$/) {
        next;
    }

    $file =~ /([^\/]+)$/;
    $input_file = $1;

    open(my $in, $file) or die("genxface.pl: Can't open file $file.\n");
    print("~~~~~ ".$file." ~~~~~\n") if ($opts{v});
    my $line = <$in>;

LINE:
    while (defined($line)) {
        getWhiteSpace(\$line, $in);

        # If there's a comment, skip it.
        #
        if (getComment(\$line, $in) ne "") {
            next;
        }

        # If there's a preprocessor directive, skip it.
        #
        if (getPreProc(\$line, $in) ne "") {
            next;
        }

        # If its a C++ extern "C" { directive, ignore it.
        #
        if (getExternC(\$line, $in) ne "") {
            next;
        }

        my $prev = "";
        my $ret = getToken(\$line, $in);

        # Look for a block, an open parenthesis (which could be the beginning of
        # a function declaration) or a statement end.
        #
        while ($ret ne "") {
            $prev .= $ret;

            if ($ret =~ /^[A-Za-z_\$]/) {
                $lastIdent   = $ret;
                $lastLineNum = $.;
            }

            # If this is the end of a statement, skip the whole thing.
            #
            if ($ret eq ";") {
                print("~S~".$prev) if $opts{v};
                next LINE;
            }

            # If this is the block of an enum, struct or union declaration,
            # skip it.
            #
            if ($ret eq "{") {
                getBlock(\$line, $in);
                # Trailing ; will get cleaned up as an empty statement.
                next LINE;
            }

            # Found a '('.  This is either a function definition, a function
            # prototype, a function pointer or an expression in an initializer.
            #
            if ($ret eq "(") {
                $prev .= getExpr(\$line, $in);

                if (getWhiteSpaceCommentsAndPreProcs(\$line, $in) ne "") {
                    $prev .= " ";
                }

                # If it's not a prototype, loop.
                #
                if (($ret = getToken(\$line, $in)) ne "{") {
                    next;
                }

                # Found a prototype! If its a duplicate, warn and skip.
                #
                if ($symbolTable{$lastIdent})
                {
                    warn("Duplicate definition of global function '"
                             .$lastIdent."'\n");
                    getBlock(\$line, $in);
                    next LINE;
                }

                # If it's static, skip the function body and continue.
                #
                if ($prev =~ /^static\s/) {
                    getBlock(\$line, $in);
                    next LINE;
                }

                $symbolTable{$lastIdent} = $lastLineNum;
                setFile($file);

                if ($opts{m}) {
                    print $out ("#if !defined(MOCK) && !defined(DEBUG)\n");
                }

                print $out ($prev.";\n");

                if ($opts{m}) {
                    writeMock($file, $lastLineNum, $lastIdent, $prev);
                    print $out ("#else if defined(DEBUG)\n");
                    $prev =~ s/$lastIdent\s*\(/${lastIdent}_wrap__\(/;
                    print $out ($prev.";\n");
                    print $out ("#define ".$lastIdent." ".$lastIdent."_wrap__\n");
                    print $out ("#else\n");
                    $prev =~ s/${lastIdent}_wrap__/\(\*${lastIdent}_mock__\)/;
                    print $out ("extern ".$prev.";\n");
                    print $out ("#define ".$lastIdent." (*".$lastIdent."_mock__)\n");
                    print $out ("#endif\n");
                    print $out ("\n");
                }

                getBlock(\$line, $in);
                next LINE;
            }

            if ($ret eq "}") {
                if ($numBlocks < 1) {
                    warn("Unexpected end of block\n");
                }
                else {
                    $numBlocks--;
                }
            }

            if (!defined($line)) {
                last;
            }

            # Convert any non-tokens to single blanks. Nuke preprocessing
            # directives!
            #
            if (getWhiteSpaceCommentsAndPreProcs(\$line, $in) ne "") {
                $prev .= " ";
            }

            $ret = getToken(\$line, $in);
        }

        last;
    }

    close($in);
}

setFile("");
