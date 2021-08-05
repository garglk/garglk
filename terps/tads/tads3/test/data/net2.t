#include <tads.h>
#include <tadsnet.h>
#include <httpsrv.h>
#include <httpreq.h>
#include <strbuf.h>
#include <file.h>

loadFile(fname)
{
    /* file extension to MIME-type map */
    local mimeMap = [
        'htm'  -> 'text/html',
        'html' -> 'text/html',
        'txt'  -> 'text/plain',
        'jpg'  -> 'image/jpeg',
        'jpeg' -> 'image/jpeg',
        'png'  -> 'image/png',
        *      -> 'application/octet-stream'];

    try
    {
        /* get the extension */
        local ext = '';

        if (rexMatch('.*<dot>([a-z0-9_]+)$', fname))
            ext = rexGroup(1)[3];
        
        /* look up the MIME type for this extension */
        local mime = mimeMap[ext];
        
        /* read the text or binary file */
        local f, body;
        if (mime.startsWith('text/'))
        {
            /* read the text file */
            f = File.openTextFile(fname, FileAccessRead);
            
            /* read the file into a StringBuffer */
            body = new StringBuffer(f.getFileSize());
            for (;;)
            {
                /* read the next line; stop if done */
                local t = f.readFile();
                if (t == nil)
                    break;
                
                /* add it to the string buffer */
                body.append(t);
            }
        }
        else
        {
            /* read the raw binary file */
            f = File.openRawFile(fname, FileAccessRead);
            
            /* read it in */
            body = new ByteArray(f.getFileSize());
            f.readBytes(body, 1, f.getFileSize());
        }
        
        /* done with the file */
        f.closeFile();

        /* return the file information */
        return new FileInfo(body, mime);
    }
    catch (FileException fex)
    {
        /* couldn't open the file - return nil */
        "error opening <<fname>>: <<fex.displayException()>>\n";
        return nil;
    }
}

class FileInfo: object
    construct(body, mime)
    {
        self.body = body;
        self.mimeType = mime;
    }

    body = nil
    mimeType = nil
;

main(args)
{
    local host;
    if (args.length() >= 2 && args.sublist(2).indexOf('-local') != nil)
        host = 'localhost';
    else
        host = getLocalIP();

    /* set up the server */
    local l = new HTTPServer(host, nil, 65535);
    "HTTP server listening on <<host>>:<<l.getPortNum()>><br>";

    /* no uploads yet */
    local uploads = new LookupTable();

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
                local flds = req.getFormFields();

                // local addr = req.getClientAddress();
                // "Got request: res=<<res>>, verb=<<req.getVerb()>>,
                //   from=<<addr[1]>>:<<addr[2]>><br>";

                if (req.getVerb() == 'GET' && res.startsWith('uploads/'))
                {
                    // "Serving uploads/ request for <<res.substr(9)>>\n";
                    local f = uploads[res.substr(9)];
                    if (f != nil)
                        req.sendReply(f.file);
                    else
                        req.sendReply('File not found', 'text/plain', 404);
                    
                    continue;
                }

                if (req.getVerb() == 'POST')
                {
                    foreach (local f in flds)
                    {
                        if (dataType(f) == TypeObject && f.ofKind(FileUpload))
                            uploads[f.filename] = f;
                    }
                    
                    local body = req.getBody();
                    if (body == nil)
                    {
                        "No body in PUT<br>";
                    }
                    else if (body.getFileMode() == FileModeRaw)
                    {
                        // "Binary message body in PUT<br>";
                    }
                    else
                    {
                        "POST message body:<br><hr>";
                        for (;;)
                        {
                            local l = body.readFile();
                            if (l == nil)
                                break;
                            tadsSay(l.htmlify());
                        }
                        "<hr><br>";
                    }
                }

                /* if this is the special "QUIT!" request, shut down */
                if (res == 'QUIT!')
                {
                    req.sendReply('QUIT! request received - shutting down');
                    break;
                }
                
                /* try opening the file */
                local f = loadFile(res);
                if (f != nil)
                {
                    if (rexMatch('([^/]*/)*test-upload.htm$', res))
                    {
                        f.body = toString(f.body);
                        
                        f.body = f.body.findReplace(
                            ['value.username', 'value.email'],
                            ['value="Mike!"', 'value="mjr!@hotmail.com"']);

                        if (flds != nil)
                        {
                            f.body += '<p>Form fields:<br><ul>'
                                + flds.keysToList().mapAll({
                                    k: '<li><<k>>=<<descField(flds[k])>>' })
                                .join()
                                + '</ul>';
                        }

                        f.body += '<p>Uploads available:<br><ul>'
                            + uploads.keysToList().mapAll(new function(k) {
                            local f = uploads[k];
                            if (f.contentType.startsWith('image/'))
                                return '<li><a href="/uploads/<<k>>">'
                                + '<img src="/uploads/<<k>>" border=0><br>'
                                + '<<k.htmlify()>></a>';
                            else
                                return '<li><a href="/uploads/<<k>>">'
                                + '<<k.htmlify()>></a>';
                        }).join() + '</ul>';
                    }
                    req.sendReply(f.body, f.mimeType);
                }
                else
                    req.sendReply('FNF File not found', 'text/plain', 404);
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
    l.shutdown(true);
}

descField(fld)
{
    if (dataType(fld) == TypeSString)
        return fld;
    else if (dataType(fld) == TypeObject && fld.ofKind(FileUpload))
        return 'File(name=<<fld.filename>>, content-type=<<fld.contentType>>)';
    else
        return 'Unknown field type';
}

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


