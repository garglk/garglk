#include <tads.h>
#include <bytearr.h>

main(args)
{
    local b;
    
    "--- integer writing ---\n";
    b = new ByteArray(200);
    b.fillValue(0, 1, 256);
    b.writeInt(001, FmtInt8, 200 /* 0xC8 */);
    b.writeInt(011, FmtInt8, -75 /* 0xB5 */);
    b.writeInt(021, FmtInt16LE, 41000 /* 0xA028 */);
    b.writeInt(031, FmtInt16LE, -21000 /* 0xADF8 */);
    b.writeInt(041, FmtInt16BE, 42000 /* 0xA410 */);
    b.writeInt(051, FmtInt16BE, -22000 /* 0xAA10 */);
    b.writeInt(061, FmtInt32LE, 600000 /* 0x000927C0 */);
    b.writeInt(071, FmtInt32LE, -600000 /* 0xFFF6D840 */);
    b.writeInt(0101, FmtInt32BE, 700000 /* 0x000AAE60 */);
    b.writeInt(0111, FmtInt32BE, -700000 /* 0xFFF551A0 */);

    for (local i = 1 ; i <= b.length() ; i += 8)
    {
        "<<fmtX(i, 8, 4)>>: ";
        for (local j = i ; j < i + 8 ; ++j)
            "<<fmtX(b[j], 16, 2)>> ";
        "\n";
    }

    "\b--- integer reading ---\n";
    "int8 - \n
        \tunsigned: <<b.readInt(001, FmtUInt8)>>\n
        \tsigned: <<b.readInt(011, FmtInt8)>>\n";
    "int16 - \n
        \tunsigned, LE: <<b.readInt(021, FmtUInt16LE)>>\n
        \tsigned, LE: <<b.readInt(031, FmtInt16LE)>>\n
        \tunsigned, BE: <<b.readInt(041, FmtUInt16BE)>>\n
        \tsigned, BE: <<b.readInt(051, FmtInt16BE)>>\n";
    "int32 - \n
        \tLE: <<b.readInt(0061, FmtInt32LE)>>\n
        \tLE: <<b.readInt(0071, FmtInt32LE)>>\n
        \tBE: <<b.readInt(0101, FmtInt32BE)>>\n
        \tBE: <<b.readInt(0111, FmtInt32BE)>>\n";
}

fmtX(val, radix, len)
{
    local str = toString(val, radix);
    local slen = str.length();
    if (slen < len)
        str = makeString('0', len - slen) + str;
    return str;
}
