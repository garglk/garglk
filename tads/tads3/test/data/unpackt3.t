#include <tads.h>
#include <file.h>

main(args)
{
    if (args.length < 2)
    {
        "usage: unpackt3 &lt;t3-file&gt;\n";
        return;
    }
    
    local fp = File.openRawFile(args[2], FileAccessRead);

    local sig = fp.unpackBytes('a11 CC a32 a24');
    "Format version: <<sig[3]>>.<<sig[2]>>\n";
    "Timestamp: <<sig[5]>>\n";

    for (local i = 1 ; ; ++i)
    {
        local blk = fp.unpackBytes('a4 L~ S');
        "Block <<i>>: id=<<blk[1]>>, <<blk[2]>> bytes, flags=<<
          toString(blk[3], 16)>>\n";

        if (blk[1] == 'EOF ')
            break;

        local endpos = fp.getPos() + blk[2];

        if (blk[1] == 'CPPG')
        {
            local pg = fp.unpackBytes('S L~ C');
            if (pg[1] == 2)
            {
                local arr = fp.unpackBytes('b<<blk[2] - 7>>')[1];
                local x = pg[3];
                for (local j = 1 ; j <= arr.length() ; ++j)
                    arr[j] ^= x;

                for (local j = 1 ; j <= arr.length() ; )
                {
                    local v;
                    try
                    {
                        v = arr.unpackBytes(j, 'S/a @?');
                    }	
                    catch (Exception exc)
                    {
                        break;
                    }
                    j += v[2];
                    if (rexSearch('[\x00-\x08\x0c\x0e-\x1f]', v[1]) == nil)
                        "\t<<rexReplace('[\n\r\t\b\\]', v[1], new function(m) {
                            return ['\n'->'\\n', '\r'->'\\b', '\t'->'\\t',
                                    '\b'->'\\b', '\\'->'\\\\'][m];
                        })>>\n";
                }
            }
        }

        fp.setPos(endpos);
    }
}

