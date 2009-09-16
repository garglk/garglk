#!/usr/bin/perl

while (<>)
{
	if ( m/^KPX ([A-Za-z0-9_.]*) ([A-Za-z0-9_.]*) ([0-9-]*)/ )
	{
		$newkpx = $3 * 50 / 100;
		print "KPX $1 $2 $newkpx\n";
	}
	else
	{
		print $_;
	}
}

