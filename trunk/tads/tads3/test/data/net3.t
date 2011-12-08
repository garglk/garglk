#include <tads.h>
#include <tadsnet.h>
#include <httpsrv.h>
#include <httpreq.h>
#include <strbuf.h>
#include <file.h>

main(args)
{
    local host;
    if (args.length() >= 2 && args.sublist(2).indexOf('-local') != nil)
        host = 'localhost';
    else
        host = getLocalIP();

    /* set up the server */
    local srv = new HTTPServer(host, nil, 65535);
    "HTTP server listening on <<host>>:<<srv.getPortNum()>><br>";

    /* connect */
    connectWebUI(srv, '/ui');

    /* handle network events */
    for (;;)
    {
        try
        {
            /* get the next network event */
            inputEvent(0);
            local evt = getNetEvent();
            
            /* see what we have */
            if (evt.evType == NetEvRequest
                && evt.evRequest.ofKind(HTTPRequest))
            {
                /* 
                 *   HTTP request - get the query string, minus the leading
                 *   slash.  For example, if they asked for
                 *   http://server/x/y, this will give us 'x/y'. 
                 */
                local req = evt.evRequest;
                local res = req.parseQuery()[1].substr(2);

                if (res == 'ui')
                {
                    /* send the main UI */
                    req.sendReply(
                        '<html><title>Test UI</title>'
                        + '<body><h1>Test UI</h1>'
                        + 'If you\'re seeing this, it means that your '
                        + 'TADS Web-based game is working!'
                        + '<p><a href="quit">Shut down server</a><br>'
                        + '</body></html>', 'text/html');
                }
                else if (res == 'quit')
                {
                    req.sendReply('Shutting down server!', 'text/plain');
                    break;
                }
                else 
                {
                    req.sendReply('file not found', 'text/plain', 404);
                }
            }
        }
        catch (SocketDisconnectException sdx)
        {
            /* 
             *   ignore these - they just mean that the client closed its
             *   connection before we could send the reply; their loss 
             */
        }
        catch (NetException nx)
        {
            "<<nx.displayException()>><br>";
        }
    }

    "Shutting down the HTTP server...<br>";
    srv.shutdown(true);
}

