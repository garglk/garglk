#include <tads.h>
#include <tadsnet.h>
#include <httpsrv.h>
#include <httpreq.h>
#include <bignum.h>
#include <strbuf.h>

modify List
    join(sep = '') { return listJoin(self, sep); }
;
listJoin(lst, delim)
{
    /* if the list is empty, return an empty string */
    local len = lst.length();
    if (len == 0)
        return '';

    /* concatenate the elements */
    local str = lst[1];
    for (local i = 2 ; i <= len ; ++i)
        str += delim + lst[i];

    /* return the finished string */
    return str;
}


main(args)
{
    /* set up the server */
    "Network name is <<getHostName()>>, IP is <<getLocalIP()>>\n";
    local l = new HTTPServer('localhost', nil, 65535);
    "HTTP server listening on localhost port <<l.getPortNum()>>\n";

    try
    {
        /* handle network events */
        for (local done = nil ; !done ; )
        {
            /* get the next network event */
            local evt = getNetEvent(60000);
            
            /* see what we have */
            switch (evt.evType)
            {
            case NetEvRequest:
                /* got a request */
                local req = evt.evRequest;
                "Got a request! query=<<req.getQuery()>>: ";
                local prs = req.parseQuery();
                local hdrs = req.getHeaders();
                local cookies = req.getCookies();
                prs.forEachAssoc({key, val: "<<key>>=<<val>>; "});
                "<br>";

                if (prs['wait'] != nil)
                {
                    "Waiting to reply - press Return...";
                    inputLine();
                }

                try
                {
                    if (prs['chunk'] != nil)
                    {
                        req.startChunkedReply(
                            'text/html', nil,
                            ['Set-Cookie: TEST=chunk-asdf; path=/',
                             'Set-Cookie: TEST2=chunk-jkl; path/']);

                        req.sendReplyChunk(
                            '<html><head>'
                            + '<title>TADS HTTP Server</title>'
                            + '</head><body>'
                            + '<h1>TADS HTPP Server</h1>'
                            + 'Here we are with a <b>chunked reply</b>!!!');

                        req.sendReplyChunk('<p>Here are the request '
                                           + 'parameters...<br><ul>');
                        prs.forEachAssoc(
                            { k, v: req.sendReplyChunk('<li><<k>>=<<v>>') });
                        req.sendReplyChunk('</ul>');

                        local sb = new StringBuffer();
                        sb.append('<p>This is some additional content that '
                                  + 'we\'re storing in a string buffer, to '
                                  + 'see how that goes.  For a really good '
                                  + 'test, we\'ll need to make sure we go '
                                  + 'beyond the internal buffer so that we '
                                  + 'have to iterate a bit.  To that end, '
                                  + 'well fill this up with some generated '
                                  + 'stuff...<p>');

                        for (local i = 1 ; i <= 100 ; ++i)
                            sb.append('<<i>> = <<spellNumber(i)>><br>');

                        req.sendReplyChunk(sb);

                        req.sendReplyChunk(
                            '<p>Okay, that should about do it!<br>'
                            + '</body></html>');

                        req.endChunkedReply();
                    }
                    else
                    {
                        local body =
                            '<html><head>'
                            + '<title>TADS HTTP Server</title>'
                            + '</head><body>'
                            + '<h1>TADS HTPP Server</h1>'
                            + 'This is the server reply! '
                            + 'Here are the request parameters:<br><ul>'
                            + prs.keysToList().mapAll(
                                { k: '<li><<k>>=<<prs[k]>>' }).join()
                            + '</ul><p>And here are the headers:<br><ul>'
                            + hdrs.keysToList().mapAll(
                                { k: '<li><<k>>=<<hdrs[k]>>' }).join()
                            + '</ul><p>And the cookies:<br><ul>'
                            + cookies.keysToList().mapAll(
                                { k: '<li><<k>>=<<cookies[k]>>' }).join()
                            + '</ul>'
                            + '</body></html>';
                        req.sendReply(body, nil, nil,
                                      ['Set-Cookie: TEST=regular-asdf; path=/',
                                       'Set-Cookie: TEST2=regular-jkl; path/']);
                    }
                }
                catch(SocketDisconnectException sde)
                {
                    "Socket reset - peer disconnected\n";
                }
                
                if (req.getQuery() == '/quit')
                {
                    "Got quit request - waiting for server to shut down\n";
                    done = true;
                }
                break;
                
            case NetEvTimeout:
                /* timeout */
                "Timeout in getNetEvent()!\n";
                break;
            }
        }
    }
    finally
    {
        /* shut down the server */
        l.shutdown();
    }
}

spellNumber(n)
{
    /* get the number formatting options */
    local dot = '.', comma = ',';
    
    /* if it's a BigNumber with a fractional part, write as digits */
    if (dataType(n) == TypeObject
        && n.ofKind(BigNumber)
        && n.getFraction() != 0)
    {
        /* 
         *   format it, and convert decimals and group separators per the
         *   options 
         */
        local f = { m, i, s: m == '.' ? dot : comma };
        return rexReplace('<.|,>',
                          n.formatString(n.getPrecision(), BignumCommas),
                          f, ReplaceAll);
    }

    /* if it's less than zero, use "minus seven" or "-123" */
    if (n < 0)
    {
        /* get the spelled version of the absolute value */
        local s = spellNumber(-n);

        /* if it has any letters, use "minus", otherwise "-" */
        return (rexSearch('<alpha>', s) != nil ? 'minus ' : '-') + s;
    }

    /* spell anything less than 100 */
    if (n < 100)
    {
        if (n < 20)
            return ['zero', 'one', 'two', 'three', 'four', 'five', 'six',
                    'seven', 'eight', 'nine', 'ten', 'eleven', 'twelve',
                    'thirteen', 'fourteen', 'fifteen', 'sixteen',
                    'seventeen', 'eighteen', 'nineteen'][n+1];
        else
            return ['twenty', 'thirty', 'forty', 'fifty', 'sixty',
                    'seventy', 'eighty', 'ninety'][n/10-1]
            + ['', '-one', '-two', '-three', '-four', '-five', '-six',
               '-seven', '-eight', '-nine'][n%10 + 1];
    }

    /* 
     *   if it's a small (<100) multiple of 100, 1,000, 1,000,000, or
     *   1,000,000,000, spell it out 
     */
    if (n % 1000000000 == 0 && n/1000000000 < 100)
        return '<<spellNumber(n/1000000000)>> billion';
    if (n % 1000000 == 0 && n/1000000 < 100)
        return '<<spellNumber(n/1000000)>> million';
    if (n % 1000 == 0 && n/1000 < 100)
        return '<<spellNumber(n/1000)>> thousand';
    if (n % 100 == 0 && n/100 < 100)
        return '<<spellNumber(n/100)>> hundred';

    /*
     *   check to see if it can be expressed as a number of millions or
     *   billions with one digit after the decimal place ("7.8 million") 
     */
    if (n % 1000000000 == 0 && n/1000000000 < 1000)
        return '<<n/1000000000>> billion';
    if (n % 100000000 == 0 && n/1000000000 < 1000)
        return '<<n/1000000000>><<dot>><<n%1000000000 / 100000000>> billion';
    if (n % 1000000 == 0 && n/1000000 < 1000)
        return '<<n/1000000>> million';
    if (n % 100000 == 0 && n/1000000 < 1000)
        return '<<n/1000000>><<dot>><<n%1000000 / 100000>> million';

    /* convert to digits */
    local s = toString(n);

    /* insert commas at the thousands */
    for (local i = s.length() - 2 ; i > 1 ; i -= 3)
        s = s.splice(i, 0, comma);

    /* return the result */
    return s;
}
