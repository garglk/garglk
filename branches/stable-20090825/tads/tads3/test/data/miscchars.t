#charset "cp1252"

/*
 *   Miscellaneous character display test.  We display a bunch of
 *   characters from various Unicode ranges in order to test conversion and
 *   display in the local character set. 
 */
#include <tads.h>

main(args)
{
    "Here are some Latin-1 characters: ÀÁÂÃÄ\n
    (You should see five capital A's, with various accents - grave,
    acute, circumflex, tilde, dieresis [umlaut])\b

    Now the same sequence for the other vowels (but sans tilde
    for E's, I's, and U's):  ÈÉÊË ÌÍÎÏ ÒÓÔÕÖ ÙÚÛÜ\n

    And in minuscules: àáâãä èéêë ìíîï òóôõö ùúûü\b

    Next, some Latin-2 characters: 
        \u0164 (T-caron); \u017d (Z-caron); \u0179 (Z-acute); \u0106 (C-acute);
        \u0118 (E-ogonek); \u010d (c-caron), \u0119 (e-ogonek);
        \u0151 (o-double-acute)\n

    A few from Latin-Viet Nam:
        \u01af (U-horn); \u01b0 (u-horn); \u0103 (a-breve);
        \u20ab (Dong sign)\b

    The Cyrillic alphabet:\n
        \tCapitals:
        \u0410\u0411\u0412\u0413\u0414\u0415\u0416\u0417\u0418";
        "\u0419\u041a\u041b\u041c\u041d\u041e\u041f\u0420\u0421";
        "\u0422\u0423\u0424\u0425\u0426\u0427\u0428\u0429\u042a";
        "\u042b\u042c\u042d\u042e\u042f\n
        \tMinuscules:
        \u0430\u0431\u0432\u0433\u0434\u0435\u0436\u0437\u0438";
        "\u0439\u043a\u043b\u043c\u043d\u043e\u043f\u0440\u0441";
        "\u0442\u0443\u0444\u0445\u0446\u0447\u0448\u0449\u044a";
        "\u044b\u044c\u044d\u044e\u044f\b

    The Hebrew alphabet:
        \u05d0\u05d1\u05d2\u05d3\u05d4\u05d5\u05d6\u05d7\u05d8";
        "\u05d9\u05da\u05db\u05dc\u05dd\u05de\u05df\u05e0\u05e1";
        "\u05e2\u05e3\u05e4\u05e5\u05e6\u05e7\u05e8\u05e9\u05ea\b

    Some miscellaneous Asian characters:\n
        \tCJK Unified: \u4e10\u4e11\u4e22\u4e70\u4e80\u4ee0\u4e88\n
        \tHangul: \u313f\u3140\u3141\u3142\u3146\n
        \tKatakana: \u31f0\u31f1\u31f2\u31f3\u31f4\n

     \bThat concludes this test!\n";
}

