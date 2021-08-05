#include <tads.h>
#include <tadsnet.h>
#include <httpsrv.h>
#include <httpreq.h>

main(args)
{
    // start the server
    local server = new HTTPServer(getHostName(), nil);
    "HTTP Server listening on port <<server.getPortNum()>>\n";

    // process requests
    for (;;)
    {
        local evt = getNetEvent();
        if (evt.evType == NetEvRequest && evt.evRequest.ofKind(HTTPRequest))
        {
            // send the reply
            evt.evRequest.sendReply('It worked! You asked for <<
               evt.evRequest.getQuery()>>.', 'text/plain', 200);
        }
    }

    // shut down the server
    server.shutdown();
}
