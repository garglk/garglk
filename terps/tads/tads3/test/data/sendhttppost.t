#include <tads.h>
#include <tadsnet.h>
#include <file.h>

main(args)
{
    if (args.length() < 2)
    {
        "usage: t3run sendhttppost &lt;url&gt;\n";
        return;
    }

    local headers = [
        'X-TADS-Client: sendhttpreq',
        'X-TADS-Test-1: One',
        'X-TADS-Test-2: Two',
        'X-TADS-Test-3: Three'
    ];

    local fba = new ByteArray(100);
    local fields = [
        'fieldOne' -> '123',
        'fieldTwo' -> 'four five six',
        'fieldThree' -> '7 eight 9 ten',
        'fileField' -> new FileUpload(fba, 'image/jpeg', 'testfile.jpg')
    ];

    "Sending request...\n";
    sendNetRequest('RequestOne', args[2], 'POST', 0,
                   headers.join('\r\n'), fields);

    "Awaiting reply...";
    for (local done = nil ; !done ; )
    {
        flushOutput();
        local evt = getNetEvent(1000);
        switch (evt.evType)
        {
        case NetEvTimeout:
            ".";
            break;

        case NetEvReply:
            "\n***** Got result for request <<evt.requestID>>! *****\n";
            "--- HTTP status line = <<evt.httpStatusLine>>
               (code=<<evt.statusCode>>)\n";
            if (evt.statusCode > 0)
            {
                showBody(evt.replyBody);
                "--- Headers ---\n<<
                  evt.replyHeaders.forEachAssoc(function(name, val) {
                    tadsSay('\t' + name + '=' + val.htmlify() + '\n');
                })>>\b";
                "--- Raw Headers ---\n<<evt.replyHeadersRaw.htmlify()>>\b";
            }
            done = true;
            break;
        }
    }

    "Done\n";
}

showBody(f)
{
    if (f == nil)
    {
        "--- No Content ---\n";
    }
    else if (f.getFileMode() == FileModeText)
    {
        "--- Content (Text) ---\n";
        local txt;
        while ((txt = f.readFile()) != nil)
            "<<txt.htmlify()>>\n";
        "\b";
    }
    else
    {
        "--- Content (Binary) ---\n";
        local txt;
        local addr = 0;
        for ( ; ; addr += 32)
        {
            txt = f.unpackBytes('H29*!');
            if (txt.length() == 0 || txt[1] == '')
                break;
            tadsSay(sprintf('%08x\ \ %s\n', addr, txt[1]));
        }
        "\b";
    }
}
