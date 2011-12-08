/*
 *   input tests - input, inputKey, inputEvent, inputDialog
 */

#include "t3.h"
#include "tads.h"

_say_embed(str) { tadsSay(str); }

class RuntimeError: object
    construct(errno, ...) { errno_ = errno; }
    display = "Runtime Error: <<errno_>>"
    errno_ = 0
;

_main(args)
{
    try
    {
        t3SetSay(_say_embed);
        main();
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

function main()
{
    local x;
    
    "Input and input key tests.\n";

    "Enter a command: ";
    x = inputLine();
    "You entered: \"<<x>>\"\n";

    "Press a key...";
    x = inputKey();
    "You typed: \"<<x>>\"\n";

    "Enter an event...";
    x = inputEvent();
    "Event code = <<x[1]>>, additional data = \"<<
        x.length() > 1 ? x[2] : "">>\"\n";

    "Dialog...\n";
    x = inputDialog(InDlgIconNone, 'This is a sample dialog prompt!',
                    ['Hello', 'Goodbye', 'Anyway'], 1, 2);
    "Response = <<x>>\n";

    x = inputDialog(InDlgIconQuestion,
                    'A standard yes/no dialog!', InDlgYesNo, 1, 2);
    "Response = <<x>>\n";

    x = inputDialog(InDlgIconInfo,
                    'Another dialog!', [InDlgLblOk, InDlgLblNo],
                    nil, nil);
    "Response = <<x>>\n";

    "Done!!!\n";
}

