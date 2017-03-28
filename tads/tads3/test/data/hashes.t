#include <tads.h>
#include <bytearr.h>
#include <file.h>

main(args)
{
    local str = 'This is a test string to check out the hash functions!';
    "String = <<str>>\n";
    "md5 = <<str.digestMD5()>>\n";
    "sha256 = <<str.sha256()>>\n";

    local arr = new ByteArray(95);
    for (local i = 1 ; i <= 95 ; ++i)
        arr[i] = i + 31;

    "\bByteArray = <<arr.mapToString(new CharacterSet('latin1')).htmlify()>>\n";
    "md5 = <<arr.digestMD5()>>\n";
    "sha256 = <<arr.sha256()>>\n";

    "\bByteArray range 34..58 (A-Z)\n";
    "md5 = <<arr.digestMD5(34, 26)>>\n";
    "sha256 = <<arr.sha256(34, 26)>>\n";

    local fp = File.openRawFile(
        FileName.fromUniversal('../data/hashes.src'), FileAccessRead);

    "\bFile 'hashes.t'\n";
    "md5 = <<fp.digestMD5()>>\n";
    "sha256 = <<fp.setPos(0), fp.sha256()>>\n";

    "\bFile 'hashes.t', range idx=256 len=3000\n";
    "md5 = <<fp.setPos(256), fp.digestMD5(3000)>>\n";
    "sha256 = <<fp.setPos(256), fp.sha256(3000)>>\n";
}

