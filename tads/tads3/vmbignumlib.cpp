#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2012 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmbignumlib.cpp - BigNumber numerics library
Function
  This module contains the low-level numerics library for the BigNumber
  object.  The routines here operate on the internal byte-array
  representation of a BigNumber value, without the CVmObjBigNum wrapper
  object, so this module can be used as a library for high-precision
  arithmetic without dragging in the whole TADS 3 VM.
Notes
  
Modified
  06/20/12 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>

#include "t3std.h"
#include "utf8.h"
#include "vmtype.h"
#include "vmbignum.h"


/* ------------------------------------------------------------------------ */
/*
 *   convert to a string, storing the result in the given buffer 
 */
const char *CVmObjBigNum::cvt_to_string_buf(
    char *buf, size_t buflen,
    int max_digits, int whole_places, int frac_digits, int exp_digits,
    ulong flags)
{
    /* convert to a string into our buffer */
    CBigNumStringBufFixed alo(buf, buflen);
    return cvt_to_string_gen(&alo, ext_, max_digits, whole_places,
                             frac_digits, exp_digits, flags, 0, 0);
}

/*
 *   Convert to a string, storign the result in the given buffer
 */
const char *CVmObjBigNum::cvt_to_string_buf(
    char *buf, size_t buflen, const char *ext,
    int max_digits, int whole_places, int frac_digits, int exp_digits,
    ulong flags, const char *lead_fill, size_t lead_fill_len)
{
    /* convert to a string into our buffer */
    CBigNumStringBufFixed alo(buf, buflen);
    return cvt_to_string_gen(&alo, ext, max_digits, whole_places,
                             frac_digits, exp_digits, flags,
                             lead_fill, lead_fill_len);
}

/*
 *   Convert an integer value to a string, storing the result in the buffer.
 *   This can be used to apply the whole set of floating-point formatting
 *   options to an ordinary integer value, without going to the trouble of
 *   creating a BigNumber object. 
 */
const char *CVmObjBigNum::cvt_int_to_string_buf(
    char *buf, size_t buflen, int32_t intval,
    int max_digits, int whole_places, int frac_digits, int exp_digits,
    ulong flags)
{
    /* 
     *   Set up a stack register with the number.  Allow 20 digits of
     *   precision so that this can extend to 64-bit ints in the future.  
     */
    char tmp[5 + 20] = { 20, 0, 0, 0, 0 };
    set_int_val(tmp, intval);

    /* 
     *   set the actual precision to equal the exponent, since zeros after
     *   the decimal point aren't significant for an integer source
     */
    set_prec(tmp, get_exp(tmp));
    
    /* convert to a string into our buffer */
    CBigNumStringBufFixed alo(buf, buflen);
    return cvt_to_string_gen(&alo, tmp, max_digits, whole_places,
                             frac_digits, exp_digits, flags, 0, 0);
}


/*
 *   Common routine to create a string representation.  If buf is null,
 *   we'll allocate a new string object, filling in new_str with the
 *   object reference; otherwise, we'll format into the given buffer.  
 */
const char *CVmObjBigNum::cvt_to_string_gen(
    IBigNumStringBuf *bufalo, const char *ext,
    int max_digits, int whole_places, int frac_digits, int exp_digits,
    ulong flags, const char *lead_fill, size_t lead_fill_len)
{
    char plus_sign = '\0';
    int always_sign_exp = ((flags & VMBN_FORMAT_EXP_SIGN) != 0);
    int always_exp = ((flags & VMBN_FORMAT_EXP) != 0);
    int lead_zero = ((flags & VMBN_FORMAT_LEADING_ZERO) != 0);
    int always_show_pt = ((flags & VMBN_FORMAT_POINT) != 0);
    int exp_caps = ((flags & VMBN_FORMAT_EXP_CAP) != 0);
    int pos_lead_space = ((flags & VMBN_FORMAT_POS_SPACE) != 0);
    int commas = ((flags & VMBN_FORMAT_COMMAS) != 0);
    int euro = ((flags & VMBN_FORMAT_EUROSTYLE) != 0);
    size_t req_chars;
    int prec = (int)get_prec(ext);
    int exp = get_exp(ext);
    int dig_before_pt;
    int dig_after_pt;
    int idx;
    unsigned int dig;
    char *p;
    int exp_carry;
    int show_pt;
    int whole_padding;
    int non_sci_digs;
    char decpt_char = (euro ? ',' : '.');
    char comma_char = (euro ? '.' : ',');
    char *tmp_ext = 0;
    uint tmp_hdl;
    int max_sig = ((flags & VMBN_FORMAT_MAXSIG) != 0);
    int all_out_cnt, sig_out_cnt;

    /* we haven't written out any digits yet */
    all_out_cnt = sig_out_cnt = 0;

    /* note the sign character to use for positive numbers, if any */
    if (pos_lead_space)
        plus_sign = ' ';
    else if ((flags & VMBN_FORMAT_SIGN) != 0)
        plus_sign = '+';

    /* 
     *   If we're not required to use exponential notation, but we don't
     *   have enough max_digits places for the part before the decimal
     *   point, use exponential anyway.  (The number of digits before the
     *   decimal point is simply the exponent if it's greater than zero,
     *   or zero otherwise.)  
     */
    if (max_digits >= 0 && exp > max_digits)
        always_exp = TRUE;
        

    /* 
     *   If we're not required to use exponential notation, but our
     *   absolute value is so small that we wouldn't show anything
     *   "0.00000..." (i.e., we'd have too many zeros after the decimal
     *   point to show any actual digits of our number), use exponential
     *   notation.  If our exponent is negative, its absolute value is the
     *   number of zeros we'd show after the decimal point before the
     *   first non-zero digit.  
     */
    if (max_digits >= 0
        && exp < 0
        && (-exp > max_digits
            || (frac_digits != -1 && -exp > frac_digits)))
        always_exp = TRUE;

    /*
     *   If we're the "compact" mode flag is set, use scientific notation if
     *   the displayed decimal exponent is less than -4 or greater than the
     *   number of digits after the decimal point.  (The displayed exponent
     *   is one lower than the internal exponent, since the normalized format
     *   has no digits before the decimal point but the displayed scientific
     *   notation format shows one whole-part digit.)  
     */
    if ((flags & VMBN_FORMAT_COMPACT) != 0
        && (exp < -3 || (max_digits >= 0 && exp - 1 > max_digits)))
        always_exp = TRUE;

    /* calculate how many digits we'd need in non-scientific notation */
    if (exp < 0)
    {
        /* we have leading zeros before the first significant digit */
        non_sci_digs = -exp + prec;
    }
    else if (exp > prec)
    {
        /* we have trailing zeros after the last significant digit */
        non_sci_digs = exp + prec;
    }
    else
    {
        /* 
         *   we have no leading or trailing zeros to represent - only the
         *   digits actually stored need to be displayed 
         */
        non_sci_digs = prec;
    }

    /* 
     *   Figure out how much space we need for our string.  Start with the
     *   length of the basic digit string, which is the smaller of max_digits
     *   or the actual space we need for non-scientific notation.  
     */
    if (max_digits >= 0 && max_digits < non_sci_digs)
    {
        /* max_digits limits the number of characters we can show */
        req_chars = max_digits;

        /* 
         *.  ..but if it only counts significant digits, and the exponent
         *   is negative, we might also have to show all those leading zeros
         *   over and above the max_digits limit 
         */
        if (max_sig && exp < 0)
            req_chars += -exp;
    }
    else
    {
        /* there's no cap on digits, so we could have to show them all */
        req_chars = non_sci_digs;
    }

    /* 
     *   Now add overhead space for the sign symbol, a leading zero, a
     *   decimal point, an 'E' for the exponent, an exponent sign, and up to
     *   five digits for the exponent (16-bit integer -> -32768 to 32767).
     *   Also allow an extra character in case we need to add a digit due to
     *   rounding.  
     */
    req_chars += 11;

    /*
     *   Make sure we leave enough room for the lead fill string - if they
     *   specified a number of whole places, and we're not using
     *   exponential format, we'll insert lead fill characters before the
     *   first non-zero whole digit. 
     */
    if (!always_exp && whole_places != -1 && exp < whole_places)
    {
        int extra;
        int char_size;
        
        /* 
         *   if the exponent is negative, we'll pad by the full amount;
         *   otherwise, we'll just pad by the difference between the
         *   number of places needed and the exponent 
         */
        extra = whole_places;
        if (exp > 0)
            extra -= exp;

        /* 
         *   Add the extra bytes: one byte per character if we're using
         *   the default space padding, or up to three bytes per character
         *   if a lead string was specified (each unicode character can
         *   take up to three bytes) 
         */
        char_size = (lead_fill != 0 ? 3 : 1);
        req_chars += extra * char_size;

        /* 
         *   add space for each padding character we could insert into a
         *   comma position (there's at most one comma per three fill
         *   characters) 
         */
        if (commas)
            req_chars += ((extra + 2)/3) * char_size;
    }

    /* 
     *   If we're using commas, and we're not using scientific notation,
     *   add space for a comma for each three digits before the decimal
     *   point 
     */
    if (commas && !always_exp)
    {
        /* add space for the commas */
        req_chars += ((exp + 2) / 3);
    }

    /* 
     *   if they requested a specific minimum number of exponent digits,
     *   and that number is greater than the allowance of 5 we already
     *   provided, add the extra space needed 
     */
    if (exp_digits > 5)
        req_chars += (exp_digits - 5);

    /*
     *   If they requested a specific number of digits after the decimal
     *   point, make sure we have room for those digits.
     */
    if (frac_digits != -1)
        req_chars += frac_digits;

    /* now that we know how much space we need, get the output buffer */
    char *buf = bufalo->get_buf(req_chars + VMB_LEN);

    /* if that failed, return failure */
    if (buf == 0)
        return 0;

    /* check for special values */
    switch(get_type(ext))
    {
    case VMBN_T_NUM:
        /* normal number - proceed */
        break;

    case VMBN_T_NAN:
        /* not a number - show "1.#NAN" */
        strcpy(buf + VMB_LEN, "1.#NAN");
        vmb_put_len(buf, 6);
        return buf;

    case VMBN_T_INF:
        /* positive or negative infinity */
        if (get_neg(ext))
        {
            memcpy(buf + VMB_LEN, "-1.#INF", 7);
            vmb_put_len(buf, 7);
        }
        else
        {
            memcpy(buf + VMB_LEN, "1.#INF", 6);
            vmb_put_len(buf, 6);
        }
        return buf;

    default:
        /* other values are not valid */
        memcpy(buf + VMB_LEN, "1.#???", 6);
        vmb_put_len(buf, 6);
        return buf;
    }

    /*
     *   Allocate a temporary buffer to contain a copy of the number.  At
     *   most, we'll have to show max_digits of the number, or the current
     *   precision, whichever is lower.  
     */
    {
        int new_prec;

        /* 
         *   limit the new precision to the maximum digits to be shown, or
         *   our existing precision, whichever is lower 
         */
        if (max_digits < 0)
            new_prec = prec;
        else
        {
            new_prec = max_digits;
            if (prec < new_prec)
                new_prec = prec;
        }

        /* allocate the space */
        alloc_temp_regs((size_t)new_prec, 1, &tmp_ext, &tmp_hdl);

        /* copy the value to the temp register, rounding the value */
        copy_val(tmp_ext, ext, TRUE);

        /* note the new precision */
        prec = new_prec;

        /* forget the original number and use the rounded version */
        ext = tmp_ext;
    }

start_over:
    /* 
     *   note the exponent, in case we rounded or otherwise adjusted the
     *   temporary number 
     */
    exp = get_exp(ext);
    
    /* presume we won't carry into the exponent */
    exp_carry = FALSE;

    /* no whole-part padding yet */
    whole_padding = 0;
    
    /* 
     *   Figure out where the decimal point goes in the display.  If we're
     *   displaying in exponential format, we'll always display exactly
     *   one digit before the decimal point.  Otherwise, we'll display the
     *   number given by our exponent if it's positive, or zero or one
     *   (depending on lead_zero) if it's negative or zero.  
     */
    if (always_exp)
        dig_before_pt = 1;
    else if (exp > 0)
        dig_before_pt = exp;
    else
        dig_before_pt = 0;

    /* 
     *   if the digits before the decimal point exceed our maximum number
     *   of digits allowed, we'll need to use exponential format
     */
    if (max_digits >= 0 && dig_before_pt > max_digits)
    {
        always_exp = TRUE;
        goto start_over;
    }

    /* 
     *   Limit digits after the decimal point according to the maximum digits
     *   allowed and the number we'll show before the decimal point.  If we
     *   have an explicit number of fractional digits to show, that overrides
     *   the limit.  
     */
    if (max_digits >= 0)
        dig_after_pt = max_digits - dig_before_pt;
    else if (frac_digits >= 0)
        dig_after_pt = frac_digits;
    else if (prec > dig_before_pt)
        dig_after_pt = prec - dig_before_pt;
    else
        dig_after_pt = 0;

    /* start writing after the buffer length prefix */
    p = buf + VMB_LEN;

    /* 
     *   if we're not in exponential mode, add leading spaces for the
     *   unused whole places 
     */
    if (!always_exp && dig_before_pt < whole_places)
    {
        int cnt;
        size_t src_rem;
        utf8_ptr src;
        utf8_ptr dst;
        int idx;

        /* start with the excess whole places */
        cnt = whole_places - dig_before_pt;

        /* if we're going to add a leading zero, that's one less space */
        if (dig_before_pt == 0 && lead_zero)
            --cnt;

        /*
         *   Increase the count by the number of comma positions in the
         *   padding area.  
         */
        if (commas)
        {
            /* scan over the positions to fill and count commas */
            for (idx = dig_before_pt ; idx < whole_places ; ++idx)
            {
                /* if this is a comma position, note it */
                if ((idx % 3) == 0)
                    ++cnt;
            }
        }

        /* set up our read and write pointers */
        src.set((char *)lead_fill);
        src_rem = lead_fill_len;
        dst.set(p);

        /* add (and count) the leading spaces */
        for ( ; cnt != 0 ; --cnt, ++whole_padding)
        {
            wchar_t ch;
            
            /* 
             *   if we have a lead fill string, read from it; otherwise,
             *   just use a space 
             */
            if (lead_fill != 0)
            {
                /* if we've exhausted the string, start over */
                if (src_rem == 0)
                {
                    src.set((char *)lead_fill);
                    src_rem = lead_fill_len;
                }

                /* get the next character */
                ch = src.getch();

                /* skip this character */
                src.inc(&src_rem);
            }
            else
            {
                /* no lead fill string - insert a space by default */
                ch = ' ';
            }

            /* write this character */
            dst.setch(ch);
        }

        /* resynchronize from our utf8 pointer */
        p = dst.getptr();
    }

    /* 
     *   if the number is negative, or we're always showing a sign, add
     *   the sign; if we're not adding a sign, but we're showing a leading
     *   space for positive numbers, add the leading space 
     */
    if (get_neg(ext))
        *p++ = '-';
    else if (plus_sign != '\0')
        *p++ = plus_sign;

    /* 
     *   if we have no digits before the decimal, and we're adding a
     *   leading zero, add it now 
     */
    if (dig_before_pt == 0 && lead_zero)
    {
        /* add the leading zero */
        *p++ = '0';

        /* 
         *   If we have a total digit limit, reduce the limit on the digits
         *   after the decimal point, since this leading zero comes out of
         *   our overall digit budget.  This doesn't apply if max_digits is
         *   the number of significant digits.  
         */
        if (max_digits >= 0 && !max_sig)
            --dig_after_pt;

        /* count it in the output counter for all digits */
        all_out_cnt++;
    }

    /*
     *   If we have limited the number of digits that we'll allow after the
     *   decimal point, due to the limit on the total number of digits and
     *   the number of digits we need to display before the decimal, start
     *   over in exponential format to ensure we get the after-decimal
     *   display we want.
     *   
     *   Note that we won't bother going into exponential format if the
     *   number of digits before the decimal point is zero or one, because
     *   exponential format won't give us any more room - in such cases we
     *   simply have an impossible request.  
     */
    if (max_digits >=0 && !always_exp
        && frac_digits != -1
        && dig_after_pt < frac_digits
        && dig_before_pt > 1)
    {
        /* switch to exponential format and start over */
        always_exp = TRUE;
        goto start_over;
    }

    /* display the digits before the decimal point */
    for (idx = 0 ; idx < dig_before_pt && idx < prec ; ++idx)
    {
        /* 
         *   if this isn't the first digit, and we're adding commas, and
         *   an even multiple of three more digits follow, insert a comma 
         */
        if (idx != 0 && commas && !always_exp
            && ((dig_before_pt - idx) % 3) == 0)
            *p++ = comma_char;
        
        /* get this digit */
        dig = get_dig(ext, idx);

        /* add it to the string */
        *p++ = (dig + '0');

        /* count it */
        all_out_cnt++;
        if (dig != 0 || sig_out_cnt != 0)
            sig_out_cnt++;
    }

    /* if we ran out of precision, add zeros */
    for ( ; idx < dig_before_pt ; ++idx)
    {
        /* add the zero */
        *p++ = '0';

        /* count it */
        all_out_cnt++;
        if (sig_out_cnt != 0)
            sig_out_cnt++;
    }

    /* 
     *   Add the decimal point.  Show the decimal point unless
     *   always_show_pt is false, and either frac_digits is zero, or
     *   frac_digits is -1 and we have no fractional part. 
     */
    if (always_show_pt)
    {
        /* always showing the point */
        show_pt = TRUE;
    }
    else
    {
        if (frac_digits > 0)
        {
            /* we're showing fractional digits - always show a point */
            show_pt = TRUE;
        }
        else if (frac_digits == 0)
        {
            /* we're showing no fractional digits, so suppress the point */
            show_pt = FALSE;
        }
        else
        {
            /* 
             *   for now assume we'll show the point; we'll take it back
             *   out if we don't encounter anything but zeros 
             */
            show_pt = TRUE;
        }
    }

    /* if we're showing the fractional part, show it */
    if (show_pt)
    {
        int frac_len;
        int frac_lim;
        char *last_non_zero;

        /* 
         *   remember the current position as the last trailing non-zero -
         *   if we don't find anything but zeros and decide to remove the
         *   trailing zeros, we'll also remove the decimal point by
         *   coming back here 
         */
        last_non_zero = p;
        
        /* add the point */
        *p++ = decpt_char;

        /* if we're always showing a decimal point, we can't remove it */
        if (always_show_pt)
            last_non_zero = p;

        /* if frac_digits is -1, there's no limit */
        if (frac_digits == -1)
            frac_lim = dig_after_pt;
        else
            frac_lim = frac_digits;

        /* 
         *   further limit the fractional digits according to the maximum
         *   digit allowance 
         */
        if (frac_lim > dig_after_pt)
            frac_lim = dig_after_pt;

        /* no fractional digits output yet */
        frac_len = 0;

        /* 
         *   if we haven't yet reached the first non-zero digit, display
         *   as many zeros as necessary 
         */
        if (idx == 0 && exp < 0)
        {
            int cnt;

            /* 
             *   display leading zeros based no the exponent: if exp is
             *   zero, we don't need any; if exp is -1, we need one; if
             *   exp is -2, we need two, and so on 
             */
            for (cnt = exp ; cnt != 0 && frac_len < frac_lim ; ++cnt)
            {
                /* add a zero */
                *p++ = '0';

                /* 
                 *   if we're counting all digits (not just significant
                 *   digits), count this digit against the total 
                 */
                if (!max_sig)
                    ++frac_len;

                /* count it in the total digits out */
                all_out_cnt++;
            }
        }

        /* add the fractional digits */
        for ( ; idx < prec && frac_len < frac_lim ; ++idx, ++frac_len)
        {
            /* get this digit */
            dig = get_dig(ext, idx);

            /* add it */
            *p++ = (dig + '0');

            /* 
             *   if it's not a zero, note the location - if we decide to
             *   trim trailing zeros, we'll want to keep at least this
             *   much, since this is a significant trailing digit 
             */
            if (dig != 0)
                last_non_zero = p;

            /* count it */
            all_out_cnt++;
            if (dig != 0 || sig_out_cnt != 0)
                sig_out_cnt++;
        }

        /*
         *   If the "keep trailing zeros" flag is set, and there's a
         *   max_digits quota that we haven't filled, add trailing zeros as
         *   needed. 
         */
        if ((flags & VMBN_FORMAT_TRAILING_ZEROS) != 0 && max_digits >= 0)
        {
            /* 
             *   fill out the request: if max_digits is stated in significant
             *   digits, count the number of significant digits we've written
             *   so far, otherwise count all digits written so far 
             */
            for (int i = max_sig ? sig_out_cnt : all_out_cnt ;
                 i < max_digits ; ++i)
            {
                /* add a digit */
                *p++ = '0';

                /* count it */
                all_out_cnt++;
                if (sig_out_cnt != 0)
                    sig_out_cnt++;
            }
        }

        /* 
         *   add the trailing zeros if we ran out of precision before we
         *   filled the requested number of places 
         */
        if (frac_digits != -1)
        {
            /* fill out the remaining request length with zeros */
            for ( ; frac_len < frac_lim ; ++frac_len)
            {
                /* add a zero */
                *p++ = '0';

                /* count it */
                all_out_cnt++;
                if (sig_out_cnt != 0)
                    sig_out_cnt++;
            }
        }
        else if (!(flags & VMBN_FORMAT_TRAILING_ZEROS))
        {
            char *p1;
            
            /* 
             *   if we're removing the decimal point, we're not showing a
             *   fractional part after all - so note 
             */
            if (last_non_zero < p && *last_non_zero == decpt_char)
                show_pt = FALSE;
            
            /* 
             *   We can use whatever length we like, so remove meaningless
             *   trailing zeros.  Before we do this, though, make sure we
             *   aren't rounding up the last trailing zero - if we need to
             *   round the last digit up, the final zero is really a 1.  
             */
            if (p > last_non_zero && get_round_dir(ext, idx))
            {
                /* 
                 *   That last zero is significant after all, since we're
                 *   going to round it up to a 1 for display.  Don't actually
                 *   do the rounding now; simply note that the last zero is
                 *   significant so that we don't drop the digits leading up
                 *   to it. 
                 */
                last_non_zero = p;
            }

            /*   
             *   We've checked for rounding in the last digit, so we can now
             *   safely remove meaningless trailing zeros.  If this leaves a
             *   completely empty buffer, not counting a sign and/or a
             *   decimal point, it means that we have a fractional number
             *   that we're showing without an exponent, and the number of
             *   digits we had for display was insufficient to reach the
             *   first non-zero fractional digit.  In this case, simply show
             *   '0' (or '0.', if a decimal point is required) as the result.
             *   To find out, scan for digits.  
             */
            p = last_non_zero;
            for (p1 = buf + VMB_LEN ; p1 < p && !is_digit(*p1) ; ++p1) ;

            /* if we didn't find any digits, add/insert a '0' */
            if (p1 == p)
            {
                /* 
                 *   if there's a decimal point, insert the '0' before it;
                 *   otherwise, just append the zero 
                 */
                if (p > buf + VMB_LEN && *(p-1) == decpt_char)
                {
                    *(p-1) = '0';
                    *p++ = decpt_char;
                }
                else
                    *p++ = '0';
            }
        }
    }

    /* if necessary, round up at the last digit */
    if (get_round_dir(ext, idx))
    {
        char *rp;
        int need_carry;
        int found_pt;
        int dig_cnt;
        
        /* 
         *   go back through the number and add one to the last digit,
         *   carrying as needed 
         */
        for (dig_cnt = 0, found_pt = FALSE, need_carry = TRUE, rp = p - 1 ;
             rp >= buf + VMB_LEN ; rp = utf8_ptr::s_dec(rp))
        {
            /* if this is a digit, bump it up */
            if (is_digit(*rp))
            {
                /* count it */
                ++dig_cnt;
                
                /* if it's 9, we'll have to carry; otherwise it's easy */
                if (*rp == '9')
                {
                    /* set it to zero and keep going to do the carry */
                    *rp = '0';

                    /* 
                     *   if we haven't found the decimal point yet, and
                     *   we're not required to show a certain number of
                     *   fractional digits, we can simply remove this
                     *   trailing zero 
                     */
                    if (show_pt && !found_pt && frac_digits == -1)
                    {
                        /* remove the trailing zero */
                        p = utf8_ptr::s_dec(p);

                        /* remove it from the digit count */
                        --dig_cnt;
                    }
                }
                else
                {
                    /* bump it up one */
                    ++(*rp);

                    /* no more carrying is needed */
                    need_carry = FALSE;

                    /* we don't need to look any further */
                    break;
                }
            }
            else if (*rp == decpt_char)
            {
                /* note that we found the decimal point */
                found_pt = TRUE;
            }
        }

        /* 
         *   If we got to the start of the number and we still need a
         *   carry, we must have had all 9's.  In this case, we need to
         *   redo the entire number, since all of the layout (commas,
         *   leading spaces, etc) can change - it's way too much work to
         *   try to back-patch all of this stuff, so we'll just bump the
         *   number up and reformat it from scratch.  
         */
        if (need_carry)
        {
            /* truncate at this digit, and round up what we're keeping */
            set_dig(tmp_ext, idx, 0);
            round_up_abs(tmp_ext, idx);

            /* 
             *   if this pushes us over the maximum digit range, switch to
             *   scientific notation 
             */
            if (max_digits >= 0 && dig_cnt + 1 > max_digits)
                always_exp = TRUE;

            /* go back and start over */
            goto start_over;
        }
    }

    /* add the exponent */
    if (always_exp)
    {
        int disp_exp;
        
        /* add the 'E' */
        *p++ = (exp_caps ? 'E' : 'e');

        /* 
         *   calculate the display exponent - it's one less than the
         *   actual exponent, because we put the point after one digit 
         */
        disp_exp = exp - 1;

        /* 
         *   if we carried into the exponent due to rounding, increase the
         *   exponent by one 
         */
        if (exp_carry)
            ++disp_exp;

        /* add the sign */
        if (disp_exp < 0)
        {
            *p++ = '-';
            disp_exp = -disp_exp;
        }
        else if (always_sign_exp)
            *p++ = '+';

        /* 
         *   if the exponent is zero, just put zero (unless a more
         *   specific format was requested) 
         */
        if (disp_exp == 0 && exp_digits == -1)
        {
            /* add the zero exponent */
            *p++ = '0';
        }
        else
        {
            char expbuf[20];
            char *ep;
            int dig_cnt;

            /* build the exponent in reverse */
            for (dig_cnt = 0, ep = expbuf + sizeof(expbuf) ; disp_exp != 0 ;
                 disp_exp /= 10, ++dig_cnt)
            {
                /* move back one character */
                --ep;
                
                /* add one digit */
                *ep = (disp_exp % 10) + '0';
            }

            /* if necessary, add leading zeros to the exponent */
            if (exp_digits != -1 && exp_digits > dig_cnt)
            {
                for ( ; dig_cnt < exp_digits ; ++dig_cnt)
                    *p++ = '0';
            }

            /* copy the exponent into the output */
            for ( ; ep < expbuf + sizeof(expbuf) ; ++ep)
                *p++ = *ep;
        }
    }

    /* if we allocated a temporary register, free it */
    if (tmp_ext != 0)
        release_temp_regs(1, tmp_hdl);

    /* set the string length and return the buffer */
    vmb_put_len(buf, p - buf - VMB_LEN);
    return buf;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a string into an allocated buffer.
 */
char *CVmObjBigNum::parse_str_alo(size_t &buflen,
                                  const char *str, size_t len, int radix)
{
    /* figure the precision */
    int prec = precision_from_string(str, len, radix);

    /* allocate a buffer */
    char *ext = new char[buflen = calc_alloc(prec)];
    set_prec(ext, prec);

    /* parse the number */
    parse_str_into(ext, str, len, radix);

    /* return the new buffer */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a string with the given radix. 
 */
void CVmObjBigNum::parse_str_into(char *ext, const char *str, size_t len,
                                  int radix)
{
    /* if the radix is 0, infer the radix from the string */
    int inferred_radix = 0;
    if (radix == 0)
        radix = inferred_radix = radix_from_string(str, len);
    
    /* if it's decimal, parse as a floating point value */
    if (radix == 10)
    {
        parse_str_into(ext, str, len);
        return;
    }

    /* get the precision */
    size_t prec = get_prec(ext);

    /* set the initial value to zero */
    set_type(ext, VMBN_T_NUM);
    memset(ext + VMBN_MANT, 0, (prec + 1)/2);

    /* set up to scan the string */
    utf8_ptr p((char *)str);
    size_t rem = len;

    /* skip leading spaces */
    for ( ; rem != 0 && is_space(p.getch()) ; p.inc(&rem)) ;

    /* check for a sign */
    int neg = FALSE;
    if (rem != 0 && p.getch() == '+')
    {
        /* skip the sign */
        p.inc(&rem);
    }
    else if (rem != 0 && p.getch() == '-')
    {
        /* note the sign and skip it */
        neg = TRUE;
        p.inc(&rem);
    }

    /* skip spaces after the sign */
    for ( ; rem != 0 && is_space(p.getch()) ; p.inc(&rem)) ;

    /* if we inferred the radix as hax, skip any '0x' marker */
    if (inferred_radix == 16
        && rem != 0
        && p.getch() == '0'
        && (p.getch_at(1) == 'x' || p.getch_at(1) == 'X'))
    {
        /* skip the '0x' prefix */
        p.inc(&rem);
        p.inc(&rem);
    }

    /* allocate a temporary register for swapping with the accumulator */
    uint thdl;
    char *tmp = alloc_temp_reg(prec, &thdl);
    char *t1 = ext, *t2 = tmp;

    /* parse the digits */
    int significant = FALSE;
    for ( ; rem != 0 ; p.inc(&rem))
    {
        /* get the next character */
        wchar_t ch = p.getch();

        /* get the digit value of this character */
        int d;
        if (ch >= '0' && ch <= '9')
            d = ch - '0';
        else if (ch >= 'A' && ch <= 'Z')
            d = ch - 'A' + 10;
        else if (ch >= 'a' && ch <= 'z')
            d = ch - 'a' + 10;
        else
            break;

        /* if the digit isn't within the radix, it's not a digit after all */
        if (d >= radix)
            break;

        /* if it's not a leading zero, it's significant */
        if (ch != '0')
            significant = TRUE;

        /* if it's a significant digit, add it to the accumulator */
        if (significant)
        {
            /* 
             *   Set up a stack register with the digit value.  The radix can
             *   be up to 36, so the digit value can be up to 35, so we need
             *   up to two decimal digits to represent the digit value. 
             */
            char dreg[5 + 2] = { 2, 0, 0, 0, 0 };
            set_uint_val(dreg, d);

            /* shift the accumulator and add the digit */
            mul_by_long(t1, radix);
            compute_abs_sum_into(t2, t1, dreg);

            /* swap the accumulators */
            char *tt = t1;
            t1 = t2;
            t2 = tt;
        }
    }

    /* if we ended up with the value in 'tmp', copy it into our result */
    if (t1 != ext)
        copy_val(ext, t1, FALSE);

    /* free our temporary register */
    release_temp_reg(thdl);

    /* set the sign */
    set_neg(ext, neg);
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a string value into an extension 
 */
void CVmObjBigNum::parse_str_into(char *ext, const char *str, size_t len)
{
    /* get the precision */
    size_t prec = get_prec(ext);

    /* set the type to number */
    set_type(ext, VMBN_T_NUM);

    /* initially zero the mantissa */
    memset(ext + VMBN_MANT, 0, (prec + 1)/2);

    /* set up to scan the string */
    utf8_ptr p((char *)str);
    size_t rem = len;

    /* initialize the exponent to zero */
    int exp = 0;

    /* presume it will be positive */
    int neg = FALSE;

    /* skip leading spaces */
    for ( ; rem != 0 && is_space(p.getch()) ; p.inc(&rem)) ;

    /* check for a sign */
    if (rem != 0 && p.getch() == '+')
    {
        /* skip the sign */
        p.inc(&rem);
    }
    else if (rem != 0 && p.getch() == '-')
    {
        /* note the sign and skip it */
        neg = TRUE;
        p.inc(&rem);
    }

    /* set the sign */
    set_neg(ext, neg);

    /* skip spaces after the sign */
    for ( ; rem != 0 && is_space(p.getch()) ; p.inc(&rem)) ;

    /* we haven't yet found a significant digit */
    int significant = FALSE;

    /* shift the digits into the value */
    int pt = FALSE;
    for (size_t idx = 0 ; rem != 0 ; p.inc(&rem))
    {
        wchar_t ch;

        /* get this character */
        ch = p.getch();

        /* check for a digit */
        if (is_digit(ch))
        {
            /* 
             *   if it's not a zero, we're definitely into the significant
             *   part of the number 
             */
            if (ch != '0')
                significant = TRUE;

            /* 
             *   if it's significant, add it to the number - skip leading
             *   zeros, since they add no information to the number 
             */
            if (significant)
            {
                /* if we're not out of precision, add the digit */
                if (idx < prec)
                {
                    /* set the next digit */
                    set_dig(ext, idx, value_of_digit(ch));

                    /* move on to the next digit position */
                    ++idx;
                }

                /* 
                 *   that's another factor of 10 if we haven't found the
                 *   decimal point (whether or not we're out of precision to
                 *   actually store the digit, count the increase in the
                 *   exponent) 
                 */
                if (!pt)
                    ++exp;
            }
            else if (pt)
            {
                /* 
                 *   we haven't yet found a significant digit, so this is a
                 *   leading zero, but we have found the decimal point - this
                 *   reduces the exponent by one 
                 */
                --exp;
            }
        }
        else if (ch == '.' && !pt)
        {
            /* 
             *   this is the decimal point - note it; from now on, we won't
             *   increase the exponent as we add digits 
             */
            pt = TRUE;
        }
        else if (ch == 'e' || ch == 'E')
        {
            int acc;
            int exp_neg = FALSE;

            /* exponent - skip the 'e' */
            p.inc(&rem);

            /* check for a sign */
            if (rem != 0 && p.getch() == '+')
            {
                /* skip the sign */
                p.inc(&rem);
            }
            else if (rem != 0 && p.getch() == '-')
            {
                /* skip it and note it */
                p.inc(&rem);
                exp_neg = TRUE;
            }

            /* parse the exponent */
            for (acc = 0 ; rem != 0 ; p.inc(&rem))
            {
                wchar_t ch;

                /* if this is a digit, add it to the exponent */
                ch = p.getch();
                if (is_digit(ch))
                {
                    /* accumulate the digit */
                    acc *= 10;
                    acc += value_of_digit(ch);
                }
                else
                {
                    /* that's it for the exponent */
                    break;
                }
            }

            /* if it's negative, apply the sign */
            if (exp_neg)
                acc = -acc;

            /* add the exponent to the one we've calculated */
            exp += acc;

            /* after the exponent, we're done with the whole number */
            break;
        }
        else
        {
            /* it looks like we've reached the end of the number */
            break;
        }
    }

    /* set the exponent */
    set_exp(ext, exp);

    /* normalize the number */
    normalize(ext);
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a string to determine the radix.  If it has a '0x' prefix, it's in
 *   hex.  If it has a '0' prefix and no decimal point, 'E', or '8' or '9',
 *   it's octal.  Otherwise it's decimal.
 */
int CVmObjBigNum::radix_from_string(const char *str, size_t len)
{
    /* set up to scan the string */
    utf8_ptr p((char *)str);
    size_t rem = len;

    /* skip leading spaces */
    for ( ; rem != 0 && is_space(p.getch()) ; p.inc(&rem)) ;

    /* skip the sign if present */
    if (rem != 0 && (p.getch() == '-' || p.getch() == '+'))
        p.inc(&rem);

    /* skip more spaces after the sign */
    for ( ; rem != 0 && is_space(p.getch()) ; p.inc(&rem)) ;

    /* check for a radix indicator */
    if (rem != 0 && p.getch() == '0')
    {
        /*
         *   There's a leading zero, so it could be hex or octal.  If there's
         *   an 'x', it's definitely hex.  Otherwise, it's octal as long as
         *   it's in purely integer format - that is, no decimal point or 'E'
         *   exponent marker.
         */
        p.inc(&rem);
        if (rem != 0 && (p.getch() == 'x' || p.getch() == 'X'))
        {
            /* 0x prefix, so it's definitely hex */
            return 16;
        }

        /* 
         *   There's a leading 0 but no 'x', so it could be octal or decimal,
         *   depending on what's in the rest of the string. 
         */
        for ( ; rem != 0 ; p.inc(&rem))
        {
            switch (p.getch())
            {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                /* octal digit, so we can't rule anything out yet */
                continue;

            case '.':
            case 'e':
            case 'E':
                /* 
                 *   decimal point or exponent marker, so it's a floating
                 *   point value, which can only be decimal 
                 */
                return 10;
                
            case '8':
            case '9':
                /* an 8 or a 9 can only appear in a decimal number */
                return 10;

            default:
                /* 
                 *   Anything else is invalid within a number, whether octal
                 *   integer or floating point, so we've reached the end of
                 *   the number.  Since we didn't encounter anything that
                 *   makes the number a floating point value or decimal
                 *   integer, we have an octal integer.
                 */
                return 8;
            }
        }

        /* 
         *   we reached the end of the number without finding anything that
         *   would rule out treating it as an octal integer, so that's what
         *   it is 
         */
        return 8;
    }
    else
    {
        /* no leading '0', so it's decimal */
        return 10;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Parse a string representation of a number in the given radix to
 *   determine the precision required to hold the value.  If 'radix' is 0, we
 *   infer the radix from the string: if it has a '0' prefix and no decimal
 *   point or 'E', it's an octal integer; if it has a '0x' prefix, it's a hex
 *   integer; otherwise it's decimal and can be an integer or a floating
 *   point value.
 */
int CVmObjBigNum::precision_from_string(const char *str, size_t len, int radix)
{
    /* if the radix is zero, infer the radix from the string */
    int inferred_radix = 0;
    if (radix == 0)
        radix = inferred_radix = radix_from_string(str, len);

    /* for decimal values, parse the full floating point format */
    if (radix == 10)
        return precision_from_string(str, len);

    /* set up to scan the string */
    utf8_ptr p((char *)str);
    size_t rem = len;

    /* skip leading spaces */
    for ( ; rem != 0 && is_space(p.getch()) ; p.inc(&rem)) ;

    /* skip the sign if present */
    if (rem != 0 && (p.getch() == '-' || p.getch() == '+'))
        p.inc(&rem);

    /* skip more spaces after the sign */
    for ( ; rem != 0 && is_space(p.getch()) ; p.inc(&rem)) ;

    /* if we inferred the radix as hax, skip any '0x' marker */
    if (inferred_radix == 16
        && rem != 0
        && p.getch() == '0'
        && (p.getch_at(1) == 'x' || p.getch_at(1) == 'X'))
    {
        /* skip the '0x' prefix */
        p.inc(&rem);
        p.inc(&rem);
    }

    /* scan digits */
    int prec, significant = FALSE;
    for (prec = 0 ; rem != 0 ; p.inc(&rem))
    {
        /* get this character */
        wchar_t ch = p.getch();

        /* get the digit value of this character */
        int d;
        if (ch >= '0' && ch <= '9')
            d = ch - '0';
        else if (ch >= 'A' && ch <= 'Z')
            d = ch - 'A' + 10;
        else if (ch >= 'a' && ch <= 'z')
            d = ch - 'a' + 10;
        else
            break;

        /* if the digit isn't within the radix, it's not a digit after all */
        if (d >= radix)
            break;

        /* it's significant if it's not a leading zero */
        if (ch != '0')
            significant = TRUE;

        /* 
         *   if we have found a significant digit so far, count this one - do
         *   not count leading zeros, whether they occur before or after the
         *   decimal point 
         */
        if (significant)
            ++prec;
    }

    /*
     *   Figure the decimal precision required to hold a number in the given
     *   radix.  This is a simple linear conversion: N base-R digits require
     *   log(R)/log(10) decimal digits.  Using log10 as our canonical log
     *   base means that log(R)/log(10) == log(R)/1 == log(R).  We of course
     *   have to round up for any fractional digits.
     */
    return (int)ceil(prec * log10((double)radix));
}

/*
 *   Parse a string representation of a number to determine the precision
 *   required to hold the value. 
 */
int CVmObjBigNum::precision_from_string(const char *str, size_t len)
{
    /* set up to scan the string */
    utf8_ptr p((char *)str);
    size_t rem = len;

    /* skip leading spaces */
    for ( ; rem != 0 && is_space(p.getch()) ; p.inc(&rem)) ;

    /* skip the sign if present */
    if (rem != 0 && (p.getch() == '-' || p.getch() == '+'))
        p.inc(&rem);

    /* skip more spaces after the sign */
    for ( ; rem != 0 && is_space(p.getch()) ; p.inc(&rem)) ;

    /* we haven't yet found a significant leading digit */
    int significant = FALSE;

    /* scan digits */
    size_t prec;
    int pt;
    int digcnt;
    for (prec = 0, digcnt = 0, pt = FALSE ; rem != 0 ; p.inc(&rem))
    {
        /* get this character */
        wchar_t ch = p.getch();

        /* see what we have */
        if (is_digit(ch))
        {
            /* 
             *   if it's not a zero, note that we've found a significant
             *   digit 
             */
            if (ch != '0')
                significant = TRUE;

            /* 
             *   if we have found a significant digit so far, count this one
             *   - do not count leading zeros, whether they occur before or
             *   after the decimal point 
             */
            if (significant)
                ++prec;

            /* count the digit in any case */
            ++digcnt;
        }
        else if (ch == '.' && !pt)
        {
            /* decimal point - note it and keep going */
            pt = TRUE;
        }
        else if (ch == 'e' || ch == 'E')
        {
            /* exponent - that's the end of the mantissa */
            break;
        }
        else
        {
            /* we've reached the end of the number */
            break;
        }
    }

    /* 
     *   if the precision is zero, the number must be zero - use the actual
     *   number of digits for the default precision, so that a value
     *   specified as "0.0000000" has eight digits of precision 
     */
    if (prec == 0)
        prec = digcnt;
    
    /* return the precision necessary to store the entire string */
    return prec;
}

/* ------------------------------------------------------------------------ */
/*
 *   Set my value to the given double 
 */
void CVmObjBigNum::set_double_val(char *ext, double val)
{
    /* get the precision */
    size_t prec = get_prec(ext);

    /* set the type to number */
    set_type(ext, VMBN_T_NUM);

    /* set the sign bit */
    if (val < 0)
    {
        set_neg(ext, TRUE);
        val = -val;
    }
    else
        set_neg(ext, FALSE);

    /* zero the mantissa */
    memset(ext + VMBN_MANT, 0, (prec + 1)/2);

    /* 
     *   Initialize our exponent 'exp' to the integer portion of log10(val).
     *   If val is a power of 10, exp == log10(val) is an integer, and val is
     *   exactly one times 10^exp (e.g., if val is 1000, log10(val) == 3.0,
     *   and val == 1*10^3).  If val isn't a power of 10, 10^exp is the
     *   nearest power of 10 below val.  For example, log10(1500) == 3.176,
     *   so exp == int(log10(1500)) == 3, so val = 1.5*10^3.  In other words,
     *   the value can be represented as some number between 1 and 10 times
     *   10^exp.  Our storage format is close to this: we store the mantissa
     *   as a value between 0 and 1.  So what we *really* want for the
     *   exponent is log10(val)+1 - this will give us an exponent that we can
     *   multiply by some number between 0 and 1 to recreate 'val'.  
     */
    int exp = (int)log10(val) + 1;
    set_exp(ext, exp);

    /* 
     *   We know that val divided by 10^exp is between 0 and 1.  So the most
     *   significant digit of val is val/10^(exp-1).  We can then divide the
     *   remainder of that calculation by 10^(exp-2) to get the next digit,
     *   and the remainder of that by 10^(exp-3) for the next digit, and so
     *   on.  Keep going until we've filled out our digits.
     */
    double base = pow(10.0, exp - 1);
    for (size_t i = 0 ; i < prec ; ++i, base /= 10.0)
    {
        /* 
         *   get the most significant remaining digit by dividing by the
         *   log10 exponent of the current value 
         */
        int dig = (int)(val / base);
        set_dig(ext, i, dig);

        /* get the remainder */
        val -= dig*base;
    }

    /* normalize the number */
    normalize(ext);
}


/* ------------------------------------------------------------------------ */
/*
 *   Set my value to a given integer value
 */
void CVmObjBigNum::set_int_val(char *ext, long val)
{
    if (val < 0)
    {
        set_uint_val(ext, -val);
        set_neg(ext, TRUE);
    }
    else
    {
        set_uint_val(ext, val);
    }
}

void CVmObjBigNum::set_uint_val(char *ext, ulong val)
{
    size_t prec;
    int exp;

    /* get the precision */
    prec = get_prec(ext);

    /* set the type to number */
    ext[VMBN_FLAGS] = 0;
    set_type(ext, VMBN_T_NUM);

    /* the value is unsigned, so it's non-negative */
    set_neg(ext, FALSE);

    /* initially zero the mantissa */
    memset(ext + VMBN_MANT, 0, (prec + 1)/2);

    /* initialize the exponent to zero */
    exp = 0;

    /* shift the integer value into the value */
    while (val != 0)
    {
        unsigned int dig;

        /* get the low-order digit of the value */
        dig = (unsigned int)(val % 10);

        /* shift the value one place */
        val /= 10;

        /* 
         *   shift our number one place right to accommodate this next
         *   higher-order digit 
         */
        shift_right(ext, 1);

        /* set the new most significant digit */
        set_dig(ext, 0, dig);

        /* that's another factor of 10 */
        ++exp;
    }

    /* set the exponent */
    set_exp(ext, exp);

    /* normalize the number */
    normalize(ext);
}

/* ------------------------------------------------------------------------ */
/*
 *   compare extensions 
 */
int CVmObjBigNum::compare_ext(const char *a, const char *b)
{
    /* if either one is not a number, they can't be compared */
    if (is_nan(a) || is_nan(b))
        err_throw(VMERR_INVALID_COMPARISON);

    /* if the signs differ, the positive one is greater */
    if (get_neg(a) != get_neg(b))
    {
        /* 
         *   if a is negative, it's the lesser number; otherwise it's the
         *   greater number 
         */
        return (get_neg(a) ? -1 : 1);
    }

    /* the signs are the same - compare the absolute values */
    int ret = compare_abs(a, b);

    /* 
     *   If the numbers are negative, and abs(a) > abs(b), a is actually the
     *   lesser number; if they're negative and abs(a) < abs(b), a is the
     *   greater number.  So, if a is negative, invert the sense of the
     *   result.  Otherwise, both numbers are positive, so the sense of the
     *   absolute value comparison is the same as the sense of the actual
     *   comparison, so just return the result directly.
     */
    return (get_neg(a) ? -ret : ret);
}

/* ------------------------------------------------------------------------ */
/*
 *   Compare the absolute values of two numbers 
 */
int CVmObjBigNum::compare_abs(const char *ext1, const char *ext2)
{
    size_t idx;
    int zero1 = is_zero(ext1);
    int zero2 = is_zero(ext2);
    size_t prec1 = get_prec(ext1);
    size_t prec2 = get_prec(ext2);
    size_t max_prec;
    size_t min_prec;
    const char *max_ext;
    int result;

    /* 
     *   if one is zero and the other is not, the one that's not zero has a
     *   larger absolute value 
     */
    if (zero1)
        return (zero2 ? 0 : -1);
    if (zero2)
        return (zero1 ? 0 : 1);

    /* 
     *   if the exponents differ, the one with the larger exponent is the
     *   larger number (this is necessarily true because we keep all numbers
     *   normalized) 
     */
    if (get_exp(ext1) > get_exp(ext2))
        return 1;
    else if (get_exp(ext1) < get_exp(ext2))
        return -1;

    /* 
     *   The exponents are equal, so we must compare the mantissas digit by
     *   digit 
     */

    /* get the larger of the two precisions */
    min_prec = prec2;
    max_prec = prec1;
    max_ext = ext1;
    if (prec2 > max_prec)
    {
        min_prec = prec1;
        max_prec = prec2;
        max_ext = ext2;
    }

    /* 
     *   The digits are in order from most significant to least significant,
     *   which means we can use memcmp to compare the common parts.  However,
     *   we can only compare an even number of digits this way, so round down
     *   the common precision if it's odd. 
     */
    if (min_prec > 1
        && (result = memcmp(ext1 + VMBN_MANT, ext2 + VMBN_MANT,
                            min_prec/2)) != 0)
    {
        /* 
         *   they're different in the common memcmp-able parts, so return the
         *   memcmp result 
         */
        return result;
    }

    /* if the common precision is odd, compare the final common digit */
    if ((min_prec & 1) != 0
        && (result = ((int)get_dig(ext1, min_prec-1)
                      - (int)get_dig(ext2, min_prec-1))) != 0)
        return result;

    /* 
     *   the common parts of the mantissas are identical; check each
     *   remaining digit of the longer one to see if any are non-zero 
     */
    for (idx = min_prec ; idx < max_prec ; ++idx)
    {
        /* 
         *   if this digit is non-zero, the longer one is greater, because
         *   the shorter one has an implied zero in this position 
         */
        if (get_dig(max_ext, idx) != 0)
            return (int)prec1 - (int)prec2;
    }

    /* they're identical */
    return 0;
}

/* ------------------------------------------------------------------------ */
/* 
 *   Compute a sum into the given buffer 
 */
void CVmObjBigNum::compute_sum_into(char *new_ext,
                                    const char *ext1, const char *ext2)
{
    /* check to see if the numbers have the same sign */
    if (get_neg(ext1) == get_neg(ext2))
    {
        /*
         *   the two numbers have the same sign, so the sum has the same sign
         *   as the two values, and the magnitude is the sum of the absolute
         *   values of the operands
         */

        /* compute the sum of the absolute values */
        compute_abs_sum_into(new_ext, ext1, ext2);

        /* set the sign to match that of the operand */
        set_neg(new_ext, get_neg(ext1));
    }
    else
    {
        /* 
         *   one is positive and the other is negative - subtract the
         *   absolute value of the negative one from the absolute value of
         *   the positive one; the sign will be set correctly by the
         *   differencing 
         */
        if (get_neg(ext1))
            compute_abs_diff_into(new_ext, ext2, ext1);
        else
            compute_abs_diff_into(new_ext, ext1, ext2);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Compute the sum of two absolute values into the given buffer.
 */
void CVmObjBigNum::compute_abs_sum_into(char *new_ext,
                                        const char *ext1, const char *ext2)
{
    int exp1 = get_exp(ext1), exp2 = get_exp(ext2), new_exp;
    int prec1 = get_prec(ext1), prec2 = get_prec(ext2);
    int new_prec = get_prec(new_ext);

    /* if one or the other is identically zero, return the other value */
    if (is_zero(ext1))
    {
        /* ext1 is zero - return ext2 */
        copy_val(new_ext, ext2, TRUE);
        return;
    }
    else if (is_zero(ext2))
    {
        /* ext2 is zero - return ext1 */
        copy_val(new_ext, ext1, TRUE);
        return;
    }

    /* 
     *   start the new value with the larger of the two exponents - this
     *   will have the desired effect of dropping the least significant
     *   digits if any digits must be dropped 
     */
    new_exp = exp1;
    if (exp2 > new_exp)
        new_exp = exp2;
    set_exp(new_ext, new_exp);

    /* compute the digit positions at the bounds of each of our values */
    int hi1 = exp1 - 1;
    int lo1 = exp1 - prec1;

    int hi2 = exp2 - 1;
    int lo2 = exp2 - prec2;

    int hi3 = new_exp - 1;
    int lo3 = new_exp - new_prec;

    /*
     *   If one of the values provides a digit one past the end of our
     *   result, remember that as the trailing digit that we're going to
     *   drop.  We'll check this when we're done to see if we need to
     *   round the number.  Since the result has precision at least as
     *   large as the larger of the two inputs, we can only be dropping
     *   significant digits from one of the two inputs - we can't be
     *   cutting off both inputs.  
     */
    int trail_dig = 0, trail_val = 0;
    if (lo3 - 1 >= lo1 && lo3 - 1 <= hi1)
    {
        /* remember the digit */
        trail_dig = get_dig(ext1, exp1 - (lo3-1) - 1);
        trail_val = get_ORdigs(ext1, exp1 - (lo3-1));
    }
    else if (lo3 - 1 >= lo2 && lo3 - 1 <= hi2)
    {
        /* remember the digit */
        trail_dig = get_dig(ext2, exp2 - (lo3-1) - 1);
        trail_val = get_ORdigs(ext2, exp2 - (lo3-1));
    }

    /* no carry yet */
    int carry = 0;

    /* add the digits */
    for (int pos = lo3 ; pos <= hi3 ; ++pos)
    {
        /* start with the carry */
        int acc = carry;

        /* add the first value digit if it's in range */
        if (pos >= lo1 && pos <= hi1)
            acc += get_dig(ext1, exp1 - pos - 1);

        /* add the second value digit if it's in range */
        if (pos >= lo2 && pos <= hi2)
            acc += get_dig(ext2, exp2 - pos - 1);

        /* check for carry */
        if (acc > 9)
        {
            /* reduce the accumulator and set the carry */
            acc -= 10;
            carry = 1;
        }
        else
        {
            /* no carry */
            carry = 0;
        }

        /* set the digit in the result */
        set_dig(new_ext, new_exp - pos - 1, acc);
    }

    /* 
     *   If we have a carry at the end, we must carry it out to a new
     *   digit.  In order to do this, we must shift the whole number right
     *   one place, then insert the one. 
     */
    if (carry)
    {
        /* 
         *   remember the last digit of the result, which we won't have
         *   space to store after the shift 
         */
        trail_val |= trail_dig;
        trail_dig = get_dig(new_ext, new_prec - 1);
        
        /* shift the result right */
        shift_right(new_ext, 1);

        /* increase the exponent to compensate for the shift */
        set_exp(new_ext, get_exp(new_ext) + 1);

        /* set the leading 1 */
        set_dig(new_ext, 0, 1);
    }

    /* the sum of two absolute values is always positive */
    set_neg(new_ext, FALSE);

    /* 
     *   round up if the trailing digits are >5000..., or exactly 5000...
     *   and the last digit is odd 
     */
    round_for_dropped_digits(new_ext, trail_dig, trail_val);

    /* normalize the number */
    normalize(new_ext);
}

/* ------------------------------------------------------------------------ */
/*
 *   Compute a difference into a buffer 
 */
void CVmObjBigNum::compute_diff_into(char *result,
                                     const char *ext1, const char *ext2)
{
    /* check to see if the numbers have the same sign */
    if (get_neg(ext1) == get_neg(ext2))
    {
        /* 
         *   The two numbers have the same sign, so the difference is the
         *   difference in the magnitudes, and has a sign depending on the
         *   order of the difference and the signs of the original values.
         *   If both values are positive, the difference is negative if the
         *   second value is larger than the first.  If both values are
         *   negative, the difference is negative if the second value has
         *   larger absolute value than the first.  
         */

        /* compute the difference in magnitudes */
        compute_abs_diff_into(result, ext1, ext2);

        /* 
         *   if the original values were negative, then the sign of the
         *   result is the opposite of the sign of the difference of the
         *   absolute values; otherwise, it's the same as the sign of the
         *   difference of the absolute values 
         */
        if (get_neg(ext1))
            negate(result);
    }
    else
    {
        /* 
         *   one is positive and the other is negative - the result has the
         *   sign of the first operand, and has magnitude equal to the sum of
         *   the absolute values 
         */

        /* compute the sum of the absolute values */
        compute_abs_sum_into(result, ext1, ext2);

        /* set the sign of the result to that of the first operand */
        if (!is_zero(result))
            set_neg(result, get_neg(ext1));
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Compute the difference of two absolute values into the given buffer 
 */
void CVmObjBigNum::compute_abs_diff_into(char *new_ext,
                                         const char *ext1, const char *ext2)
{
    int max_exp;
    int lo1, hi1;
    int lo2, hi2;
    int lo3, hi3;
    int pos;
    int result_neg = FALSE;
    int borrow;

    /* if we're subtracting zero or from zero, it's easy */
    if (is_zero(ext2))
    {
        /* 
         *   we're subtracting zero from another value - the result is
         *   simply the first value 
         */
        copy_val(new_ext, ext1, TRUE);
        return;
    }
    else if (is_zero(ext1))
    {
        /* 
         *   We're subtracting a value from zero - we know the value we're
         *   subtracting is non-zero (we already checked for that case
         *   above), and we're only considering the absolute values, so
         *   simply return the negative of the absolute value of the
         *   second operand.  
         */
        copy_val(new_ext, ext2, TRUE);
        set_neg(new_ext, TRUE);
        return;
    }

    /*
     *   Compare the absolute values of the two operands.  If the first
     *   value is larger than the second, subtract them in the given
     *   order.  Otherwise, reverse the order and note that the result is
     *   negative. 
     */
    if (compare_abs(ext1, ext2) < 0)
    {
        const char *tmp;
        
        /* the result will be negative */
        result_neg = TRUE;

        /* swap the order of the subtraction */
        tmp = ext1;
        ext1 = ext2;
        ext2 = tmp;
    }

    /* 
     *   start the new value with the larger of the two exponents - this
     *   will have the desired effect of dropping the least significant
     *   digits if any digits must be dropped 
     */
    max_exp = get_exp(ext1);
    if (get_exp(ext2) > max_exp)
        max_exp = get_exp(ext2);
    set_exp(new_ext, max_exp);

    /* compute the digit positions at the bounds of each of our values */
    hi1 = get_exp(ext1) - 1;
    lo1 = get_exp(ext1) - get_prec(ext1);

    hi2 = get_exp(ext2) - 1;
    lo2 = get_exp(ext2) - get_prec(ext2);

    hi3 = get_exp(new_ext) - 1;
    lo3 = get_exp(new_ext) - get_prec(new_ext);

    /* start off with no borrow */
    borrow = 0;

    /*
     *   Check for borrowing from before the least significant digit
     *   position in common to both numbers 
     */
    if (lo3-1 >= lo1 && lo3-1 <= hi1)
    {
        /* 
         *   In this case, we would be dropping precision from the end of
         *   the top number.  This case should never happen - the only way
         *   it could happen is for the bottom number to extend to the
         *   left of the top number at the most significant end.  This
         *   cannot happen because the bottom number always has small
         *   magnitude by this point (we guarantee this above).  So, we
         *   should never get here.
         */
        assert(FALSE);
    }
    else if (lo3-1 >= lo2 && lo3-1 <= hi2)
    {
        size_t idx;
        
        /*
         *   We're dropping precision from the bottom number, so we want
         *   to borrow into the subtraction if the rest of the number is
         *   greater than 5xxxx.  If it's exactly 5000..., do not borrow,
         *   since the result would end in 5 and we'd round up.
         *   
         *   If the next digit is 6 or greater, we know for a fact we'll
         *   have to borrow.  If the next digit is 4 or less, we know for
         *   a fact we won't have to borrow.  If the next digit is 5,
         *   though, we must look at the rest of the number to see if
         *   there's anything but trailing zeros.  
         */
        idx = (size_t)(get_exp(ext2) - (lo3-1) - 1);
        if (get_dig(ext2, idx) >= 6)
        {
            /* definitely borrow */
            borrow = 1;
        }
        else if (get_dig(ext2, idx) == 5)
        {
            /* borrow only if we have something non-zero following */
            for (++idx ; idx < get_prec(ext2) ; ++idx)
            {
                /* if it's non-zero, we must borrow */
                if (get_dig(ext2, idx) != 0)
                {
                    /* note the borrow */
                    borrow = 1;

                    /* no need to keep scanning */
                    break;
                }
            }
        }
    }

    /* subtract the digits from least to most significant */
    for (pos = lo3 ; pos <= hi3 ; ++pos)
    {
        int acc;

        /* start with zero in the accumulator */
        acc = 0;

        /* start with the top-line value if it's represented here */
        if (pos >= lo1 && pos <= hi1)
            acc = get_dig(ext1, get_exp(ext1) - pos - 1);

        /* subtract the second value digit if it's represented here */
        if (pos >= lo2 && pos <= hi2)
            acc -= get_dig(ext2, get_exp(ext2) - pos - 1);

        /* subtract the borrow */
        acc -= borrow;

        /* check for borrow */
        if (acc < 0)
        {
            /* increase the accumulator */
            acc += 10;

            /* we must borrow from the next digit up */
            borrow = 1;
        }
        else
        {
            /* we're in range - no need to borrow */
            borrow = 0;
        }

        /* set the digit in the result */
        set_dig(new_ext, get_exp(new_ext) - pos - 1, acc);
    }

    /* set the sign of the result as calculated earlier */
    set_neg(new_ext, result_neg);

    /* normalize the number */
    normalize(new_ext);
}

/* ------------------------------------------------------------------------ */
/*
 *   Compute the product of the values into the given buffer 
 */
void CVmObjBigNum::compute_prod_into(char *new_ext,
                                     const char *ext1, const char *ext2)
{
    size_t prec1 = get_prec(ext1);
    size_t prec2 = get_prec(ext2);
    size_t new_prec = get_prec(new_ext);
    size_t idx1;
    size_t idx2;
    size_t out_idx;
    size_t start_idx;
    int out_exp;
    
    /* start out with zero in the accumulator */
    memset(new_ext + VMBN_MANT, 0, (new_prec + 1)/2);

    /* 
     *   Initially write output in the same 'column' where the top number
     *   ends, so we start out with the same scale as the top number.  
     */
    start_idx = get_prec(ext1);

    /* 
     *   initial result exponent is the sum of the exponents, minus the
     *   number of digits in the bottom number (effectively, this lets us
     *   treat the bottom number as a whole number by scaling it enough to
     *   make it whole, soaking up the factors of ten into decimal point
     *   relocation) 
     */
    out_exp = get_exp(ext1) + get_exp(ext2) - prec2;

    /* there's no trailing accumulator digit yet */
    int trail_dig = 0, trail_val = 0;

    /* 
     *   Loop over digits in the bottom number, from least significant to
     *   most significant - we'll multiply each digit of the bottom number
     *   by the top number and add the result into the accumulator.  
     */
    for (idx2 = prec2 ; idx2 != 0 ; )
    {
        int carry;
        int dig;
        int ext2_dig;

        /* no carry yet on this round */
        carry = 0;
        
        /* start writing this round at the output start index */
        out_idx = start_idx;

        /* move to the next digit */
        --idx2;

        /* get this digit of ext2 */
        ext2_dig = get_dig(ext2, idx2);

        /* 
         *   if this digit of ext2 is non-zero, multiply the digits of
         *   ext1 by the digit (obviously if the digit is zero, there's no
         *   need to iterate over the digits of ext1) 
         */
        if (ext2_dig != 0)
        {
            /* 
             *   loop over digits in the top number, from least to most
             *   significant 
             */
            for (idx1 = prec1 ; idx1 != 0 ; )
            {
                /* move to the next digit of the top number */
                --idx1;
                
                /* move to the next digit of the accumulator */
                --out_idx;
                
                /* 
                 *   compute the product of the current digits, and add in
                 *   the carry from the last digit, then add in the
                 *   current accumulator digit in this position 
                 */
                dig = (get_dig(ext1, idx1) * ext2_dig)
                      + carry
                      + get_dig(new_ext, out_idx);

                /* figure the carry to the next digit */
                carry = (dig / 10);
                dig = dig % 10;

                /* store the new digit */
                set_dig(new_ext, out_idx, dig);
            }
        }

        /* 
         *   Shift the result accumulator right in preparation for the
         *   next round.  One exception: if this is the last (most
         *   significant) digit of the bottom number, and there's no
         *   carry, there's no need to shift the number, since we'd just
         *   normalize away the leading zero anyway 
         */
        if (idx2 != 0 || carry != 0)
        {
            /* remember the trailing digit that we're going to drop */
            trail_val |= trail_dig;
            trail_dig = get_dig(new_ext, new_prec - 1);

            /* shift the accumulator */
            shift_right(new_ext, 1);

            /* increase the output exponent */
            ++out_exp;

            /* insert the carry as the lead digit */
            set_dig(new_ext, 0, carry);
        }
    }

    /* set the result exponent */
    set_exp(new_ext, out_exp);

    /* 
     *   set the sign - if both inputs have the same sign, the output is
     *   positive, otherwise it's negative 
     */
    set_neg(new_ext, get_neg(ext1) != get_neg(ext2));

    /* if the trailing digit is 5 or greater, round up */
    round_for_dropped_digits(new_ext, trail_dig, trail_val);

    /* normalize the number */
    normalize(new_ext);
}

/* ------------------------------------------------------------------------ */
/*
 *   Compute a quotient into the given buffer.  If new_rem_ext is
 *   non-null, we'll save the remainder into this buffer.  We calculate
 *   the remainder only to the precision of the quotient.  
 */
void CVmObjBigNum::compute_quotient_into(char *new_ext,
                                         char *new_rem_ext,
                                         const char *ext1, const char *ext2)
{
    char *rem_ext;
    uint rem_hdl;
    char *rem_ext2;
    uint rem_hdl2;
    int quo_exp;
    size_t quo_idx;
    size_t quo_prec = get_prec(new_ext);
    size_t dvd_prec = get_prec(ext1);
    size_t dvs_prec = get_prec(ext2);
    char *dvs_ext;
    uint dvs_hdl;
    char *dvs_ext2;
    uint dvs_hdl2;
    int lead_dig_set;
    int zero_remainder;
    int acc;
    size_t rem_prec;

    /* if the divisor is zero, throw an error */
    if (is_zero(ext2))
        err_throw(VMERR_DIVIDE_BY_ZERO);

    /* if the dividend is zero, the result is zero */
    if (is_zero(ext1))
    {
        /* set the result to zero */
        set_zero(new_ext);

        /* if they want the remainder, it's zero also */
        if (new_rem_ext != 0)
            set_zero(new_rem_ext);

        /* we're done */
        return;
    }

    /* 
     *   Calculate the precision we need for the running remainder.  We
     *   must retain enough precision in the remainder to calculate exact
     *   differences, so we need the greater of the precisions of the
     *   dividend and the divisor, plus enough extra digits for the
     *   maximum relative shifting.  We'll have to shift at most one
     *   extra digit, but use two to be extra safe.  
     */
    rem_prec = dvd_prec;
    if (rem_prec < dvs_prec)
        rem_prec = dvs_prec;
    rem_prec += 2;

    /*   
     *   Make sure the precision is at least three, since it simplifies
     *   our digit approximation calculation.  
     */
    if (rem_prec < 3)
        rem_prec = 3;

    /* 
     *   Allocate two temporary registers for the running remainder, and
     *   one more for the multiplied divisor.  Note that we allocate the
     *   multiplied divisor at our higher precision so that we don't lose
     *   digits in our multiplier calculations.  
     */
    alloc_temp_regs(rem_prec, 3,
                    &rem_ext, &rem_hdl, &rem_ext2, &rem_hdl2,
                    &dvs_ext2, &dvs_hdl2);

    /* 
     *   Allocate another temp register for the shifted divisor.  Note
     *   that we need a different precision here, which is why we must
     *   make a separate allocation call 
     */
    err_try
    {
        /* make the additional allocation */
        alloc_temp_regs(dvs_prec, 1, &dvs_ext, &dvs_hdl);
    }
    err_catch_disc
    {
        /* delete the first group of registers we allocated */
        release_temp_regs(3, rem_hdl, rem_hdl2, dvs_hdl2);

        /* re-throw the exception */
        err_rethrow();
    }
    err_end;

    /* the dividend is the initial value of the running remainder */
    copy_val(rem_ext, ext1, TRUE);

    /* copy the initial divisor into our temp register */
    copy_val(dvs_ext, ext2, TRUE);

    /* we haven't set a non-zero leading digit yet */
    lead_dig_set = FALSE;

    /*
     *   scale the divisor so that the divisor and dividend have the same
     *   exponent, and absorb the multiplier in the quotient 
     */
    quo_exp = get_exp(ext1) - get_exp(ext2) + 1;
    set_exp(dvs_ext, get_exp(ext1));

    /* we don't have a zero remainder yet */
    zero_remainder = FALSE;

    /* 
     *   if the result of the floating point division is entirely fractional,
     *   the dividend is the remainder, and the quotient is zero 
     */
    if (new_rem_ext != 0 && quo_exp <= 0)
    {
        /* copy the initial remainder into the output remainder */
        copy_val(new_rem_ext, rem_ext, TRUE);

        /* set the quotient to zero */
        set_zero(new_ext);

        /* we have the result - no need to do any more work */
        goto done;
    }

    /* 
     *   Loop over each digit of precision of the quotient.
     */
    for (quo_idx = 0 ; ; )
    {
        int rem_approx, dvs_approx;
        int dig_approx;
        char *tmp;
        int exp_diff;

        /* start out with 0 in our digit accumulator */
        acc = 0;

        /*
         *   Get the first few digits of the remainder, and the first few
         *   digits of the divisor, rounding up the divisor and rounding
         *   down the remainder.  Compute the quotient - this will give us
         *   a rough guess and a lower bound for the current digit's
         *   value.  
         */
        rem_approx = (get_dig(rem_ext, 0)*100
                      + get_dig(rem_ext, 1)*10
                      + get_dig(rem_ext, 2));
        dvs_approx = (get_dig(dvs_ext, 0)*100
                      + (dvs_prec >= 2 ? get_dig(dvs_ext, 1) : 0)*10
                      + (dvs_prec >= 3 ? get_dig(dvs_ext, 2) : 0)
                      + 1);

        /* adjust for differences in the scale */
        exp_diff = get_exp(rem_ext) - get_exp(dvs_ext);
        if (exp_diff > 0)
        {
            /* the remainder is larger - scale it up */
            for ( ; exp_diff > 0 ; --exp_diff)
                rem_approx *= 10;
        }
        else if (exp_diff <= -3)
        {
            /* 
             *   The divisor is larger by more than 10^3, which means that
             *   the result of our integer division is definitely going to
             *   be zero, so there's no point in actually doing the
             *   calculation.  This is only a special case because, for
             *   sufficiently large negative differences, we'd have to
             *   multiply our divisor approximation by 10 so many times
             *   that we'd overflow a native int type, at which point we'd
             *   get 0 in the divisor, which would result in a
             *   divide-by-zero.  To avoid this, just put 1000 in our
             *   divisor, which is definitely larger than anything we can
             *   have in rem_ext at this point (since it was just three
             *   decimal digits, after all).  
             */
            dvs_approx = 1000;
        }
        else if (exp_diff < 0)
        {
            /* the divisor is larger - scale it up */
            for ( ; exp_diff < 0 ; ++exp_diff)
                dvs_approx *= 10;
        }

        /* calculate our initial guess for this digit */
        dig_approx = rem_approx / dvs_approx;

        /*
         *   If this digit is above 2, it'll save us a lot of work to
         *   subtract digit*divisor once, rather than iteratively
         *   deducting the divisor one time per iteration.  (It costs us a
         *   little to calculate the digit*divisor product, so we don't
         *   want to do this for very small digit values.)  
         */
        if (dig_approx > 2)
        {
            /* put the approximate digit in the accumulator */
            acc = dig_approx;

            /* make a copy of the divisor */
            copy_val(dvs_ext2, dvs_ext, FALSE);

            /* 
             *   multiply it by the guess for the digit - we know this
             *   will always be less than or equal to the real value
             *   because of the way we did the rounding 
             */
            mul_by_long(dvs_ext2, (ulong)dig_approx);

            /* subtract it from the running remainder */
            compute_abs_diff_into(rem_ext2, rem_ext, dvs_ext2);

            /* if that leaves zero in the remainder, note it */
            if (is_zero(rem_ext2))
            {
                zero_remainder = TRUE;
                break;
            }

            /* 
             *   swap the remainder registers, since rem_ext2 is now the
             *   new running remainder value 
             */
            tmp = rem_ext;
            rem_ext = rem_ext2;
            rem_ext2 = tmp;

            /*
             *   Now we'll finish up the job by subtracting the divisor
             *   from the remainder as many more times as necessary for
             *   the remainder to fall below the divisor.  We can't be
             *   exact at this step because we're not considering all of
             *   the digits, but we should only have one more subtraction
             *   remaining at this point.  
             */
        }
        
        /* 
         *   subtract the divisor from the running remainder as many times
         *   as we can 
         */
        for ( ; ; ++acc)
        {
            int comp_res;

            /* compare the running remainder to the divisor */
            comp_res = compare_abs(rem_ext, dvs_ext);
            if (comp_res < 0)
            {
                /* 
                 *   the remainder is smaller than the divisor - we have
                 *   our result for this digit 
                 */
                break;
            }
            else if (comp_res == 0)
            {
                /* note that we have a zero remainder */
                zero_remainder = TRUE;

                /* count one more subtraction */
                ++acc;

                /* we have our result for this digit */
                break;
            }

            /* subtract it */
            compute_abs_diff_into(rem_ext2, rem_ext, dvs_ext);

            /* 
             *   swap the remainder registers, since rem_ext2 is now the
             *   new running remainder value 
             */
            tmp = rem_ext;
            rem_ext = rem_ext2;
            rem_ext2 = tmp;
        }

        /* store this digit of the quotient */
        if (quo_idx < quo_prec)
        {
            /* store the digit */
            set_dig(new_ext, quo_idx, acc);
        }
        else if (quo_idx == quo_prec)
        {
            /* set the quotient's exponent */
            set_exp(new_ext, quo_exp);

            /* 
             *   This is the last digit, which we calculated for rounding
             *   purposes only.  If it's greater than 5, round up.  If it's
             *   exactly 5 and we have a non-zero remainder, round up.  If
             *   it's exactly 5 and we have a zero remainder, round up if the
             *   last digit is odd.  
             */
            round_for_dropped_digits(new_ext, acc, !zero_remainder);

            /* we've reached the rounding digit - we can stop now */
            break;
        }

        /* 
         *   if this is a non-zero digit, we've found a significant
         *   leading digit 
         */
        if (acc != 0)
            lead_dig_set = TRUE;

        /* 
         *   if we've found a significant leading digit, move to the next
         *   digit of the quotient; if not, adjust the quotient exponent
         *   down one, and keep preparing to write the first digit at the
         *   first "column" of the quotient
         */
        if (lead_dig_set)
            ++quo_idx;
        else
            --quo_exp;

        /* 
         *   If we have an exact result (a zero remainder), we're done.
         *   
         *   Similarly, if we've reached the units digit, and the caller
         *   wants the remainder, stop now - the caller wants an integer
         *   result for the quotient, which we now have.
         *   
         *   Similarly, if we've reached the rounding digit, and the
         *   caller wants the remainder, skip the rounding step - the
         *   caller wants an unrounded result for the quotient so that the
         *   quotient times the divisor plus the remainder equals the
         *   dividend.  
         */
        if (zero_remainder
            || (new_rem_ext != 0
                && ((int)quo_idx == quo_exp || quo_idx == quo_prec)))
        {
            /* zero any remaining digits of the quotient */
            for ( ; quo_idx < quo_prec ; ++quo_idx)
                set_dig(new_ext, quo_idx, 0);

            /* set the quotient's exponent */
            set_exp(new_ext, quo_exp);

            /* that's it */
            break;
        }

        /* divide the divisor by ten */
        set_exp(dvs_ext, get_exp(dvs_ext) - 1);
    }

    /* store the remainder if the caller wants the value */
    if (new_rem_ext != 0)
    {
        /* save the remainder into the caller's buffer */
        if (zero_remainder)
        {
            /* the remainder is exactly zero */
            set_zero(new_rem_ext);
        }
        else
        {
            /* copy the running remainder */
            copy_val(new_rem_ext, rem_ext, TRUE);

            /* the remainder has the same sign as the dividend */
            set_neg(new_rem_ext, get_neg(ext1));

            /* normalize the remainder */
            normalize(new_rem_ext);
        }
    }

    /* 
     *   the quotient is positive if both the divisor and dividend have
     *   the same sign, negative if they're different 
     */
    set_neg(new_ext, get_neg(ext1) != get_neg(ext2));

    /* normalize the quotient */
    normalize(new_ext);

done:
    /* release the temporary registers */
    release_temp_regs(4, rem_hdl, rem_hdl2, dvs_hdl, dvs_hdl2);
}

/* ------------------------------------------------------------------------ */
/*
 *   The BigNumber cache is implemented as an ordinary static.  This can be
 *   shared among VM instances (since it stores mathematical constants and
 *   provides some simple memory management facilities), so it's not
 *   necessary to use the VM global scheme (which is only needed for data
 *   that must be isolated per VM instance).  This simplifies interfaces to
 *   many of the low-level routines by letting us omit the VMG_ parameter,
 *   and allows many BigNumber features to be accessed without having to link
 *   in the whole VM global subsystem.  This last bit lets us use BigNumber
 *   calculations in the compiler, for example, which is useful for constant
 *   folding.
 */
CVmBigNumCache *S_bignum_cache = 0;

/* number of references to the cache */
static int S_bignum_cache_refs = 0;


/* ------------------------------------------------------------------------ */
/*
 *   Cache creation/deletion 
 */

void CVmObjBigNum::init_cache()
{
    if (S_bignum_cache_refs++ == 0)
        S_bignum_cache = new CVmBigNumCache(32);
}

void CVmObjBigNum::term_cache()
{
    if (--S_bignum_cache_refs == 0)
    {
        delete S_bignum_cache;
        S_bignum_cache = 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Allocate a temporary register 
 */
char *CVmObjBigNum::alloc_temp_reg(size_t prec, uint *hdl)
{
    /* allocate a register with enough space for the desired precision */
    char *p = S_bignum_cache->alloc_reg(calc_alloc(prec), hdl);

    /* if that succeeded, initialize the memory */
    if (p != 0)
    {
        /* set the desired precision */
        set_prec(p, prec);

        /* initialize the flags */
        p[VMBN_FLAGS] = 0;
    }

    /* return the register memory */
    return p;
}

/* ------------------------------------------------------------------------ */
/*
 *   release a temporary register 
 */
void CVmObjBigNum::release_temp_reg(uint hdl)
{
    /* release the register to the cache */
    S_bignum_cache->release_reg(hdl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Allocate a group of temporary registers 
 */
void CVmObjBigNum::alloc_temp_regs(size_t prec, size_t cnt, ...)
{
    va_list marker;
    size_t i;
    int failed;
    char **ext_ptr;
    uint *hdl_ptr;

    /* set up to read varargs */
    va_start(marker, cnt);

    /* no failures yet */
    failed = FALSE;

    /* scan the varargs list */
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get the next argument */
        ext_ptr = va_arg(marker, char **);
        hdl_ptr = va_arg(marker, uint *);

        /* allocate a register */
        *ext_ptr = alloc_temp_reg(prec, hdl_ptr);

        /* if this allocation failed, note it, but keep going for now */
        if (*ext_ptr == 0)
            failed = TRUE;
    }

    /* done reading argument */
    va_end(marker);

    /* if we had any failures, free all of the registers we allocated */
    if (failed)
    {
        /* restart reading the varargs */
        va_start(marker, cnt);

        /* scan the varargs and free the successfully allocated registers */
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get the next argument */
            ext_ptr = va_arg(marker, char **);
            hdl_ptr = va_arg(marker, uint *);

            /* free this register if we successfully allocated it */
            if (*ext_ptr != 0)
                release_temp_reg(*hdl_ptr);
        }

        /* done reading varargs */
        va_end(marker);

        /* throw the error */
        err_throw(VMERR_BIGNUM_NO_REGS);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Release a block of temporary registers 
 */
void CVmObjBigNum::release_temp_regs(size_t cnt, ...)
{
    /* start reading the varargs */
    va_list marker;
    va_start(marker, cnt);

    /* scan the varargs and free the listed registers */
    for (size_t i = 0 ; i < cnt ; ++i)
    {
        uint hdl;

        /* get the next handle */
        hdl = va_arg(marker, uint);

        /* free this register */
        release_temp_reg(hdl);
    }

    /* done reading varargs */
    va_end(marker);
}

/* ------------------------------------------------------------------------ */
/*
 *   Round the extension to the nearest integer 
 */
void CVmObjBigNum::round_to_int(char *ext)
{
    /* 
     *   the exponent is equal to the number of digits before the decimal
     *   point, so that's the number of digits we want to keep 
     */
    round_to(ext, get_exp(ext));
}

/*
 *   Round the extension to the given number of digits 
 */
void CVmObjBigNum::round_to(char *ext, int digits)
{
    /* if we're dropping digits, and we need to round up, round up */
    int prec = get_prec(ext);
    if (digits < prec)
    {
        /* note if we need to round up */
        int r = get_round_dir(ext, digits);

        /* zero the digits beyond the last one we're keeping */
        for (int i = digits ; i < prec ; ++i)
            set_dig(ext, i, 0);

        /* round up if necessary */
        if (r)
            round_up_abs(ext, digits);
    }
}

/*
 *   get the OR sum of digits from the given starting place to least
 *   significant 
 */
int CVmObjBigNum::get_ORdigs(const char *ext, int d)
{
    int sum, prec = get_prec(ext);
    for (sum = 0 ; d < prec ; sum |= get_dig(ext, d++)) ;
    return sum;
}

/*
 *   Determine the direction to round a value to the given number of digits.
 *   Returns 1 to round up, 0 to round down - this is effectively the value
 *   to add at the last digit we're keeping.  
 */
int CVmObjBigNum::get_round_dir(const char *ext, int digits)
{
    /* if the dropped digit is beyond the precision, there's no rounding */
    int prec = get_prec(ext);
    if (digits >= prec)
        return 0;

    /* we can't round to negative digits */
    if (digits < 0)
        digits = 0;

    /* 
     *   if the first dropped digit is greater than 5, round up; if it's less
     *   than 5, round down; otherwise we have to break the tie 
     */
    int d = get_dig(ext, digits);
    if (d > 5)
        return 1;
    if (d < 5)
        return 0;

    /* 
     *   it's a 5 - if there are any non-zero digits after this point, the
     *   value is greater than 5, so round up 
     */
    for (int i = digits + 1 ; i < prec ; ++i)
    {
        /* if it's a non-zero digit, round up */
        if (get_dig(ext, i) != 0)
            return 1;
    }

    /* 
     *   It's exactly 5000...., so we need to break the tie by rounding to
     *   the nearest even digit in the last digit we're keeping.  Get that
     *   last digit.  
     */
    d = (digits > 0 ? get_dig(ext, digits - 1) : 0);

    /* 
     *   If the digit is already even, round down (return 0) to stay at that
     *   even value.  If it's odd, round up (return 1) to get to the next
     *   even digit.  (d & 1) happens to be 1 if it's odd, 0 if it's even.
     */
    return d & 1;
}

/*
 *   Round a number based on dropped digits lost during a calculation.
 *   'trail_dig' is the most significant dropped digit, and 'trail_val' is
 *   the "|" sum of all of the less significant dropped digits.  
 */
void CVmObjBigNum::round_for_dropped_digits(
    char *ext, int trail_dig, int trail_val)
{
    if (trail_dig > 5
        || (trail_dig == 5 && trail_val > 0)
        || (trail_dig == 5 && trail_val == 0
            && (get_dig(ext, get_prec(ext)-1) & 1) != 0))
        round_up_abs(ext);
}

/*
 *   Round a number up.  This adds 1 starting at the least significant digit
 *   of the number. 
 */
void CVmObjBigNum::round_up_abs(char *ext)
{
    round_up_abs(ext, get_prec(ext));
}

/*
 *   Round a number up.  This adds 1 starting at the least significant digit
 *   we're keeping.  
 */
void CVmObjBigNum::round_up_abs(char *ext, int keep_digits)
{
    /* start with a "carry" into the least significant digit */
    int carry = TRUE;

    /* propagate the carry through the number's digits */
    for (int idx = keep_digits ; idx != 0 ; )
    {
        /* move to the next more significant digit */
        --idx;

        /* add the carry */
        int d = get_dig(ext, idx);
        if (d < 9)
        {
            /* bump it up */
            set_dig(ext, idx, d + 1);

            /* no carrying required, so we can stop now */
            carry = FALSE;
            break;
        }
        else
        {
            /* it's a '9', so bump it up to '0' and carry to the next digit */
            set_dig(ext, idx, 0);
        }
    }

    /* 
     *   If we carried out of the top digit, we must have had all nines, in
     *   which case we now have all zeros.  Put a 1 in the first digit and
     *   increase the exponent to renormalize. 
     */
    if (carry)
    {
        /* shift the mantissa */
        shift_right(ext, 1);

        /* increase the exponent */
        set_exp(ext, get_exp(ext) + 1);
        
        /* set the lead digit to 1 */
        set_dig(ext, 0, 1);
    }

    /* we know the value is non-zero now */
    ext[VMBN_FLAGS] &= ~VMBN_F_ZERO;
}

/* ------------------------------------------------------------------------ */
/*
 *   Copy a value, extending with zeros if expanding, or truncating or
 *   rounding, as desired, if the precision changes 
 */
void CVmObjBigNum::copy_val(char *dst, const char *src, int round)
{
    size_t src_prec = get_prec(src);
    size_t dst_prec = get_prec(dst);

    /* check to see if we're growing or shrinking */
    if (dst_prec > src_prec)
    {
        /* 
         *   it's growing - copy the entire old mantissa, plus the flags,
         *   sign, and exponent 
         */
        memcpy(dst + VMBN_EXP, src + VMBN_EXP,
               (VMBN_MANT - VMBN_EXP) + (src_prec + 1)/2);

        /* clear the balance of the mantissa */
        memset(dst + VMBN_MANT + (src_prec + 1)/2,
               0, (dst_prec + 1)/2 - (src_prec + 1)/2);

        /* 
         *   clear the digit just after the last digit we copied - we might
         *   have missed this with the memset, since it only deals with
         *   even-numbered pairs of digits
         */
        set_dig(dst, src_prec, 0);
    }
    else
    {
        /* it's shrinking - truncate the mantissa to the new length */
        memcpy(dst + VMBN_EXP, src + VMBN_EXP,
               (VMBN_MANT - VMBN_EXP) + (dst_prec + 1)/2);

        /* check for rounding */
        if (round && get_round_dir(src, dst_prec))
            round_up_abs(dst);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Multiply by an integer constant value 
 */
void CVmObjBigNum::mul_by_long(char *ext, unsigned long val)
{
    size_t idx;
    size_t prec = get_prec(ext);
    unsigned long carry = 0;

    /* 
     *   start at the least significant digit and work up through the digits 
     */
    for (idx = prec ; idx != 0 ; )
    {
        unsigned long prod;

        /* move to the next digit */
        --idx;

        /* 
         *   compute the product of this digit and the given value, and add
         *   in the carry from the last digit 
         */
        prod = (val * get_dig(ext, idx)) + carry;

        /* set this digit to the residue mod 10 */
        set_dig(ext, idx, prod % 10);

        /* carry the rest */
        carry = prod / 10;
    }

    /* if we have a carry left over, shift it in */
    int dropped_dig = 0, dropped_val = 0;
    while (carry != 0)
    {
        /* 
         *   Note if the previous dropped digit is non-zero, then note the
         *   newly dropped digit.  dropped_val tells us if there's anything
         *   non-zero after the last digit, in case the last digit turns out
         *   to be a 5.  
         */
        dropped_val |= dropped_dig;
        dropped_dig = get_dig(ext, prec - 1);

        /* shift the number and adjust the exponent */
        shift_right(ext, 1);
        set_exp(ext, get_exp(ext) + 1);

        /* shift in this digit of the carry */
        set_dig(ext, 0, carry % 10);

        /* take it out of the carry */
        carry /= 10;
    }

    /* 
     *   round up if the dropped digits are >5000..., or they're exactly
     *   5000... and the last digit we're keeping is odd 
     */
    round_for_dropped_digits(ext, dropped_dig, dropped_val);

    /* normalize the result */
    normalize(ext);
}

/*
 *   Divide the magnitude of the number by 2^32.  This is a special case of
 *   div_by_long() for splitting a number into 32-bit chunks.  Returns with
 *   the quotient in 'ext', and the remainder in 'remp'.  
 */
void CVmObjBigNum::div_by_2e32(char *ext, uint32_t *remp)
{
    size_t in_idx;
    size_t out_idx;
    int sig;
    size_t prec = get_prec(ext);
    unsigned long rem = 0;
    int exp = get_exp(ext);

    /* if it's entirely fractional, the result is 0 with remainder 0 */
    if (exp <= 0)
    {
        *remp = 0;
        set_zero(ext);
        return;
    }

    /* start at the most significant digit and work down */
    for (rem = 0, sig = FALSE, in_idx = out_idx = 0 ;
         out_idx < prec ; ++in_idx)
    {
        /* 
         *   Multiply the remainder by 10.  10 == (2+8), so this is the sum
         *   of rem<<1 and rem<<3.  This is handy because it makes it
         *   relatively easy to calculate the overflow from the 32-bit
         *   accumulator on 32-bit platforms: the overflow is the sum of the
         *   high bit and the high three bits, plus the carry from the low
         *   part.  The low part has a carry of 1 if rem<<1 + rem<<3 exceeds
         *   0xffffffff, which we can calculate in 32 bits as rem<<1 >
         *   0xffffffff - rem<<3.  
         */
        ulong rem1 = (rem << 1) & 0xffffffff;
        ulong rem3 = (rem << 3) & 0xffffffff;
        ulong hi = ((rem >> 31) & 0x00000001)
                   + ((rem >> 29) & 0x00000007)
                   + (rem1 > 0xffffffff - rem3 ? 1 : 0);
        rem = (rem1 + rem3) & 0xffffffff;

        /* 
         *   Add the current digit into the remainder.  We again need to
         *   carry the overflow into the high part. 
         */
        ulong dig = (in_idx < prec ? get_dig(ext, in_idx) : 0);
        hi += dig > 0xffffffff - rem ? 1 : 0;
        rem = (rem + dig) & 0xffffffff;

        /*
         *   We now have the quotient of rem/2^32 in 'hi', and the remainder
         *   in 'rem'.
         */
        int quo = (int)hi;

        /* if the quotient is non-zero, we've found a significant digit */
        if (quo != 0)
            sig = TRUE;

        /* 
         *   if we've found a significant digit, store it; otherwise, just
         *   reduce the exponent to account for an implied leading zero that
         *   we won't actually store 
         */
        if (sig)
        {
            /* store the digit */
            set_dig(ext, out_idx++, quo);

            /* 
             *   if we've reached the decimal place, stop here, since we're
             *   doing integer division 
             */
            if ((int)out_idx == exp)
                break;
        }
        else
        {
            /* all leading zeros so far - adjust the exponent */
            set_exp(ext, --exp);

            /* if this leaves us with a fractional result, we're done */
            if (exp == 0)
                break;
        }

        /* 
         *   If we've reached the last input digit and the remainder is zero,
         *   we're done - fill out the rest of the number with trailing zeros
         *   and stop looping.  If we've reached the decimal point in the
         *   output, stop here, since we're doing an integer division.  
         */
        if (rem == 0 && in_idx >= prec)
        {
            /* if we don't have any significant digits, we have a zero */
            if (!sig)
            {
                set_zero(ext);
                out_idx = prec;
            }

            /* we have our result */
            break;
        }
    }

    /* fill out the rest of the number with zeros */
    for ( ; out_idx < prec ; ++out_idx)
        set_dig(ext, out_idx, 0);

    /* normalize the result */
    normalize(ext);

    /* pass back the remainder */
    *remp = rem;
}

/*
 *   Divide by an integer constant value.  Note that 'val' must not exceed
 *   ULONG_MAX/10, because we have to be able to compute intermediate integer
 *   dividend values up to 10*val.  
 */
void CVmObjBigNum::div_by_long(char *ext, unsigned long val,
                               unsigned long *remp)
{
    size_t in_idx;
    size_t out_idx;
    int sig;
    size_t prec = get_prec(ext);
    unsigned long rem = 0;
    int exp = get_exp(ext);

    /* 
     *   if we're doing integer division, and the dividend is entirely
     *   fractional, the result is 0 with remainder 0 
     */
    if (remp != 0 && exp <= 0)
    {
        *remp = 0;
        set_zero(ext);
        return;
    }

    /* start at the most significant digit and work down */
    for (rem = 0, sig = FALSE, in_idx = out_idx = 0 ;
         out_idx < prec ; ++in_idx)
    {
        /* 
         *   shift this digit into the remainder - if we're past the end of
         *   the number, shift in an implied trailing zero 
         */
        rem *= 10;
        rem += (in_idx < prec ? get_dig(ext, in_idx) : 0);

        /* calculate the quotient and the next remainder */
        long quo = rem / val;
        rem %= val;

        /* if the quotient is non-zero, we've found a significant digit */
        if (quo != 0)
            sig = TRUE;

        /* 
         *   if we've found a significant digit, store it; otherwise, just
         *   reduce the exponent to account for an implied leading zero that
         *   we won't actually store 
         */
        if (sig)
        {
            /* store the digit */
            set_dig(ext, out_idx++, (int)quo);

            /* 
             *   if we've reached the decimal place, and the caller wants the
             *   remainder, stop here - the caller wants the integer quotient
             *   and remainder rather than the full quotient 
             */
            if (remp != 0 && (int)out_idx == exp)
                break;
        }
        else
        {
            /* all leading zeros so far - adjust the exponent */
            set_exp(ext, --exp);

            /* 
             *   if we're doing integer division, and this leaves us with a
             *   fractional result, we're done 
             */
            if (remp != 0 && exp == 0)
                break;
        }

        /* 
         *   if we've reached the last input digit and the remainder is zero,
         *   we're done - fill out the rest of the number with trailing zeros
         *   and stop looping
         */
        if (rem == 0 && in_idx >= prec)
        {
            /* check to see if we have any significant digits */
            if (!sig)
            {
                /* no significant digits - the result is zero */
                set_zero(ext);
                out_idx = prec;
            }

            /* we have our result */
            break;
        }
    }

    /* fill out any remaining output digits with zeros */
    for ( ; out_idx < prec ; ++out_idx)
        set_dig(ext, out_idx, 0);

    /* pass back the remainder if desired */
    if (remp != 0)
    {
        /* hand back the integer remainder */
        *remp = rem;
    }
    else
    {
        /*
         *   We're computing the full-precision quotient, not the quotient
         *   and remainder.  Round up if the remainder is more than half the
         *   divisor, or it's exactly half the divisor and the last digit is
         *   odd.
         *   
         *   (This is effectively testing to see if the next digits are
         *   5000...  or higher.  The next digit is the whole part of
         *   remainder*10/val, so it's >=5 if remainder/val >= 0.5, which is
         *   to say remainder >= val/2, or remainder*2 >= val.)  
         */
        if (rem*2 > val || (rem*2 == val && (get_dig(ext, prec-1) & 1)))
            round_up_abs(ext);
    }

    /* normalize the result */
    normalize(ext);
}

/* ------------------------------------------------------------------------ */
/*
 *   Normalize a number - shift it so that the first digit is non-zero.  If
 *   the number is zero, set the exponent to zero and clear the sign bit.  
 */
void CVmObjBigNum::normalize(char *ext)
{
    int idx;
    int prec = get_prec(ext);
    int all_zero;
    int nonzero_idx = 0;

    /* no work is necessary for anything but ordinary numbers */
    if (is_nan(ext))
        return;

    /* check for an all-zero mantissa */
    for (all_zero = TRUE, idx = 0 ; idx < prec ; ++idx)
    {
        /* check this digit */
        if (get_dig(ext, idx) != 0)
        {
            /* note the location of the first non-zero digit */
            nonzero_idx = idx;

            /* note that the number isn't all zeros */
            all_zero = FALSE;

            /* no need to keep searching */
            break;
        }
    }

    /* if it's zero or underflows, set the canonical zero format */
    if (all_zero || get_exp(ext) - nonzero_idx < -32768)
    {
        /* set the value to zero */
        set_zero(ext);
    }
    else
    {
        /* clear the zero flag */
        ext[VMBN_FLAGS] &= ~VMBN_F_ZERO;

        /* shift the mantissa left to make the first digit non-zero */
        if (nonzero_idx != 0)
            shift_left(ext, nonzero_idx);

        /* decrease the exponent to account for the mantissa shift */
        set_exp(ext, get_exp(ext) - nonzero_idx);
    }
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */
/*
 *   Shift the value left (multiply by 10^shift)
 */
void CVmObjBigNum::shift_left(char *ext, unsigned int shift)
{
    size_t prec = get_prec(ext);
    size_t i;

    /* do nothing with a zero shift */
    if (shift == 0)
        return;

    /* if it's an even shift, it's especially easy */
    if ((shift & 1) == 0)
    {
        /* simply move the bytes left by the required amount */
        for (i = 0 ; i + shift/2 < (prec+1)/2 ; ++i)
            ext[VMBN_MANT + i] = ext[VMBN_MANT + i + shift/2];

        /* zero the remaining digits */
        for ( ; i < (prec+1)/2 ; ++i)
            ext[VMBN_MANT + i] = 0;

        /* 
         *   be sure to zero the last digit - if we have an odd precision, we
         *   will have copied the garbage digit from the final half-byte 
         */
        set_dig(ext, prec - shift, 0);
    }
    else
    {
        /* apply the shift to each digit */
        for (i = 0 ; i + shift < prec  ; ++i)
        {
            unsigned int dig;

            /* get this source digit */
            dig = get_dig(ext, i + shift);

            /* set this destination digit */
            set_dig(ext, i, dig);
        }

        /* zero the remaining digits */
        for ( ; i < prec ; ++i)
            set_dig(ext, i, 0);
    }
}

/*
 *   Shift the value right (divide by 10^shift)
 */
void CVmObjBigNum::shift_right(char *ext, unsigned int shift)
{
    size_t prec = get_prec(ext);
    size_t i;

    /* if it's an even shift, it's especially easy */
    if ((shift & 1) == 0)
    {
        /* simply move the bytes left by the required amount */
        for (i = (prec+1)/2 ; i > shift/2 ; --i)
            ext[VMBN_MANT + i-1] = ext[VMBN_MANT + i-1 - shift/2];

        /* zero the leading digits */
        for ( ; i > 0 ; --i)
            ext[VMBN_MANT + i-1] = 0;
    }
    else
    {
        /* apply the shift to each digit */
        for (i = prec ; i > shift  ; --i)
        {
            unsigned int dig;

            /* get this source digit */
            dig = get_dig(ext, i-1 - shift);

            /* set this destination digit */
            set_dig(ext, i-1, dig);
        }

        /* zero the remaining digits */
        for ( ; i >0 ; --i)
            set_dig(ext, i-1, 0);
    }
}

/*
 *   Increment a number 
 */
void CVmObjBigNum::increment_abs(char *ext)
{
    size_t idx;
    int exp = get_exp(ext);
    int carry;

    /* start at the one's place, if represented */
    idx = (exp <= 0 ? 0 : (size_t)exp);

    /* 
     *   if the units digit is to the right of the number (i.e., the number's
     *   scale is large), there's nothing to do 
     */
    if (idx > get_prec(ext))
        return;

    /* increment digits */
    for (carry = TRUE ; idx != 0 ; )
    {
        int dig;

        /* move to the next digit */
        --idx;

        /* get the digit value */
        dig = get_dig(ext, idx);

        /* increment it, checking for carry */
        if (dig == 9)
        {
            /* increment it to zero and keep going to carry */
            set_dig(ext, idx, 0);
        }
        else
        {
            /* increment this digit */
            set_dig(ext, idx, dig + 1);

            /* there's no carry out */
            carry = FALSE;

            /* done */
            break;
        }
    }

    /* if we carried past the end of the number, insert the leading 1 */
    if (carry)
    {
        /* 
         *   if we still haven't reached the units position, shift right
         *   until we do 
         */
        while (get_exp(ext) < 0)
        {
            /* shift it right and adjust the exponent */
            shift_right(ext, 1);
            set_exp(ext, get_exp(ext) + 1);
        }

        /* shift the number right and adjust the exponent */
        shift_right(ext, 1);
        set_exp(ext, get_exp(ext) + 1);

        /* insert the leading 1 */
        set_dig(ext, 0, 1);
    }

    /* we know the value is now non-zero */
    ext[VMBN_FLAGS] &= ~VMBN_F_ZERO;
}

/*
 *   Determine if the fractional part is zero 
 */
int CVmObjBigNum::is_frac_zero(const char *ext)
{
    size_t idx;
    int exp = get_exp(ext);
    size_t prec = get_prec(ext);

    /* start at the first fractional digit, if represented */
    idx = (exp <= 0 ? 0 : (size_t)exp);

    /* scan the digits for a non-zero digit */
    for ( ; idx < prec ; ++idx)
    {
        /* if this digit is non-zero, the fraction is non-zero */
        if (get_dig(ext, idx) != 0)
            return FALSE;
    }

    /* we didn't find any non-zero fractional digits */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Convert to an integer value 
 */
int32_t CVmObjBigNum::ext_to_int(const char *ext, int &ov)
{        
    /* get the magnitude */
    ov = FALSE;
    uint32_t m = convert_to_int_base(ext, ov);

    /* apply the sign and check limits */
    if (get_neg(ext))
    {
        /* 
         *   a T3 VM int is a 32-bit signed value (irrespective of the local
         *   platform int size), so the magnitude of a negative value can't
         *   exceed 2147483648
         */
        if (m > 2147483648U)
            ov = TRUE;

        /* negate the number and return it */
        return -(long)m;
    }
    else
    {
        /* 
         *   a T3 VM positive int value can't exceed 2147483647L (regardless
         *   of the local platform int size) 
         */
        if (m > 2147483647U)
            ov = TRUE;

        /* return the value */
        return (long)m;
    }
}

/*
 *   Convert to unsigned int 
 */
uint32_t CVmObjBigNum::convert_to_uint(int &ov) const
{
    /* negative numbers can't be converted to unsigned */
    ov = get_neg(ext_);

    /* return the magnitude */
    return convert_to_int_base(ext_, ov);
}

/*
 *   Get the magnitude of an integer value, ignoring the sign
 */
uint32_t CVmObjBigNum::convert_to_int_base(const char *ext, int &ov)
{
    size_t prec = get_prec(ext);
    int exp = get_exp(ext);
    size_t idx;
    size_t stop_idx;
    int round_inc;
    
    /* start the accumulator at zero */
    uint32_t acc = 0;

    /* 
     *   if the exponent is negative, it means that the first non-zero digit
     *   is in the second position after the decimal point, so even after
     *   rounding, the result will always be zero 
     */
    if (exp < 0)
        return 0;

    /* get the rounding direction for truncating at the decimal point */
    round_inc = get_round_dir(ext, exp);

    /* stop at the first fractional digit */
    if (exp <= 0)
    {
        /* all digits are fractional */
        stop_idx = 0;
    }
    else if ((size_t)exp < prec)
    {
        /* stop at the first fractional digit */
        stop_idx = (size_t)exp;
    }
    else
    {
        /* all of the digits are in the whole part */
        stop_idx = prec;
    }

    /* convert the integer part digit by digit */
    if (stop_idx > 0)
    {
        /* loop over digits */
        for (idx = 0 ; idx < stop_idx ; ++idx)
        {
            /* get this digit */
            int dig = get_dig(ext, idx);

            /* make sure that shifting the accumulator won't overflow */
            ov |= (acc > UINT32MAXVAL/10);

            /* shift the accumulator */
            acc *= 10;
            
            /* make sure this digit won't overflow the 32-bit VM int type */
            ov |= (acc > (UINT32MAXVAL - dig));

            /* add the digit */
            acc += dig;
        }
    }

    /* make sure rounding won't overflow */
    ov |= (acc > UINT32MAXVAL - round_inc);

    /* return the result adjusted for rounding */
    return acc + round_inc;
}

/*
 *   Convert to a 64-bit portable representation, signed 
 */
void CVmObjBigNum::wp8s(char *buf, int &ov) const
{
    /* note if the value is negative */
    int neg = get_neg(ext_);

    /* generate the absolute value */
    wp8abs(buf, ov);

    /* if it's negative, compute the 2's complement of the buffer */
    if (neg)
    {
        /* take the 2's complement */
        twos_complement_p8((unsigned char *)buf);

        /* if it didn't come out negative, we have an overflow */
        ov |= ((buf[7] & 0x80) == 0);
    }
    else
    {
        /* positive - the sign bit is set, we overflowed */
        ov |= (buf[7] & 0x80);
    }
}

/*
 *   Convert to a 64-bit portable representation, unsigned
 */
void CVmObjBigNum::wp8(char *buf, int &ov) const
{
    /* generate the absolute value */
    wp8abs(buf, ov);

    /* 
     *   if the value is negative, we can't represent it as unsigned; take
     *   the two's complement in case they want the truncated version of the
     *   value, and set the overflow flag 
     */
    if (get_neg(ext_))
    {
        ov = TRUE;
        twos_complement_p8((unsigned char *)buf);
    }
}

/*
 *   generate the portable 64-bit integer representation of the absolute
 *   value of the number 
 */
void CVmObjBigNum::wp8abs(char *buf, int &ov) const
{
    /* 
     *   as a rough cut, if the number is greater than 10^30, it's definitely
     *   too big for a 64-bit integer buffer 
     */
    ov = (get_exp(ext_) > 30);

    /* 
     *   Make a local copy of the value - we need at most 20 digits of
     *   precision, since we only want the integer part.  
     */
    char tmp[5 + 20] = { 20, 0, 0, 0, 0 };
    copy_val(tmp, ext_, TRUE);

    /* 
     *   Divide the value by 2^32.  This splits the value into two 32-bit
     *   halves for us, which are then easy to arrange into the buffer with
     *   integer arithmetic.  
     */
    uint32_t lo, hi;
    div_by_2e32(tmp, &lo);
    hi = convert_to_int_base(tmp, ov);

    /* 
     *   store the low part in the first four bytes, the high part in the
     *   next four bytes 
     */
    oswp4(buf, lo);
    oswp4(buf + 4, hi);
}

/*
 *   Encode the integer portion of the number as a BER compressed integer. 
 */
void CVmObjBigNum::encode_ber(char *buf, size_t buflen, size_t &outlen,
                              int &ov)
{
    /* BER can only store unsigned integers */
    ov = get_neg(ext_);

    /* clear the output buffer */
    outlen = 0;

    /* make a temporary copy of the value in a temp register */
    uint thdl;
    char *tmp = alloc_temp_reg(get_prec(ext_), &thdl);
    copy_val(tmp, ext_, FALSE);

    /* keep going until the number is zero or we run out of space */
    int z;
    while (!(z = is_zero(tmp)) && outlen < buflen)
    {
        /* get the low-order 7 bits by getting the remainder mod 128 */
        ulong rem;
        div_by_long(tmp, 128, &rem);
        
        /* store it */
        buf[outlen++] = (char)rem;
    }

    /* release the temporary register */
    release_temp_reg(thdl);

    /* if we ran out of buffer before we ran out of digits, we overflowed */
    ov |= (!z);

    /* if we wrote nothing, write a trivial zero value */
    if (outlen == 0)
        buf[outlen++] = 0;

    /* 
     *   We stored the bytes from least significant to most significant, but
     *   the standard format is the other way around.  Reverse the bytes.  
     */
    int i, j;
    for (i = 0, j = (int)outlen - 1 ; i < j ; ++i, --j)
    {
        char tmpc = buf[i];
        buf[i] = buf[j];
        buf[j] = tmpc;
    }

    /* set the high bit in every byte except the last one */
    for (i = 0 ; i < (int)outlen - 1 ; ++i)
        buf[i] |= 0x80;
}

/*
 *   Calculate the 2's complement of a portable 8-byte integer buffer 
 */
void CVmObjBigNum::twos_complement_p8(unsigned char *p)
{
    size_t i;
    int carry;
    for (i = 0, carry = 1 ; i < 8 ; ++i)
    {
        p[i] = (~p[i] + carry) & 0xff;
        carry &= (p[i] == 0);
    }
}

/*
 *   Convert to double 
 */
double CVmObjBigNum::ext_to_double(const char *ext)
{
    /* note the precision and sign */
    size_t prec = get_prec(ext);
    int is_neg = get_neg(ext);

    /* if we're not a number (INF, NAN), it's an error */
    if (get_type(ext) != VMBN_T_NUM)
        err_throw(VMERR_NO_DOUBLE_CONV);

    /* 
     *   if our absolute value is too large to store in a double, throw an
     *   overflow error 
     */
    if (compare_abs(ext, cache_dbl_max()) > 1)
        err_throw(VMERR_NUM_OVERFLOW);

    /* 
     *   Our value is represented as .abcdef * 10^exp, where a is our first
     *   digit, b is our second digit, etc.  Start off the accumulator with
     *   our most significant digit, a.  The actual numeric value of this
     *   digit is a*10^(exp-1) - one less than the exponent, because of the
     *   implied decimal point at the start of the mantissa.  
     */
    double base = pow(10.0, get_exp(ext) - 1);
    double acc = get_dig(ext, 0) * base;

    /* 
     *   Now add in the remaining digits, starting from the second most
     *   significant.
     *   
     *   We might have more precision than a double can hold.  Digits added
     *   beyond the precision of the double will have no effect on the double
     *   value, since a double will naturally drop digits at the less
     *   significant end when we exceed its precision.  There's no point in
     *   adding digits beyond that point.  The maximum number of digits a
     *   native double can hold is given by the float.h constant DBL_DIG; go
     *   a couple of digits beyond this anyway, since many float packages
     *   perform intermediate calculations at slightly higher precision and
     *   can thus benefit from an extra digit or two for rounding purposes.  
     */
    size_t max_digits = (prec > DBL_DIG + 2 ? DBL_DIG + 2 : prec);
    for (size_t idx = 1 ; idx < max_digits ; ++idx)
    {
        /* get this digit */
        int dig = get_dig(ext, idx);

        /* adjust the exponent base for this digit */
        base /= 10.0;
        
        /* add the digit */
        acc += dig * base;
    }

    /* apply the sign */
    if (is_neg)
        acc = -acc;

    /* return the result */
    return acc;
}

/* ------------------------------------------------------------------------ */
/*
 *   Cache DBL_MAX 
 */
const char *CVmObjBigNum::cache_dbl_max()
{
    /* get the cache register; use the platform precision for a double */
    size_t prec = DBL_DIG;
    int expanded;
    char *ext = S_bignum_cache->get_dbl_max_reg(calc_alloc(prec), &expanded);

    /* if that failed, throw an error */
    if (ext == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* if we got a previously cached value, return it */
    if (!expanded && get_prec(ext) >= prec)
        return ext;

    /* we allocated or reallocated the register, so set the new precision */
    set_prec(ext, prec);

    /* store DBL_MAX in the register */
    set_double_val(ext, DBL_MAX);

    /* return the register */
    return ext;
}

/*
 *   Cache DBL_MIN 
 */
const char *CVmObjBigNum::cache_dbl_min()
{
    /* get the cache register; use the platform precision for a double */
    size_t prec = DBL_DIG;
    int expanded;
    char *ext = S_bignum_cache->get_dbl_min_reg(calc_alloc(prec), &expanded);

    /* if that failed, throw an error */
    if (ext == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* if we got a previously cached value, return it */
    if (!expanded && get_prec(ext) >= prec)
        return ext;

    /* we allocated or reallocated the register, so set the new precision */
    set_prec(ext, prec);

    /* store DBL_MIN in the register */
    set_double_val(ext, DBL_MIN);

    /* return the register */
    return ext;
}


/* ------------------------------------------------------------------------ */
/*
 *   Internal cache.  The cache stores frequently used constants (pi, e,
 *   DBL_MAX), and also provides memory for temporary registers for
 *   intermediate values in calculations.
 */

/*
 *   initialize 
 */
CVmBigNumCache::CVmBigNumCache(size_t max_regs)
{
    CVmBigNumCacheReg *p;
    size_t i;

    /* allocate our register array */
    reg_ = (CVmBigNumCacheReg *)t3malloc(max_regs * sizeof(reg_[0]));

    /* remember the number of registers */
    max_regs_ = max_regs;

    /* clear the list heads */
    free_reg_ = 0;
    unalloc_reg_ = 0;

    /* we haven't actually allocated any registers yet - clear them out */
    for (p = reg_, i = max_regs ; i != 0 ; ++p, --i)
    {
        /* clear this register descriptor */
        p->clear();

        /* link it into the unallocated list */
        p->nxt_ = unalloc_reg_;
        unalloc_reg_ = p;
    }

    /* we haven't allocated the constants registers yet */
    ln10_.clear();
    ln2_.clear();
    pi_.clear();
    e_.clear();
    dbl_max_.clear();
    dbl_min_.clear();
}

/*
 *   delete 
 */
CVmBigNumCache::~CVmBigNumCache()
{
    CVmBigNumCacheReg *p;
    size_t i;

    /* delete each of our allocated registers */
    for (p = reg_, i = max_regs_ ; i != 0 ; ++p, --i)
        p->free_mem();

    /* free the register list array */
    t3free(reg_);

    /* free the constant value registers */
    ln10_.free_mem();
    ln2_.free_mem();
    pi_.free_mem();
    e_.free_mem();
    dbl_max_.free_mem();
    dbl_min_.free_mem();
}

/*
 *   Allocate a register 
 */
char *CVmBigNumCache::alloc_reg(size_t siz, uint *hdl)
{
    CVmBigNumCacheReg *p;
    CVmBigNumCacheReg *prv;

    /* 
     *   search the free list for an available register satisfying the size
     *   requirements 
     */
    for (p = free_reg_, prv = 0 ; p != 0 ; prv = p, p = p->nxt_)
    {
        /* if it satisfies the size requirements, return it */
        if (p->siz_ >= siz)
        {
            /* unlink it from the free list */
            if (prv == 0)
                free_reg_ = p->nxt_;
            else
                prv->nxt_ = p->nxt_;

            /* it's no longer in the free list */
            p->nxt_ = 0;

            /* return it */
            *hdl = (uint)(p - reg_);
            return p->buf_;
        }
    }

    /* 
     *   if there's an unallocated register, allocate it and use it;
     *   otherwise, reallocate the smallest free register 
     */
    if (unalloc_reg_ != 0)
    {
        /* use the first unallocated register */
        p = unalloc_reg_;

        /* unlink it from the list */
        unalloc_reg_ = unalloc_reg_->nxt_;
    }
    else if (free_reg_ != 0)
    {
        CVmBigNumCacheReg *min_free_p;
        CVmBigNumCacheReg *min_free_prv = 0;

        /* search for the smallest free register */
        for (min_free_p = 0, p = free_reg_, prv = 0 ; p != 0 ;
             prv = p, p = p->nxt_)
        {
            /* if it's the smallest so far, remember it */
            if (min_free_p == 0 || p->siz_ < min_free_p->siz_)
            {
                /* remember it */
                min_free_p = p;
                min_free_prv = prv;
            }
        }

        /* use the smallest register we found */
        p = min_free_p;

        /* unlink it from the list */
        if (min_free_prv == 0)
            free_reg_ = p->nxt_;
        else
            min_free_prv->nxt_ = p->nxt_;
    }
    else
    {
        /* there are no free registers - return failure */
        return 0;
    }

    /* 
     *   we found a register that was either previously unallocated, or was
     *   previously allocated but was too small - allocate or reallocate the
     *   register at the new size 
     */
    p->alloc_mem(siz);

    /* return the new register */
    *hdl = (uint)(p - reg_);
    return p->buf_;
}

/*
 *   Release a register
 */
void CVmBigNumCache::release_reg(uint hdl)
{
    CVmBigNumCacheReg *p = &reg_[hdl];

    /* add the register to the free list */
    p->nxt_ = free_reg_;
    free_reg_ = p;
}

/*
 *   Release all registers 
 */
void CVmBigNumCache::release_all()
{
    size_t i;

    /* mark each of our registers as not in use */
    for (i = 0 ; i < max_regs_ ; ++i)
        release_reg(i);
}


