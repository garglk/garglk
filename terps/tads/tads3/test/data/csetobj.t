#include <tads.h>
#include <charset.h>

main(args)
{
    local cs;
    local mappable;
    local s;
    local b;

    /* test some simple US-ASCII mappings */
    cs = new CharacterSet('us-ascii');
    "us-ascii:\n";
    "\texists? <<cs.isMappingKnown() ? 'yes' : 'no'>>\n";
    "\tcan map 0x77? <<cs.isMappable(0x77) ? 'yes' : 'no'>>\n";
    "\tcan map 'abc'? <<cs.isMappable('abc') ? 'yes' : 'no'>>\n";
    "\tcan map 0x10CD? <<cs.isMappable(0x10CD) ? 'yes' : 'no'>>\n";
    "\tcan map 'abc\u1011\u1012def'? <<
        cs.isMappable('abc\u1011\u1012def') ? 'yes' : 'no'>>\n";
    "\b";

    /* test local character set operations */
    "Local display character set =
        <<getLocalCharSet(CharsetDisplay)>>\n";
    "Local file system character set =
        <<getLocalCharSet(CharsetFileName)>>\n";
    "\b";

    /* test for the local presence of the Cyrillic alphabet */
    cs = new CharacterSet(getLocalCharSet(CharsetDisplay));
    "Can the local display character set show the Cyrillic alphabet? ";
    for (mappable = 'yes', local ch = 0x0410 ; ch <= 0x044F ; ++ch)
    {
        if (!cs.isMappable(ch))
        {
            mappable = 'no';
            break;
        }
    }
    "<<mappable>>\b";

    /* iso-8859-1 - unicode to local */
    cs = new CharacterSet('iso-8859-1');
    "Some ISO 8859-1 mappings: unicode to local:\n";
    s = 'test \x91\x92\u1024 done';
    b = s.mapToByteArray(cs);
    "'<<s>>' -> ";
    for (local i = 1 ; i <= b.length() ; ++i)
        "[<<i>>]=<<b[i]>>\t";
    "\b";

    /* iso8859-1 - local to unicode */
    "ISO 8859-1: local to unicode\n";
    for (local i = 1 ; i <= b.length() ; ++i)
        b[i] = 64 + i;
    "'<<b.mapToString(cs)>>'\b";
}

