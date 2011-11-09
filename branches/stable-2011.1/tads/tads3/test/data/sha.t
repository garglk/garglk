/*
 *   Copyright 2004 Michael J. Roberts.
 *   
 *   This is an implementation of the Secure Hash Algorithm-1 (SHA-1).  We
 *   provide a simple function that can be called with a string value to
 *   calculate the SHA hash of the string.  The result of hashing any
 *   message is a string of 160 bits, which we represent as a list of five
 *   32-bit integer values.
 *   
 *   SHA is a cryptographic hash that's useful for digital signatures and
 *   other applications.  The algorithm is designed to have the properties
 *   necessary for a good cryptographic hash; specifically, that the
 *   function is irreversible (that is, there exists no function R(x) such
 *   that R(H(x)) == x for arbitrary values x), that it is highly
 *   non-linear (that is, given a value y that is "near" x or otherwise
 *   related to x, H(y) has no detectable relationship to H(x)), and that
 *   the probability of a collision in the hash value is very small (that
 *   is, given x, it is intractable to find another value y such that H(y)
 *   == H(x)).  It's difficult to prove that these properties hold for any
 *   given function, but SHA is a public algorithm that's been analyzed by
 *   many experts, and it is widely considered to satisfy these
 *   requirements.
 *   
 *   Refer to http://www.itl.nist.gov/fipspubs/fip180-1.htm for details of
 *   the standard.  
 */

#include <tads.h>
#include <bytearr.h>
#include <charset.h>

/*
 *   Calculate the SHA value for the given message string.  The result is
 *   returned as a list of five (32-bit) integers; taken together, these
 *   give the 160-bit hash value.  Note that, by design of the standard,
 *   the hash value is always 160 bits, regardless of the input message
 *   length.  
 */
calcSHA(str)
{
    local msg;
    local msgLen;
    local paddedLen;
    local h0 = 0x67452301;
    local h1 = 0xefcdab89;
    local h2 = 0x98badcfe;
    local h3 = 0x10325476;
    local h4 = 0xc3d2e1f0;
    local w = new Vector(80);
    local val;
    local a, b, c, d, e, temp;
    local t;
    local i, j;
    local k = [0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xca62c1d6];
    local f;

    /* convert the message string to a byte array */
    msg = str.mapToByteArray(new CharacterSet('us-ascii'));

    /*
     *   We need to extend this to a 512-bit (64-byte) multiple, including
     *   padding at the end.  The padding is a minimum of 9 extra bytes,
     *   because we have to add a '10000000' (0x80) byte and and 8-byte
     *   length suffix.  So, compute the byte length including the minimum
     *   9 extra bytes, and round up to the next multiple of 64.  
     */
    msgLen = msg.length();
    paddedLen = (msgLen + 9 + 63) & ~63;

    /* allocate the larger padded array */
    msg = new ByteArray(msg, 1, paddedLen);

    /* add the padding: start with the 0x80 byte */
    msg[msgLen + 1] = 0x80;

    /* 
     *   add the 0x00 bytes - we need to fill the space we added beyond the
     *   fixed parts (the 0x00 byte and length suffix), which add up to 9
     *   characters in all, so this is simply the padded length minus the
     *   message length minus 9 
     */
    msg.fillValue(0, msgLen + 2, paddedLen - msgLen - 9);

    /* add the length suffix (in *bits*), as a big-endian value */
    msg.writeInt(paddedLen - 7, FmtInt32BE, 0);
    msg.writeInt(paddedLen - 3, FmtInt32BE, msgLen * 8);

    /* now process the message */
    for (i = 1 ; i <= paddedLen ; i += 64)
    {
        /* copy the message words into the w array */
        for (j = 0 ; j < 16 ; ++j)
            w[j+1] = msg.readInt(i + j*4, FmtInt32BE);

        /* calculate w's 16-79 */
        for (t = 17 ; t <= 80 ; ++t)
        {
            val = w[t-3] ^ w[t-8] ^ w[t-14] ^ w[t-16];
            w[t] = shaROL(val, 1);
        }

        /* calculate the a-e values */
        a = h0;
        b = h1;
        c = h2;
        d = h3;
        e = h4;

        /* iteratively calculate the new a-e values */
        for (t = 1 ; t <= 80 ; ++t)
        {
            /* calculate the f[t] value */
            if (t <= 20)
                f = (b & c) | (~b & d);
            else if (t <= 40 || t > 60)
                f = b ^ c ^ d;
            else if (t <= 60)
                f = (b & c) | (b & d) | (c & d);

            /* calculate the new a-e values for this round */
            temp = shaROL(a, 5) + f + e + w[t] + k[(t-1)/20 + 1];
            e = d;
            d = c;
            c = shaROL(b, 30);
            b = a;
            a = temp;
        }

        /* calculate the new h's */
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    /* the digest is now the h's - return it as a list */
    return [h0, h1, h2, h3, h4];
}

/* SHA service routine - rotate 'val' left by 'b' bits */
shaROL(val, b)
{
    /* 
     *   Shift the value left by the given number of bits, and OR in the
     *   result of shifting it right by the complementary amount (32 minus
     *   the number of bits).  Mask off any high-order bits filled in for
     *   us in the right-shift; if the value is signed, those will show up.
     */
    return (val << b) | ((val >> (32 - b)) & ((1 << b) - 1));
}

/* ------------------------------------------------------------------------ */
/*
 *   Test main: display the hash value of a string passed as the
 *   command-line argument to the program.
 */
#ifdef TEST_SHA

main(args)
{
    local hash;

    /* check arguments */
    if (args.length() < 2)
    {
        "No message specified.\n";
        return;
    }

    /* calculate the hash */
    hash = calcSHA(args[2]);

    /* display the result - this is a list of five integer values */
    "Digest = <<toString(hash[1], 16)>> <<toString(hash[2], 16)>>
    <<toString(hash[3], 16)>> <<toString(hash[4], 16)>>
    <<toString(hash[5], 16)>>\n";
}

#endif /* TEST_SHA */

