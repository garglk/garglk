#include "tads.h"
#include "t3.h"
#include "bignum.h"

modify BigNumber
    spellNumber()
    {
        return inherited.spellNumber().toUpper();
    }
;

modify BigNumber
    spellNumber()
    {
        return inherited.spellNumber() + '!';
    }
;
