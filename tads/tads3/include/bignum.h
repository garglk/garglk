#charset "us-ascii"
#pragma once

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3.
 *   
 *   This header defines the BigNumber intrinsic class.  
 */

/* include our base class definition */
#include "systype.h"

/*
 *   The BigNumber intrinsic class lets you perform floating-point and
 *   integer arithmetic with (almost) any desired precision.  BigNumber uses
 *   a decimal representation, which means that decimal values can be
 *   represented exactly (i.e., with no rounding errors, as can happen with
 *   IEEE 'double' and 'float' values that languages like C typically
 *   support).  BigNumber combines a varying-length mantissa with an
 *   exponent; the length of the mantissa determines how many digits of
 *   precision a given BigNumber can store, and the exponent lets you
 *   represent very large or very small values with minimal storage.  You can
 *   specify the desired precision when you create a BigNumber explicitly;
 *   when BigNumber values are created implicitly by computations, the system
 *   chooses a precision based on the inputs to the calculations, typically
 *   equal to the largest of the precisions of the input values.
 *   
 *   The maximum precision for a BigNumber is about 64,000 digits, and the
 *   exponent can range from -32768 to +32767.  Since this is a decimal
 *   exponent, this implies an absolute value range from 1.0e-32768 to
 *   1.0e+32767.  The more digits of precision stored in a given BigNumber
 *   value, the more memory the object consumes, and the more time it takes
 *   to perform calculations using the value.  
 */
intrinsic class BigNumber 'bignumber/030001': Object
{
    /* format to a string */
    formatString(maxDigits?, flags?,
                 wholePlaces?, fracDigits?, expDigits?, leadFiller?);

    /* 
     *   compare for equality after rounding to the smaller of my
     *   precision and num's precision 
     */
    equalRound(num);

    /* 
     *   returns an integer giving the number of digits of precision that
     *   this number stores 
     */
    getPrecision();

    /* 
     *   Return a new number, with the same value as this number but with
     *   the given number of decimal digits of precision.  If the new
     *   precision is higher than the old precision, this will increase
     *   the precision to the requested new size and add trailing zeros
     *   to the value.  If the new precision is lower than the old
     *   precision, we'll round the number for the reduced precision.  
     */
    setPrecision(digits);

    /* get the fractional part */
    getFraction();

    /* get the whole part (truncates the fraction - doesn't round) */
    getWhole();

    /* 
     *   round to the given number of digits after the decimal point; if
     *   the value is zero, round to integer; if the value is negative,
     *   round to the given number of places before the decimal point 
     */
    roundToDecimal(places);

    /* return the absolute value */
    getAbs();

    /* least integer greater than or equal to this number */
    getCeil();

    /* greatest integer less than or equal to this number */
    getFloor();

    /* get the base-10 scale of the number */
    getScale();

    /* 
     *   scale by 10^x - if x is positive, this multiplies the number by
     *   ten the given number of times; if x is negative, this divides the
     *   number by ten the given number of times 
     */
    scaleTen(x);

    /* negate - invert the sign of the number */
    negate();

    /* 
     *   copySignFrom - combine the absolute value of self with the sign
     *   of x 
     */
    copySignFrom(x);

    /* determine if the value is negative */
    isNegative();

    /* 
     *   Calculate the integer quotient and the remainder; returns a list
     *   whose first element is the integer quotient (a BigNumber
     *   containing an integer value), and whose second element is the
     *   remainder (the value R such that dividend = quotient*x + R).
     *   
     *   Note that the quotient returned will not necessarily have the
     *   same value as the whole part of dividing self by x with the '/'
     *   operator, because this division handles rounding differently.  In
     *   particular, the '/' operator will perform the appropriate
     *   rounding on the quotient if the quotient has insufficient
     *   precision to represent the exact result.  This routine, in
     *   contrast, does NOT round the quotient, but merely truncates any
     *   trailing digits that cannot be represented in the result's
     *   precision.  The reason for this difference is that it ensures
     *   that the relation (dividend=quotient*x+remainder) holds, which
     *   would not always be the case if the quotient were rounded up.
     *   
     *   Note also that the remainder will not necessarily be less than
     *   the divisor.  If the quotient cannot be exactly represented
     *   (which occurs if the precision of the quotient is smaller than
     *   its scale), the remainder will be the correct value so that the
     *   relationship above holds, rather than the unique remainder that
     *   is smaller than the divisor.  In all cases where there is
     *   sufficient precision to represent the quotient exactly (to the
     *   units digit only, since the quotient returned from this method
     *   will always be an integer), the remainder will satisfy the
     *   relationship AND will be the unique remainder with absolute value
     *   less than the divisor.  
     */
    divideBy(x);

    /* 
     *   calculate and return the trigonometric sine of the value (taken
     *   as a radian value) 
     */
    sine();

    /* 
     *   calculate and return the trigonometric cosine of the value (taken
     *   as a radian value) 
     */
    cosine();

    /* 
     *   calculate and return the trigonometric tangent of the value
     *   (taken as a radian value) 
     */
    tangent();

    /* 
     *   interpreting this number as a number of degrees, convert the
     *   value to radians and return the result 
     */
    degreesToRadians();

    /* 
     *   interpreting this number as a number of radians, convert the
     *   value to degrees and return the result 
     */
    radiansToDegrees();

    /* 
     *   Calculate and return the arcsine (in radians) of the value.  Note
     *   that the value must be between -1 and +1 inclusive, since sine()
     *   never has a value outside of this range. 
     */
    arcsine();

    /* 
     *   Calculate and return the arccosine (in radians).  The value must
     *   be between -1 and +1 inclusive. 
     */
    arccosine();

    /* calculate and return the arctangent (in radians) */
    arctangent();

    /* calculate the square root and return the result */
    sqrt();

    /* 
     *   calculate the natural logarithm of this number and return the
     *   result 
     */
    logE();

    /* 
     *   raise e (the base of the natural logarithm) to the power of this
     *   value and return the result 
     */
    expE();

    /* calculate the base-10 logarithm of the number and return the result */
    log10();

    /* 
     *   raise this number to the power of the argument and return the
     *   result 
     */
    raiseToPower(x);

    /* calculate the hyperbolic sine, cosine, and tangent */
    sinh();
    cosh();
    tanh();

    /* class method: get the value of pi to a given precision */
    getPi(digits);

    /* class method: get the value of e to a given precision */
    getE(digits);

    /*
     *   Get the type of this number.  This returns a combination of
     *   NumTypeXxx flags, combined with the '|' operator.  This can be used
     *   to check for special values, such as infinites and "not a number"
     *   values.  
     */
    numType();
}

/* ------------------------------------------------------------------------ */
/*
 *   flags for formatString 
 */

/* always show a sign, even if positive */
#define BignumSign          0x0001

/* always show in exponential format (scientific notation, as in "1.0e20") */
#define BignumExp           0x0002

/* always show a sign in the exponent, even if positive */
#define BignumExpSign      0x0004

/* 
 *   show a zero before the decimal point - this is only relevant in
 *   non-exponential format when the number is between -1 and +1 
 */
#define BignumLeadingZero  0x0008

/* always show a decimal point */
#define BignumPoint         0x0010

/* insert commas to denote thousands, millions, etc */
#define BignumCommas        0x0020

/* show a leading space if the number is positive */
#define BignumPosSpace     0x0040

/* 
 *   use European-style formatting: use a comma instead of a period for
 *   the decimal point, and use periods instead of commas to set off
 *   thousands, millions, etc 
 */
#define BignumEuroStyle     0x0080

/* 
 *   "Compact" format: use the shorter of the regular format and scientific
 *   notation.  If the scientific notation exponent is less than -4 or
 *   greater than or equal to the number of digits after the decimal point,
 *   we'll use scientific notation; otherwise we'll use the plain format. 
 */
#define BignumCompact      0x0100

/* 
 *   maxDigits counts only significant digits; leading zeros aren't counted
 *   against the maximum. 
 */
#define BignumMaxSigDigits 0x0200

/* 
 *   Keep trailing zeros.  If there's a maxDigits value, this keeps enough
 *   trailing zeros so that the number of digits shown equals maxDigits.  By
 *   default, trailing zeros after the decimal point are removed. 
 */
#define BignumKeepTrailingZeros 0x0400


/* ------------------------------------------------------------------------ */
/*
 *   Number type flags, for numType() 
 */

/*
 *   Number type: ordinary number. 
 */
#define NumTypeNum  0x0001

/* 
 *   Number type: "Not a number" (NaN).  This indicates that the value is the
 *   result of a calculation with invalid input(s).  Currently there are no
 *   BigNumber calculations that return NaNs, as all functions on invalid
 *   inputs throw errors instead.  But it's possible to construct NaN value,
 *   such as by reading an IEEE 754-2008 NaN value from a file via
 *   unpackBytes().  
 */
#define NumTypeNAN  0x0002

/* 
 *   Number type: positive infinity, negative infinity.  These indicate a
 *   value that overflowed the capacity of the BigNumber type, or a
 *   calculation that yields infinity (e.g., tan(pi/2)).  Currently there are
 *   no BigNumber calculations that return Infinities, as all functions where
 *   an overflow is possible throw errors instead.  But it's possible to
 *   construct an Infinity value, such as by reading an IEEE 754-2008
 *   Infinity value from a file via unpackBytes().  
 */
#define NumTypePInf 0x0004
#define NumTypeNInf 0x0008
#define NumTypeInf  (NumTypePInf | NumTypeNInf)

/*
 *   Number type: zero, positive or negative.  Mathematically, zero is
 *   neither positive nor negative, but the BigNumber type retains a sign for
 *   all values, even zeros.  Negative zeros can come from calculations
 *   that yield negative results with absolute values too small for the
 *   internal representation.  It's also possible to construct a negative
 *   zero, such as by reading an IEEE 754-2008 negative zero value from a
 *   file via unpackBytes().  
 */
#define NumTypePZero  0x0010
#define NumTypeNZero  0x0020
#define NumTypeZero (NumTypePZero | NumTypeNZero)


