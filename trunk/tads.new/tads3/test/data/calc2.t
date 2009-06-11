#include "tads.h"
#include "t3.h"
#include "dict.h"
#include "gramprod.h"
#include "tok.h"
#include "bignum.h"

enum token tokOp;
enum token tokFloat;

dictionary gDict;
dictionary property funcName;

/*
 *   Calc globals 
 */
calcGlobals: object
    /* 
     *   input precision - for any number entered with smaller precision,
     *   we'll increase the precision to this value 
     */
    inputPrecision = 8

    /* guard digits added to input precision */
    guardPrecision = 3

    /* number of fractional digits to display for each value */
    dispFracDigits = 8

    /* maximum number of digits to display for each value */
    dispMaxDigits = 32
    
    /* display in scientific notation */
    dispSci = nil
;


/*
 *   Custom tokenizer for arithmetic expressions
 */
CalcTokenizer: Tokenizer
    rules_ =
    [
        /* skip whitespace */
        ['[ \t]+', nil, tokCvtSkip, nil],

        /* numbers */
        ['(%.[0-9]+|[0-9]+(%.[0-9]*)?)([eE][+-]?[0-9]+)?',
         tokFloat, nil, nil],

        /* operators */
        ['[(+*-/)!^?]', tokOp, nil, nil],

        /* words */
        ['[a-zA-Z]+', tokWord, &tokCvtLower, nil]
    ]
;

grammar term: term->val1_ '+' atomic->val2_: object
    eval() { return val1_.eval() + val2_.eval(); }
;

grammar term: atomic->val_: object
    eval() { return val_.eval(); }
;

grammar atomic: number->num_ : object
    eval() { return num_.eval(); }
;

grammar atomic: '(' term->val_ ')' : object
    eval() { return val_.eval(); }
;

grammar number: tokFloat->num_ : object
    eval()
    {
        local val;

        /* parse the number */
        val = new BigNumber(num_);

        /* if the precision is smaller than the input minimum, increase it */
        if (val.getPrecision()
            < calcGlobals.inputPrecision + calcGlobals.guardPrecision)
            val = val.setPrecision(calcGlobals.inputPrecision
                                   + calcGlobals.guardPrecision);

        /* return the value */
        return val;
    }
;

main(args)
{
    "T3 Scientific Calculator\n
    Type ?\ for help, type Q or QUIT to quit.\n";
    
    for (;;)
    {
        local str, toks;
        local match;

        /* read a line */
        "\b>";
        str = inputLine();

        /* tokenize the string */
        try
        {
            toks = CalcTokenizer.tokenize(str);
        }
        catch (TokErrorNoMatch err)
        {
            "Invalid character '<<err.remainingStr_.substr(1, 1)>>'\n";
            continue;
        }

        /* if it's 'quit' or 'q', stop */
        if (toks.length() == 1
            && (getTokVal(toks[1]) is in('q', 'quit')))
            break;

        /* if it's '?', show help */
        if (toks.length() == 1
            && (getTokVal(toks[1]) == '?'))
        {
            showHelp();
            continue;
        }

        /* parse it */
        match = term.parseTokens(toks, gDict);

        /* if we didn't get anything, say so */
        if (match.length() == 0)
        {
            "Invalid expression\n";
        }
        else
        {
            local val;
            local flags;
            
            /* evaluate the expression */
            val = match[1][2].eval();
            if (val != nil)
            {
                /* clear the display flags */
                flags = 0;
                
                /* display in scientific or normal notation as desired */
                if (calcGlobals.dispSci)
                    flags |= BignumExp;
                
                /* display the value */                
                "<<val.formatString(calcGlobals.dispMaxDigits, flags,
                                    -1, calcGlobals.dispFracDigits)>>\n";
            }
        }
    }
}

showHelp()
{
    "Enter numbers in decimal or scientific notation:\b
    \t3.1415926\n
    \t1.705e-11\b
    Operators, in order of precedence:\b
    \ta ^ b\t\traise a to the power of b\n
    \ta * b\t\tmultiply\n
    \ta / b\t\tdivide\n
    \ta + b\t\tadd\n
    \ta - b\t\tsubtract\n
    \t( a )\t\toverride operator precedence\n
    \b
    Functions:\b
    \tsin(x)\t\ttrigonometric sine\n
    \tcos(x)\t\ttrigonometric cosine\n
    \ttan(x)\t\ttrigonometric tangent\n
    \tln(x)\t\tnatural logarithm\n
    \tlog(x)\t\tcommon (base-10) logarithm\n
    \texp(x)\t\traise e (the base of the natural logarithm) to power\n
    \b
    Constants:\b
    \tpi\t\t3.14159265\n
    \te\t\t2.7182818\n
    \b";
    "Commands:\b
    \tquit\t\tturn off the calculator\n
    \tsci N\t\tdisplay scientific notation with N digits after the decimal\n
    \tfix N\t\tdisplay fixed notation with N digits after the decimal\n
    \tprec N\t\tset default input precision to N digits\n
    \b";
}

