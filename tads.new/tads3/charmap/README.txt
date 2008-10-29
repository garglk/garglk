All TADS 3 distributions include the character mappings listed below,
provided courtesy of Ilya Goz.  The mappings files are compiled and
collected together into the single file cmaplib.t3r - this file uses
the TADS 3 resource library file format to store the full set of
mapping files.  The set below should fill the needs of most users on
most systems.

If you're building from the source code distribution, note that the
Windows makefile includes build instructions to create the cmaplib.t3r
file based on the .tcm files listed below.  You do NOT need to build
cmaplib.t3r, because it's included in the source distribution - for
most platforms, you should simply copy this file directly to the
directory containing the TADS 3 executables.  Note that cmaplib.t3r
uses a binary-portable format, so the same cmaplib.t3r works on every
platform - there's no need to recompile it locally for byte-order
differences or anything else.  The source files for the mappings are
included with the source distribution; note that these are based on
the mapping tables available at the Unicode website (www.unicode.org),
but are extended beyond the basic Unicode tables with numerous
approximation mappings.

If you need a character set mapping that isn't in the set provided
below, you can use the character map compiler tool (mkchrtab) to
create a .tcm file for your character set, and place it in the CHARMAP
subdirectory of the main TADS 3 executables directory.  For
information on the TADS 3 Unicode-based character set translation
system, including details on how to construct and compile your own
mapping files, please refer to the file "t3cmap.htm" in the "doc"
subdirectory of the TADS 3 source distribution.


ascii.tcm   7-bit ASCII
cp1250.tcm  cp1250 (Windows, Latin-2, East Europe)
cp1251.tcm  cp1251 (Windows, Cyrillic)
cp1252.tcm  cp1252 (Windows, Latin-1, West Europe)
cp1253.tcm  cp1253 (Windows, Greek)
cp1254.tcm  cp1254 (Windows, Turkish)
cp1255.tcm  cp1255 (Windows, Hebrew)
cp1256.tcm  cp1256 (Windows, Arabic)
cp1257.tcm  cp1257 (Windows, Baltic)
cp1258.tcm  cp1258 (Windows, Vietnamese)
cp437.tcm   cp437 (DOS, Latin US)
cp737.tcm   cp737 (DOS, Greek)
cp775.tcm   cp775 (DOS, Baltic)
cp850.tcm   cp850 (DOS, Latin-1, West Europe)
cp852.tcm   cp852 (DOS, Latin-2, East Europe)
cp855.tcm   cp855 (DOS, Cyrillic)
cp857.tcm   cp857 (DOS, Turkish)
cp860.tcm   cp860 (DOS, Portuguese)
cp861.tcm   cp861 (DOS, Icelandic)
cp862.tcm   cp862 (DOS, Hebrew)
cp863.tcm   cp863 (DOS, Canadian French)
cp864.tcm   cp864 (DOS, Arabic)
cp865.tcm   cp865 (DOS, Nordic)
cp866.tcm   cp866 (DOS, Cyrillic Russian)
cp869.tcm   cp869 (DOS, Greek2)
cp874.tcm   cp874 (DOS, Thai)
iso1.tcm    ISO/IEC 8859-1:1998
iso10.tcm   ISO/IEC 8859-10:1998
iso2.tcm    ISO 8859-2:1999 
iso3.tcm    ISO/IEC 8859-3:1999
iso4.tcm    ISO/IEC 8859-4:1998
iso5.tcm    ISO 8859-5:1999
iso6.tcm    ISO 8859-6:1999
iso7.tcm    ISO 8859-7:1987
iso8.tcm    ISO/IEC 8859-8:1999
iso9.tcm    ISO/IEC 8859-9:1999
koi8-r.tcm  KOI8-R (RFC1489, Cyrillic Russian)
mac.tcm     Mac OS Roman
macce.tcm   Mac OS Central European
maccyr.tcm  Mac OS Cyrillic
macgreek.tcm Mac OS Greek
maciceland.tcm  Mac OS Icelandic
mactur.tcm  Mac OS Turkish
