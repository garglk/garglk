#include <tads.h>

main(args)
{
    "<title>Title&rsquo;s &lt;Entity Markup&gt;, &#8220;Numeric&#8221;, &amp;
    &ldquo;Quote&rdquo; Test</title>

    <b>Title&rsquo;s &lt;Entity Markup&gt;, &#8220;Numeric&#8221;, &amp;
    &ldquo;Quote&rdquo; Test</b>

    <p>

    This is a test of setting the window title with various entity
    markups.  That should about do it!
    \bPress a key to exit...";

    inputKey();
}
