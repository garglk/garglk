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
        ['whitespace', '[ \t]+', nil, &tokCvtSkip, nil],

        /* numbers */
        ['numbers', '(%.[0-9]+|[0-9]+(%.[0-9]*)?)([eE][+-]?[0-9]+)?',
         tokFloat, nil, nil],

        /* operators */
        ['operators', '[(+*-/)!^?]', tokOp, nil, nil],

        /* words */
        ['words', '[a-zA-Z]+', tokWord, &tokCvtLower, nil]
    ]
;

grammar command: expression->expr_ : object
    eval() { return expr_.eval(); }
;

class commandWithIntegerOp: object
    op_ = nil // this is the grammar element for the operand
    desc_ = "desc" // this is the command name
    opDesc_ = "operator desc" // description of operator's purpose
    eval()
    {
        try
        {
            local val = toInteger(op_.eval());
            evalWithOp(val);
        }
        catch (Exception exc)
        {
            "Invalid argument to '<<desc_>>' - please specify
            <<opDesc_>>\n";
        }
    }
;

grammar command: 'sci' number->op_ : commandWithIntegerOp
    desc_ = "sci"
    opDesc_ = "the number of digits to display after the decimal point"
    evalWithOp(val)
    {
        /* set scientific notation */
        calcGlobals.dispSci = true;

        /* note the number of digits after the decimal */
        calcGlobals.dispFracDigits = val;

        /* set the maximum digits */
        calcGlobals.dispMaxDigits = (val > 32 ? val + 5 : 32);

        /* explain the change */
        "Scientific notation, <<val>> digits after the decimal\n";
    }
;

grammar command: [badness 1] 'sci' *: object
    eval()
    {
        "Please enter in the format 'sci N', where N is the number of
        digits to display after the decimal point.\n";
    }
;

grammar command: 'fix' number->op_ : commandWithIntegerOp
    desc_ = "fix"
    opDesc_ = "the number of digits to display after the decimal point"
    evalWithOp(val)
    {
        /* set non-scientific notation */
        calcGlobals.dispSci = nil;

        /* note the number of digits after the decimal */
        calcGlobals.dispFracDigits = val;

        /* set the maximum digits */
        calcGlobals.dispMaxDigits = (val > 32 ? val + 12 : 32);

        /* explain the change */
        "Fixed notation, <<val>> digits after the decimal\n";
    }
;

grammar command: [badness 1] 'fix' *: object
    eval()
    {
        "Please enter in the format 'fix N', where N is the number of
        digits to display after the decimal point.\n";
    }
;

grammar command: 'prec' number->op_ : commandWithIntegerOp
    desc_ = "prec"
    opDesc_ = "the default input precision"
    evalWithOp(val)
    {
        /* note the new input precision */
        calcGlobals.inputPrecision = val;

        /* explain the change */
        "Input precision is now <<val>> digits\n";
    }
;

grammar command: [badness 1] 'prec' *: object
    eval()
    {
        "Please enter in the format 'prec N', where N is the number of
        digits to use for the default input precision.\n";
    }
;

grammar expression: term->val_: object
    eval() { return val_.eval(); }
;

grammar term: term->val1_ '+' factor->val2_: object
    eval() { return val1_.eval() + val2_.eval(); }
;

grammar term: term->val1_ '-' factor->val2_: object
    eval() { return val1_.eval() - val2_.eval(); }
;

grammar term: factor->val_: object
    eval() { return val_.eval(); }
;

grammar factor: factor->val1_ '*' power->val2_: object
    eval() { return val1_.eval() * val2_.eval(); }
;

grammar factor: factor->val1_ '/' power->val2_: object
    eval() { return val1_.eval() / val2_.eval(); }
;

grammar factor: power->val_: object
    eval() { return val_.eval(); }
;

grammar power: power->val1_ '^' prefix->val2_: object
    eval() { return val1_.eval().raiseToPower(val2_.eval()); }
;

grammar power: prefix->val_: object
    eval() { return val_.eval(); }
;

grammar prefix: funcName->func_ '(' expression->expr_ ')': object
    eval() { return gDict.findWord(func_, &funcName)[1].eval(expr_); }
;

grammar prefix: funcName->func_ prefix->expr_ : object
    eval() { return gDict.findWord(func_, &funcName)[1].eval(expr_); }
;

#define DefFunc(nm, func) \
  object funcName = #@nm eval(expr) { return expr.eval().func(); }

DefFunc(ln, logE);
DefFunc(log, log10);
DefFunc(exp, expE);
DefFunc(sin, sine);
DefFunc(cos, cosine);
DefFunc(tan, tangent);
DefFunc(asin, arcsine);
DefFunc(acos, arccosine);
DefFunc(atan, arctangent);
DefFunc(sqr, sqrt);
DefFunc(sqrt, sqrt);

#if 0
lnFunc: object
    funcName = 'ln'
    eval(expr)
    {
        return expr.eval().logE();
    }
;

logFunc: object
    funcName = 'log'
    eval(expr)
    {
        return expr.eval().log10();
    }
;

expFunc: object
    funcName = 'exp'
    eval(expr)
    {
        return expr.eval().expE();
    }
;

sinFunc: object
    funcName = 'sin'
    eval(expr)
    {
        return expr.eval().sine();
    }
;

cosFunc: object
    funcName = 'cos'
    eval(expr)
    {
        return expr.eval().cosine();
    }
;

tanFunc: object
    funcName = 'tan'
    eval(expr)
    {
        return expr.eval().tangent();
    }
;

asinFunc: object
    funcName = 'asin'
    eval(expr)
    {
        return expr.eval().arcsine();
    }
;

acosFunc: object
    funcName = 'acos'
    eval(expr)
    {
        return expr.eval().arccosine();
    }
;

atanFunc: object
    funcName = 'atan'
    eval(expr)
    {
        return expr.eval().arctangent();
    }
;

sqrFunc: object
    funcName = 'sqr'
    eval(expr)
    {
        return expr.eval().sqrt();
    }
;

sqrtFunc: object
    funcName = 'atan'
    eval(expr)
    {
        return expr.eval().sqrt();
    }
;
#endif

grammar prefix: '-' postfix->val_ : object
    eval() { return -val_.eval(); }
;

grammar prefix: '+' postfix->val_: object
    eval() { return val_.eval(); }
;

grammar prefix: postfix->val_ : object
    eval() { return val_.eval(); }
;

grammar postfix: atomic->val_ '!': object
    eval() { return factorial(val_.eval()); }
;

grammar postfix: atomic->val_ : object
    eval() { return val_.eval(); }
;

factorial(x)
{
    local prod;
    
    /* 
     *   do this iteratively rather than recursively, to allow for really
     *   big inputs without fear of blowing out the stack 
     */
    for (prod = 1.0 ; x > 1 ; --x)
        prod *= x;

    /* return the product */
    return prod;
}

grammar atomic: '(' expression->val_ ')': object
    eval() { return val_.eval(); }
;

grammar atomic: number->num_ : object
    eval() { return num_.eval(); }
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

grammar atomic: 'e' : object
    eval()
    {
        return BigNumber.getE(calcGlobals.inputPrecision
                              + calcGlobals.guardPrecision);
    }
;

grammar atomic: 'pi': object
    eval()
    {
        return BigNumber.getPi(calcGlobals.inputPrecision
                               + calcGlobals.guardPrecision);
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
            && (getTokVal(toks[1]) is in ('q', 'quit')))
            break;

        /* if it's '?', show help */
        if (toks.length() == 1
            && (getTokVal(toks[1]) == '?'))
        {
            showHelp();
            continue;
        }

        /* parse it */
        match = command.parseTokens(toks, gDict);

        /* if we didn't get anything, say so */
        if (match.length() == 0)
        {
            "Invalid expression\n";
        }
        else
        {
            local val;
            local flags;
            
            try
            {
                /* evaluate the expression */
                val = match[1].eval();

                /* if we got a valid, display it */
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
            catch (Exception exc)
            {
                "Evaluation error: <<exc.displayException()>>\n";
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
    \tasin(x)\t\tarcsine\n
    \tacos(x)\t\tarccosine\n
    \tatan(x)\t\tarctangent\n
    \tln(x)\t\tnatural logarithm\n
    \tlog(x)\t\tcommon (base-10) logarithm\n
    \texp(x)\t\traise e (the base of the natural logarithm) to power\n
    \tsqr(x)\t\tsquare root (sqrt(x) is equivalent)\n
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
