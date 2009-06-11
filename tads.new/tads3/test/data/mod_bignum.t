#include "tads.h"
#include "t3.h"
#include "bignum.h"

preinit()
{
}

main(args)
{
    local x;

    x = 1.00000000000e8 / 3;
    tadsSay(x.spellNumber());
    "\b";

    "log2(32) = << (32.0000).log2() >>\n";
    "log2(55) = << (55.0000000).log2() >>\n";
}

modify BigNumber
    log2()
    {
        /* 
         *   cache ln(2) with slightly greater precision than our value -
         *   if we already have a precise enough value cached, there's no
         *   need to cache it again 
         */
        if (BigNumber.cacheLn2_ == nil
            || BigNumber.cacheLn2_.getPrecision() < getPrecision() + 3)
            BigNumber.cacheLn2_ = new BigNumber(2, getPrecision() + 3).logE();

        /* 
         *   calculate ln(self) to slightly greater than the required
         *   precision, to avoid rounding error, then divide by the cached
         *   value of ln(2) to yield the log base 2 of self 
         */
        return (self.setPrecision(getPrecision() + 3).logE()
                / BigNumber.cacheLn2_).setPrecision(getPrecision());
    }
    cacheLn2_ = nil
    
    spellNumber()
    {
        local str;
        local val = self;
        local units = ['one', 'two', 'three', 'four', 'five',
                       'six', 'seven', 'eight', 'nine', 'ten'];

        /* start out with an empty string */
        str = '';

        /* if the value is exactly zero, it's a special case */
        if (val == 0.0)
            return 'zero';

        /* 
         *   if it's negative, start the string with 'minus' and change
         *   the value we're working with to positive 
         */
        if (val < 0.0)
        {
            str = 'minus ';
            val = -val;
        }

        /* do the billions */
        if (val > 1.0e9)
        {
            /* add the billions */
            str += (val / 1.0e9).getWhole().spellNumber() + ' billion ';

            /* remove the billions portion */
            val -= (val / 1.0e9).getWhole() * 1.0e9;
        }

        /* do the millions */
        if (val > 1.0e6)
        {
            /* add the millions */
            str += (val / 1.0e6).getWhole().spellNumber() + ' million ';

            /* remove the millions portion */
            val -= (val / 1.0e6).getWhole() * 1.0e6;
        }

        /* do the thousands */
        if (val > 1.0e3)
        {
            /* add the number of thousands */
            str += (val / 1.0e3).getWhole().spellNumber() + ' thousand ';

            /* remove the thousands */
            val -= (val / 1.0e3).getWhole() * 1.0e3;
        }

        /* do the hundreds */
        if (val > 100.0)
        {
            /* add the number of hundreds */
            str += units[toInteger(val / 100.0)] + ' hundred ';

            /* remove the hundreds */
            val -= (val / 100.0).getWhole() * 100.0;
        }

        /* do the rest */
        if (val >= 20.0)
        {
            /* add the tens name */
            str += ['twenty', 'thirty', 'forty', 'fifty', 'sixty',
                    'seventy', 'eighty', 'ninety']
                   [toInteger(val / 10.0) - 1];

            /* remove the tens */
            val -= (val / 10.0).getWhole() * 10.0;

            /* if there's anything left, add it */
            if (val != 0)
                str += '-' + units[toInteger(val)];
        }
        else if (val >= 10.0)
        {
            /* add the 'teen' name */
            str += ['ten', 'eleven', 'twelve', 'thirteen', 'forteen',
                    'fifteen', 'sixteen', 'seventeen', 'eighteen',
                    'nineteen'][toInteger(val) - 9];
        }
        else if (val >= 0.0)
        {
            /* only the units remain */
            str += units[toInteger(val)];
        }
        else
        {
            /* 
             *   we're not adding anything more - we left a trailing space
             *   in case we were going to add something, so remove it 
             */
            str = str.substr(1, str.length() - 1);
        }

        /* return the result */
        return str;
    }
;

