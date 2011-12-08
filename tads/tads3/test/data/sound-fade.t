#include <tads.h>

main(args)
{
    "Test 1: single-track repeat with fade.  This will play a brief
    tone three times in a row; the first iteration should fade in,
    and the third iteration should fade out at the end, but there should
    be no fading between iterations.
    \b
    Press a key to start...";

    inputKey();
    "\b
    <sound cancel>
    <sound layer=background src='250hz.wav' fadein=1
       fadeout=1 repeat=3>
    Playing - press a key to continue...";

    inputKey();
    
    "\bTest 2: three-track loop with fade. This will play three brief
    tones (250 Hz, 500 Hz, 1000 Hz) in a row, then repeat the same
    three tones, then repeat them again, for a total of three times
    through the three-tone sequence.  The first of the nine chunks
    should fade in, the last should fade out, and there should be
    no fading between segments.
    \b
    Press a key to start...";

    inputKey();

    "\b
    <sound cancel>
    <sound layer=background src='250hz.wav' fadein=1
       fadeout=1 repeat=3 sequence=loop>
    <sound layer=background src='500hzL.wav'
       fadeout=1 repeat=3 sequence=loop>
    <sound layer=background src='1000hz.wav'
       fadeout=1 repeat=3 sequence=loop>
    Playing - press a key to continue...";

    inputKey();

    "\bTest completed.\n";
}
