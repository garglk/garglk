#include <tads.h>

main(args)
{
    local win;
    local win2;
    local win3;
    local moreStyle;

    /* 
     *   use MORE mode in most of the banners unless there's a "-nomore"
     *   option in the arguments 
     */
    moreStyle = (args.indexOf('-nomore') == nil ? BannerStyleMoreMode : 0);
    
    "This is a test of various banner API capabilities.\n";

    win = bannerCreate(nil, BannerLast, nil, BannerTypeText, BannerAlignLeft,
                       20, BannerSizePercent,
                       BannerStyleVScroll | BannerStyleAutoVScroll
                       | BannerStyleBorder | moreStyle);
    bannerSay(win, '<body bgcolor=silver text=black>This is a
        vertical banner window on the left sized to 20% of the
        total application window size.  This window has a border
        and a vertical scrollbar, and automatically scrolls vertically
        when new contents are displayed and they don\'t fit in the
        window\'s existing height. [end part one]');

    "Press a key to continue...\n";
    inputKey();

    bannerSay(win, '\bThis is some more text, following a blank line,
        in the left banner window.  The goal here is to eventually
        provide enough text to make the window scroll past the bottom,
        so that we can see the auto-vscroll effect in action.
        [end part two]');

    "Press another key to continue...\n";
    inputKey();

    bannerSay(win, '\bAnd finally, some more text, again following a
        blank line.  This should be enough to overflow 50 lines, so
        it should give us auto-scrolling on the DOS console version,
        and should be plenty even for a fairly good-sized window in
        HTML mode. [end part three]');

    "Press yet another key...\n";
    inputKey();

    win2 = bannerCreate(win, BannerFirst, nil, BannerTypeText, BannerAlignTop,
                        40, BannerSizePercent, BannerStyleBorder);
    bannerSay(win2, '<body bgcolor=navy text=white>This is a banner that
        should split off the top half of the left pane. This is to exercise
        the parent/child parameters. [end of child banner text]');

    "Press another key...\n";
    inputKey();

    bannerDelete(win);
    bannerDelete(win2);

    win = bannerCreate(nil, BannerLast, nil, BannerTypeText, BannerAlignRight,
                       15, BannerSizePercent,
                       BannerStyleVScroll | BannerStyleBorder);
    bannerSay(win, '<body bgcolor=yellow text=black>This is another
        vertical banner.  This time, the banner is only 15% of the total
        screen/application window width.  This one has a border and
        a vertical scrollbar, but it does NOT scroll automatically when
        it overflows. [end part one]');

    "\bPress a key...\n";
    inputKey();
    
    bannerSay(win, '\bHopefully we can overflpow this one a bit more
        quickly because of its diminutive width.  When this one overflows,
        it should just sit at the top - it should not scroll down by itself.
        On interpreters that offer a scrollbar, of course, the user can
        manually scroll down; on non-scrollbar interpreters, the text after
        the bottom of the window will be unviewable. [end part two]');

    "\nPress a key...\n";
    inputKey();

    win2 = bannerCreate(nil, BannerFirst, nil, BannerTypeText,
                        BannerAlignBottom, 20, BannerSizePercent,
                        BannerStyleVScroll | BannerStyleBorder
                        | BannerStyleAutoVScroll | moreStyle);
    bannerSay(win2, '<body bgcolor=text text=bgcolor>This one is a
        bottom banner.  It has a scrollbar and a border, and
        automatic vertical scrolling.  The interesting thing about this
        one will be to ensure that the border is properly excluded from
        the scrolling area.
        \bNote another important thing about this banner: it\'s inserted
        first in the banner list, so it should be formatted to take up
        the entire bottom of the screen.  The right-aligned banner still
        on the screen from earlier should shrink vertically to give room
        to this banner.[end part one]');
    
    "\bPress a key...\n";
    inputKey();

    bannerSay(win2, '\bHere is some more text.  That should about do
        it.  We should not need too much, since the banner should only
        get about ten lines or so on a 50-line main window.  So, we
        should be getting pretty close to the bottom by now.
        \b
        That blank line should help!
        \n[end part two]');

    win3 = bannerCreate(nil, BannerBefore, win2, BannerTypeTextGrid,
                        BannerAlignBottom, nil, nil, 0);

    /* size the banner to a height of eight lines */
    bannerSetScreenColor(win3, ColorRGB(255, 255, 0));
    bannerSetTextColor(win3, ColorRGB(255, 0, 0), ColorTransparent);
    bannerGoTo(win3, 8, 1);
    bannerSay(win3, '\u00A0');
    bannerFlush(win3);
    bannerSizeToContents(win3);

    /* show some stuff in the banner at various positions */
    bannerGoTo(win3, 5, 10);
    bannerSay(win3, '* at row 5, column 10');
    bannerSetTextColor(win3, ColorRGB(0, 0, 128), ColorTransparent);
    bannerGoTo(win3, 7, 20);
    bannerSay(win3, '* at row 7, column 20');

    "\bPress a key...\n";
    inputKey();

    bannerGoTo(win3, 3, 10);
    bannerSay(win3, '> at row 3, column 10');
    bannerGoTo(win3, 4, 20);
    bannerSay(win3, '> at row 4, column 20');
    bannerGoTo(win3, 6, 10);
    bannerSay(win3, '  Here  is  a   line    with   excess   spaces.   ');
    bannerSay(win3, '<eol>');

    "\nPress a key...\n";
    inputKey();

    bannerSetScreenColor(win3, ColorRGB(255, 255, 255));
    bannerSetTextColor(win3, ColorRGB(0, 255, 0), ColorTransparent);
    bannerGoTo(win3, 5, 15);
    bannerSay(win3, '@ 5/15 @');

    "\nPress a key...\n";
    inputKey();

    bannerSetScreenColor(win3, ColorMaroon);
    bannerClear(win3);
    bannerGoTo(win3, 3, 20);
    bannerSay(win3, '% cleared - now at row 3, col 20');

    /* show the sizes of the banners */
    "\bRight banner: "; showBannerInfo(win);
    "\bBottom banner 1: "; showBannerInfo(win2);
    "\bBottom banner 2 (grid): "; showBannerInfo(win3);

    "\bPress another key...\n";
    inputKey();

    win3 = bannerCreate(win2, BannerFirst, nil, BannerTypeText,
                        BannerAlignLeft,
                        33, BannerSizePercent, BannerStyleBorder);
    bannerSay(win3, '<body bgcolor=#a000a0 text=black>This is a child
        banner on the left side of the second-to-bottom banner.  It
        should take up a third of the banner\'s width, on the left side.');

    "Another key please...\n";
    inputKey();

    win3 = bannerCreate(win2, BannerAfter, win3, BannerTypeText,
                        BannerAlignLeft,
                        50, BannerSizePercent, BannerStyleBorder);
    bannerSay(win3, '<body bgcolor=#505050 text=white>This is a second
        child banner.  It should appear in the middle of the second-to-bottom
        banner, and should take up half the remaining area (so, the bottom
        banner should now be split into thirds).');

    "\bPress another key to exit...\n";
    inputKey();
}

showBannerInfo(win)
{
    local info = bannerGetInfo(win);

    "align = <<info[1]>>, style = <<toString(info[2], 16)>>,
    rows = <<info[3]>>, columns = <<info[4]>>, pixel height = <<info[5]>>,
    pixel width = <<info[6]>>";
}
