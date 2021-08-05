#include <tads.h>
#include <tadsnet.h>
#include <httpsrv.h>
#include <httpreq.h>
#include <strbuf.h>
#include <file.h>

/* set up a handler for our custom resources, in the net4res/ directory  */
WebResourceResFile
    path = static new RexPattern('^/net4res/')
;

/* the main command window */
cmdwin: WebCommandWin
;

/* the main status window */
statwin: WebStatusWin
;

/*
 *   Main entrypoint 
 */
main(args)
{
    randomize();
    
    local host;
    if (args.length() >= 2 && args.sublist(2).indexOf('-local') != nil)
        host = 'localhost';
    else
        host = getLocalIP();

    /* set up the server */
    local srv = new HTTPServer(host, nil, 65535);
    "HTTP server listening on <<host>>:<<srv.getPortNum()>><br>";

    /* connect to the user interface */
    webMainWin.connect(srv);

    /* set up the command window and status line */
    webMainWin.createFrame(cmdwin, 'cmd', '0, stat.bottom, 100%, 100%');
    webMainWin.createFrame(statwin, 'stat', '0, 0, 100%, content.height');

    /* set the default output function to write to the command window */
    t3SetSay({txt: cmdwin.write(toString(txt))});

    statwin.setStatus('Welcome');
    statwin.resize();
    
    local turnCount = 0;
    "http://<<host>>:<<srv.getPortNum()>>/?TADS_session=<<webSession.key>><p>";
    "Hello! This is our first test! We shall see how it goes!";
    for (;;)
    {
        "<p>Please enter your name:<br><nobr>&gt;";
        statwin.setStatus('Name Entry', turnCount++);
        local txt = cmdwin.getInputLine();
        "</nobr>";

        "<p>Well, it looks like you entered <b><<txt.htmlify()>></b>!
        If this is correct, please press 1.  If not, press 2.
        <br><nobr>&gt;";

        statwin.setStatus('Confirmation', turnCount++);
        local n = cmdwin.getInputLine();
        "</nobr>";
        if (n == '1')
        {
            "<p>Great!  We\'ll call that a day, then!<br>";
            break;
        }
        else if (n != '2')
        {
            "<p>You didn\'t press 1 or 2, but we\'ll assume you
            need to re-enter your name.";
        }
    }
    
    /* done - shut down the server */
    "<p>Shutting down the server...<br>";
    eventPage.sendEvent('<shutdown/>');
    ClientSession.shutdownWait();

    //t3SetSay(&tadsSay);
    local x = nil;
    x.sendEvent();
    "Shutting down the HTTP server...<br>";
    srv.shutdown(true);
}

