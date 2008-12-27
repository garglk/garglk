/*
 *   test adv3_num's spellInt() function
 */

#include "adv3.h"

main(args)
{
    if (args.length() < 2)
    {
        "usage: adv3_num <number> ...\n";
        return;
    }

    /* 
     *   spell each argument value, and give each number's Roman numeral
     *   representation 
     */
    for (local i = 2, local cnt = args.length() ; i <= cnt ; ++i)
    {
        "<<args[i]>>:\n";
        "\t<<spellInt(toInteger(args[i]))>>\n";
        "\t<<spellIntExt(toInteger(args[i]), SPELLINT_TEEN_HUNDREDS)>>\n";
        "\t<<spellIntExt(toInteger(args[i]), SPELLINT_AND_TENS)>>\n";
        "\t<<spellIntExt(toInteger(args[i]),
             SPELLINT_AND_TENS | SPELLINT_COMMAS)>>\n";
        "\t<<spellIntBelowExt(toInteger(args[i]), 100,
                              0, INTDEC_GROUP_SEP)>>\n";
        "\t<<intToRoman(toInteger(args[i]))>>\n";
    }
}

