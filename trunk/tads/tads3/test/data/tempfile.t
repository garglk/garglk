#include <tads.h>
#include <file.h>

main(args)
{
    local t = new TemporaryFile();
    "Temp file = <<t.getFilename()>>\n";

    local f = File.openTextFile(t, FileAccessWrite, 'ascii');
    for (local i in 1..10)
        f.writeFile('Test line #<<i>>!\n');
    f.closeFile();
    
    f = File.openTextFile(t, FileAccessRead, 'ascii');
    for (local i = 1 ;; ++i)
    {
        local l = f.readFile();
        if (l == nil)
            break;
        
        "<<sprintf('%06d %s\n', i, l)>>";
    }
    f.closeFile();
}
