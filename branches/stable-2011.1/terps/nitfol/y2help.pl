#!/usr/bin/perl -w
use strict;

# Generate help (both C and texinfo) from inform.y
#
# This one doesn't even pretend to be nitfol-independant.

my $helptext;
my $helpcommand;
my $helpargs;
my @helpentry;
my %helptable;

# command => [ [ argtype1, argtype2, ... ], helptext [ helpname, ... ] ]

while(<>) {
    if(/^\/\* :: (.*) \*\//) {
	$helptext = $1;
	$_ = <>;
	/\| (\S+)\s *(.*?(\/\*.*?\*\/)?.*?)\{/;
	$helpcommand = $1;
	$_ = $2;
	s/\/\*(.*?)\*\//$1/;
	s/\'(.)\'/$1/;
	s/NUM/\@var\{num\}/g;
	s/FILE/\@var\{file\}/g;
	s/commaexp/\@var\{exp\}/g;
	s/linespec/\@var\{linespec\}/g;
	s/IF/if/g;               # Ugly, but oh well...
	s/TO/to/g;
	$helpargs = $_;
	if($helptable{$helpcommand}[0]) {
	    push @{ $helptable{$helpcommand}[0] }, $helpargs;
	    $helptable{$helpcommand}[1] = $helptable{$helpcommand}[1] . "\\n" . $helptext;
	} else {
	    @{ $helptable{$helpcommand}[0] } = ( $helpargs );
	    $helptable{$helpcommand}[1] = $helptext;
	}
    } elsif(/static name_token infix_commands/) {
	while(<> =~ /\{\s*(\S+)\,\s*\"(.*?)\"\s*\}/) {
	    $helpcommand = $1; $helptext = $2;
	    push @{ $helptable{$helpcommand}[2] }, $helptext;
	}
    }
}

open("MYTEXINFO", ">dbg_help.texi") || die "Unable to write to dbg_help.texi";
select "MYTEXINFO";

foreach $helpcommand ( keys %helptable) {
    my $tag = "\@item ";
    foreach my $helparg (@{ $helptable{$helpcommand}[0] }) {
	print $tag, @{$helptable{$helpcommand}[2]}[0], " $helparg\n";
	$tag = "\@itemx ";
    }
    $_ = $helptable{$helpcommand}[1];
    s/\\n/  /g;
    print "$_\n\n";
}

close "MYTEXINFO";

open("MYCHELP",   ">dbg_help.h")    || die "Unable to write to dbg_help.c";
select "MYCHELP";

print "static name_token command_help[] = {\n";

my $flag = 0;
foreach $helpcommand ( keys %helptable) {
    if($flag) {
	print ",\n";
    }
    $flag = 1;
    print "  { $helpcommand, \"", texi2txt($helptable{$helpcommand}[1]), "\" }";
}
print "\n};\n";
close "MYCHELP";

sub texi2txt
{
    $_ = $_[0];
    s/\@code\{(.*?)\}/\'$1\'/g;
    s/\@file\{(.*?)\}/\'$1\'/g;
    s/\@\@/\@/g;
    return $_;
}
