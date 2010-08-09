#!/usr/bin/perl

use strict;
use warnings;
use POSIX;

my $fd;

# Flush output on every line
#
$| = 1;

while (my $arg = shift(@ARGV)) {
    if ($arg eq "-n") {
        print "\n";
    }
    elsif ($arg eq "-L") {
        if (!($fd = POSIX::open("command-lock", &POSIX::O_CREAT | &POSIX::O_EXCL))) {
            print STDERR ("command.pl -L: Command is locked out\n");
            exit(1);
        }
    }
}

while(my $line = <>) {
    if ($line =~ /^abort/) {
        abort();
    }

    if ($line =~ /^sleep (\d+)/) {
        sleep($1);
    }

    print $line;
}

if (defined($fd)) {
    close($fd);
    unlink("command-lock");
}
