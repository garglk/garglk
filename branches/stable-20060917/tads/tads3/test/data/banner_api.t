#include <tads.h>

main(args)
{
    local win;
    local stat;
    local lineNum;
    
    "Left-banner test.  The idea here is to see how flashy the left banner
    is as we resize it repeatedly.\b";

    stat = bannerCreate(nil, BannerLast, nil, BannerTypeText,
                        BannerAlignTop, nil, nil, 0);
    bannerSay(stat, '<body bgcolor=teal text=white>');
    bannerSay(stat, 'Current line number = 0');
    bannerSizeToContents(stat);

    win = bannerCreate(nil, BannerLast, nil, BannerTypeText,
                       BannerAlignLeft, nil, nil, 0);
    bannerSay(win, '<body bgcolor=silver text=black>');
    bannerSay(win, 'one two three four five six seven eight nine ten\n');
    bannerSizeToContents(win);

    for (lineNum = 1 ;; ++lineNum)
    {
        local txt;
        
        "Enter some text to write to the banner; start the line with '+'
        to add it to the existing contents of the banner.  Start the
        line with '@' to switch to a right-side banner.  Type CLS to
        simply clear the main text window.  Enter an empty
        line to quit.\n&gt; ";

        "<font face='tads-input'>";
        txt = inputLine();
        "</font>";

        if (txt == nil || txt == '')
            break;

        if (txt.toLower() == 'cls')
        {
            clearScreen();
        }
        else if (txt.substr(1, 1) == '+')
        {
            bannerSay(win, txt.substr(2));
        }
        else if (txt.substr(1, 1) == '@')
        {
            bannerDelete(win);
            win = bannerCreate(nil, BannerLast, nil, BannerTypeText,
                               BannerAlignRight, nil, nil, 0);
            bannerSay(win, '<body bgcolor=olive text=white>');
            bannerSay(win, txt.substr(2));
        }
        else
        {
            bannerDelete(win);
            win = bannerCreate(nil, BannerLast, nil, BannerTypeText,
                               BannerAlignLeft, nil, nil, 0);
            bannerSay(win, '<body bgcolor=silver text=black>');
            bannerSay(win, txt);
        }
        bannerSay(win, '\n');
        bannerSizeToContents(win);

        bannerClear(stat);
        bannerSay(stat, '<body bgcolor=teal text=white>');
        bannerSay(stat, 'Current line number = ' + lineNum);
    }
}
