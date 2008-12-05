/*
 *  Extended Audio Converter for SDL (Simple DirectMedia Layer)
 *  Copyright (C) 2002  Frank Ranostaj
 *                      Institute of Applied Physik
 *                      Johann Wolfgang Goethe-Universität
 *                      Frankfurt am Main, Germany
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Frank Ranostaj
 *  ranostaj@stud.uni-frankfurt.de
 *
 * (This code blatantly abducted for SDL_sound. Thanks, Frank! --ryan.)
 */

#ifndef Suffix
#error include filter_template.h with defined Suffix macro!
#else
#define CH(x) (Suffix((x)*))

/*-------------------------------------------------------------------------*/
/* this filter (Kaiser-window beta=6.8) gives a decent -80dB attentuation  */
/*-------------------------------------------------------------------------*/
#define sum_d(v,dx) ((int) v[CH(dx)] + v[CH(1-dx)])
static Sint16* Suffix(doubleRate)( Sint16 *outp, Sint16 *inp, int length,
                                   VarFilter* filt, RateAux* aux  )
{
    int out;
    Sint16 *to;

    to = inp - length;

    while( inp > to )
    {
        out =     0;
        out-=     9 * sum_d( inp, 16);
        out+=    23 * sum_d( inp, 15);
        out-=    46 * sum_d( inp, 14);
        out+=    83 * sum_d( inp, 13);
        out-=   138 * sum_d( inp, 12);
        out+=   217 * sum_d( inp, 11);
        out-=   326 * sum_d( inp, 10);
        out+=   474 * sum_d( inp,  9);
        out-=   671 * sum_d( inp,  8);
        out+=   936 * sum_d( inp,  7);
        out-=  1295 * sum_d( inp,  6);
        out+=  1800 * sum_d( inp,  5);
        out-=  2560 * sum_d( inp,  4);
        out+=  3863 * sum_d( inp,  3);
        out-=  6764 * sum_d( inp,  2);
        out+= 20798 * sum_d( inp,  1);

        outp[CH(1)] = ( 32770 * inp[CH(1)] + out) >> 16;
        outp[CH(0)] = ( 32770 * inp[CH(0)] + out) >> 16;

        inp -= CH(1);
        outp -= CH(2);
    }

    return outp;
}
#undef sum_d

/*-------------------------------------------------------------------------*/
#define sum_h(v,dx) ((int) v[CH(dx)] + v[CH(-dx)])
static Sint16* Suffix(halfRate)( Sint16 *outp, Sint16 *inp, int length,
                                 VarFilter* filt, RateAux* aux )
{
    int out;
    Sint16* to;

    to = inp + length;
    inp += aux->carry;

    while( inp < to )
    {
        out =     0;
        out-=     9 * sum_h( inp, 31);
        out+=    23 * sum_h( inp, 29);
        out-=    46 * sum_h( inp, 27);
        out+=    83 * sum_h( inp, 25);
        out-=   138 * sum_h( inp, 23);
        out+=   217 * sum_h( inp, 21);
        out-=   326 * sum_h( inp, 19);
        out+=   474 * sum_h( inp, 17);
        out-=   671 * sum_h( inp, 15);
        out+=   936 * sum_h( inp, 13);
        out-=  1295 * sum_h( inp, 11);
        out+=  1800 * sum_h( inp,  9);
        out-=  2560 * sum_h( inp,  7);
        out+=  3863 * sum_h( inp,  5);
        out-=  6764 * sum_h( inp,  3);
        out+= 20798 * sum_h( inp,  1);
        out+= 32770 * (int)inp[0];

        outp[0] = out >> 16;

        inp += CH(2);
        outp += CH(1);
    }

    aux->carry = inp < to + CH(1) ? 0 : CH(1);
    return outp;
}
#undef sum_h

/*-------------------------------------------------------------------------*/
static Sint16* Suffix(increaseRate)( Sint16 *outp, Sint16 *inp, int length,
                                     VarFilter* filter, RateAux* aux )
{
    const static int fsize = CH(2*_fsize);
    Sint16 *f;
    int out;
    int i, pos;
    Sint16* to;

    inp -= fsize;
    to = inp - length;
    pos = aux->pos;

    while( inp > to )
    {
        out = 0;
        f = filter->c[pos];
        for( i = _fsize + 1; --i; inp+=CH(4), f+=4 )
        {
    	    out+= f[0] * (int)inp[CH(0)];
    	    out+= f[1] * (int)inp[CH(1)];
    	    out+= f[2] * (int)inp[CH(2)];
    	    out+= f[3] * (int)inp[CH(3)];
        }
        outp[0] = out >> 16;

        pos = ( pos + filter->ratio.denominator - 1 )
              % filter->ratio.denominator;
        inp -= CH( 4 * _fsize );
        inp -= CH( filter->incr[pos] );
        outp -= CH(1);
    }

    aux->pos = pos;
    return outp;
}

/*-------------------------------------------------------------------------*/
static Sint16* Suffix(decreaseRate)( Sint16 *outp, Sint16 *inp, int length,
                                     VarFilter* filter, RateAux* aux )
{
    const static int fsize = CH(2*_fsize);
    Sint16 *f;
    int out;
    int i, pos;
    Sint16 *to;

    inp -= fsize;
    to = inp + length;
    pos = aux->pos;
    inp += aux->carry;

    while( inp < to )
    {
        out = 0;
        f = filter->c[pos];
        for( i = _fsize + 1; --i; inp+=CH(4), f+=4 )
        {
    	    out+= f[0] * (int)inp[CH(0)];
    	    out+= f[1] * (int)inp[CH(1)];
    	    out+= f[2] * (int)inp[CH(2)];
    	    out+= f[3] * (int)inp[CH(3)];
        }
        outp[0] = out >> 16;

        inp -= CH( 4 * _fsize );
        inp += CH( filter->incr[pos] );
        outp += CH(1);
        pos = ( pos + 1 ) % filter->ratio.denominator;
    }

    aux->pos = pos;
    aux->carry = inp < to + CH(1) ? 0 : CH(1);
    return outp;
}

/*-------------------------------------------------------------------------*/
#undef CH
#endif /* Suffix */

