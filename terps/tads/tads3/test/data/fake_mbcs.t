#charset "fake_mbcs_for_testing"

/*
 *   This is a test of using our fake testing multibyte character set.  The
 *   character set represents each upper-case letter as an at-sign ("@")
 *   followed by the corresponding lower-case letter: so 'A' is written as
 *   '@a', and so on.  This exercises the multi-byte mapper without using a
 *   true multi-byte character set, to make the test results easier to
 *   interpret, and to facilitate testing on systems that are localized to
 *   the US or Western Europe.  
 */
main(args)
{
    "@this @is @a @test @of @the @fake @multi-@byte @character @set.
    @each word in the preceding sentence should be capitalized, but
    only the first word of this sentence should be.\n";
}
