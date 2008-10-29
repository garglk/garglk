/*
 *   dictionary test 
 */

#include "tads.h"
#include "t3.h"
#include "dict.h"

obj1: object
    adjective = 'none'
    noun = 'none'
;

bluebook: object sdesc = "blue_book";
redbook: object sdesc = "red_book";
greenbook: object sdesc = "green_book";
blueball: object sdesc = "blue_ball";
redball: object sdesc = "red_ball";
greenball: object sdesc = "green_ball";

preinit()
{
}

main(args)
{
    local dict;

    /* create a dictionary */
    dict = new Dictionary(6);

    /* add some words */
    dict.addWord(bluebook, 'blue', &adjective);
    dict.addWord(redbook, 'red', &adjective);
    dict.addWord(greenbook, 'green', &adjective);

    dict.addWord(blueball, 'blue', &adjective);
    dict.addWord(redball, 'red', &adjective);
    dict.addWord(greenball, 'green', &adjective);

    dict.addWord(bluebook, 'book', &noun);
    dict.addWord(redbook, 'book', &noun);
    dict.addWord(greenbook, 'book', &noun);

    dict.addWord(blueball, 'ball', &noun);
    dict.addWord(redball, 'ball', &noun);
    dict.addWord(greenball, 'ball', &noun);

    /* start undo */
    savepoint();

    /* show instructions */
    "+word   - add a noun to bluebook\n
     -word   - remove a word\n
     word    - list objects matching word\n
     save    - save file\n
     restore - restore state\n
     undo    - undo state\n
     quit    - terminate\n";

    /* interactive command loop */
    for (;;)
    {
        local cmd;
        
        "\n>";
        cmd = inputLine();
        if (cmd == nil || cmd == 'quit')
            break;

        if (cmd.substr(1, 1) == '+')
        {
            savepoint();
            dict.addWord(bluebook, cmd.substr(2), &noun);
            "Added.\n";
        }
        else if (cmd.substr(1, 1) == '-')
        {
            savepoint();
            dict.removeWord(bluebook, cmd.substr(2), &noun);
            "Removed.\n";
        }
        else
        {
            local fname;
            
            switch(cmd)
            {
            case 'save':
                fname = inputFile('File to save', InFileSave,
                                  FileTypeT3Save, 0);
                if (fname[1] == InFileSuccess)
                {
                    if (saveGame(fname[2]) != nil)
                        "Restored.\n";
                    else
                        "Saved.\n";
                }
                break;
                
            case 'restore':
                fname = inputFile('File to restore', InFileOpen,
                                  FileTypeT3Save, 0);
                if (fname[1] == InFileSuccess)
                {
                    try
                    {
                        restoreGame(fname[2], 1);
                    }
                    catch (RuntimeError exc)
                    {
                        "Restore failed (<<exc.exceptionMessage>>).\n";
                    }
                }
                break;
                
            case 'undo':
                if (undo())
                    "Undid one command.\n";
                else
                    "No undo information available.\n";
                break;

            default:
                "noun '<<cmd>>': <<sayList(dict.findWord(cmd, &noun))>>\n";
                "adjective '<<cmd>>':
                    <<sayList(dict.findWord(cmd, &adjective))>>\n";
                break;
            }
        }
    }

    "Bye!\b";
}

sayList(lst)
{
    "[";
    for (local i = 1, local len = lst.length() ; i <= len ; ++i)
    {
        if (i > 1)
            ", ";
        lst[i].sdesc;
    }
    "]";
}
