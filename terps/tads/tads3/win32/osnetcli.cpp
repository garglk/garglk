/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osnetcli.cpp - Windows implemention of HTTP client requests
Function

Notes

Modified
  04/10/10 MJRoberts  - Creation
*/


#include "t3std.h"
#include "os.h"
#include "osifcnet.h"
#include "vmnet.h"
#include "vmisaac.h"
#include "vmfile.h"


/* ------------------------------------------------------------------------ */
/*
 *   HTTP client 
 */

/* ISAAC context for generating random multipart boundary strings */
static isaacctx isaac;
static int isaac_inited = FALSE;

/*
 *   Send an HTTP request
 */
int OS_HttpClient::request(int opts,
                           const char *host, unsigned short portno,
                           const char *verb, const char *resource,
                           const char *send_headers, size_t send_headers_len,
                           OS_HttpPayload *payload, CVmStream *reply,
                           char **reply_headers, char **location,
                           const char *ua)
{
    HINTERNET hconn = 0;
    HINTERNET hsession = 0;
    HINTERNET hreq = 0;
    int ok = FALSE;
    static const char *accept_types[] = {
        "text/*",
        "image/*",
        "application/octet-stream",
        0
    };
    char *gen_headers = 0;
    size_t gen_hdrlen = 0;
    DWORD flags;
    char *putdata = 0;
    size_t putlen = 0;
    int part_cnt = 0;
    char **part_hdrs = 0;
    int status = ErrOther;

    /* check for custom headers from the caller */
    if (send_headers != 0)
    {
        /* remove any trailing \r\n from the caller's headers */
        while (send_headers_len >= 2
               && send_headers[send_headers_len - 2] == '\r'
               && send_headers[send_headers_len - 1] == '\n')
            send_headers_len -= 2;
    }
    else
    {
        /* no custom headers from the caller - use an empty string */
        send_headers = "";
        send_headers_len = 0;
    }

    /* apply our default UA string if we don't have one */
    if (ua == 0 || ua[0] == '\0')
        ua = "TadsWinHttpClient/001";

    /* presume we won't return any headers */
    if (reply_headers != 0)
        *reply_headers = 0;

    /* open an internet connection */
    hconn = InternetOpen(ua, INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0);
    if (hconn == 0)
    {
        status = ErrNet;
        goto done;
    }

    /* connect to the server */
    hsession = InternetConnect(
        hconn, host, portno, "", "", INTERNET_SERVICE_HTTP, 0, 0);
    if (hsession == 0)
    {
        status = ErrNoConn;
        goto done;
    }

    /* set up our base flags */
    flags = INTERNET_FLAG_NO_UI | INTERNET_FLAG_RELOAD
            | INTERNET_FLAG_NO_CACHE_WRITE;

    /* add the "no auto redirect" flag if desired */
    if (location != 0)
    {
        flags |= INTERNET_FLAG_NO_AUTO_REDIRECT;
        *location = 0;
    }

    /* add the "secure sockets" flag if we're using https */
    if ((opts & OptHTTPS) != 0)
        flags |= INTERNET_FLAG_SECURE;

    /* if there's a payload, build the send buffer */
    if (payload != 0 && payload->count_items() > 0)
    {
        /* get the nubmer of payload items */
        part_cnt = payload->count_items();

        /* presume we're not going to do special POST encoding */
        int post_encoding = FALSE;
        
        /* check the verb */
        if (stricmp(verb, "POST") == 0)
        {
            /*
             *   For POST, we're sending form data, but there are three
             *   different formats for form data:
             *   
             *   - If the payload consists of a single file item with an
             *   empty string as the name, it means that the caller has
             *   provided a pre-encoded content body that we should send
             *   exactly as given, with no further format encoding.
             *   
             *   - If the payload consists entirely of simple name/value
             *   fields, send in the application/x-www-url-encoded format.
             *   
             *   - If we have any file uploads among the form fields, send as
             *   multipart/form-data.
             */
            if (payload->count_items() == 1
                && payload->get(0)->name[0] == '\0')
            {
                /* 
                 *   The caller has provided a pre-encoded content body.
                 *   Send it as given - in other words, use the PUT handling.
                 */
            }
            else if (payload->is_multipart())
            {
                /* 
                 *   We have files, so use the multipart/form-data format.
                 *   Note that we're doing special POST encoding.
                 */
                post_encoding = TRUE;

                /*   
                 *   First, generate a boundary string.  It should be
                 *   sufficient to generate a long random string - the
                 *   chances of a long enough random string appearing within
                 *   the data should be negligible.  Initialize our RNG if we
                 *   haven't already done so.  
                 */
                if (!isaac_inited)
                {
                    os_gen_rand_bytes((uchar *)isaac.rsl, sizeof(isaac.rsl));
                    isaac_init(&isaac, TRUE);
                    isaac_inited = TRUE;
                }

                /* generate the boundary string */
                char boundary[128];
                sprintf(boundary, "--%08lx-%08lx-%08lx-%08lx-%08lx-%08lx",
                        isaac_rand(&isaac), isaac_rand(&isaac),
                        isaac_rand(&isaac), isaac_rand(&isaac),
                        isaac_rand(&isaac), GetTickCount());
                size_t boundlen = strlen(boundary);

                /* figure the length: start with the opening boundary */
                putlen = boundlen;

                /* 
                 *   run through the items: for each item, build its item
                 *   header, and figure its contribution to the data length 
                 */
                part_hdrs = new char*[part_cnt];
                int i;
                for (i = 0 ; i < part_cnt ; ++i)
                {
                    /* get this item */
                    OS_HttpPayloadItem *item = payload->get(i);

                    /* check to see which kind of item we have */
                    if (item->stream != 0)
                    {
                        /* file item */
                        part_hdrs[i] = t3sprintf_alloc(
                            "\r\n"
                            "Content-disposition: form-data; "
                            "name=\"%s\"; filename=\"%s\"\r\n"
                            "Content-type: %s\r\n"
                            "Content-transfer-encoding: %s\r\n"
                            "\r\n",
                            item->name,
                            item->val,
                            item->mime_type,
                            memicmp(item->mime_type, "text/", 5) == 0
                            ? "8bit" : "binary");

                        /* add the stream length */
                        putlen += item->stream->get_len();
                    }
                    else
                    {
                        /* ordinary field item */
                        part_hdrs[i] = t3sprintf_alloc(
                            "\r\n"
                            "Content-disposition: form-data; name=\"%s\"\r\n"
                            "Content-type: text/plain; encoding=utf-8\r\n"
                            "\r\n",
                            item->name);

                        /* add the length of the value */
                        putlen += strlen(item->val);
                    }

                    /* if we couldn't allocate a part header, give up */
                    if (part_hdrs[i] == 0)
                    {
                        status = ErrNoMem;
                        goto done;
                    }

                    /* add this item's header length */
                    putlen += strlen(part_hdrs[i]);

                    /* add the CR-LF and boundary at the end of the item */
                    putlen += 2 + boundlen;
                }

                /* add the final "--\r\n" boundary ending */
                putlen += 4;

                /* allocate the buffer */
                putdata = (char *)t3malloc(putlen);
                if (putdata == 0)
                {
                    status = ErrNoMem;
                    goto done;
                }

                /* build the buffer - start with the opening boundary */
                char *p = putdata;
                memcpy(p, boundary, boundlen);
                p += boundlen;

                /* add each item */
                for (i = 0 ; i < part_cnt ; ++i)
                {
                    /* get this item */
                    OS_HttpPayloadItem *item = payload->get(i);

                    /* add the header */
                    strcpy(p, part_hdrs[i]);
                    p += strlen(p);

                    /* add the content */
                    if (item->stream != 0)
                    {
                        err_try
                        {
                            /* copy the entire stream into the buffer */
                            size_t l = item->stream->get_len();
                            item->stream->read_bytes(p, l);

                            /* skip past it */
                            p += l;
                        }
                        err_catch(exc)
                        {
                            status = ErrReadFile;
                            goto done;
                        }
                        err_end;
                    }
                    else
                    {
                        /* copy the value into the buffer */
                        strcpy(p, item->val);
                        p += strlen(p);
                    }

                    /* add the trailing CR-LF and boundary */
                    memcpy(p, "\r\n", 2);
                    memcpy(p + 2, boundary, boundlen);
                    p += 2 + boundlen;
                }

                /* add the "--\r\n" at the end of the last boundary */
                memcpy(p, "--\r\n", 4);
                p += 4;

                /* prepare the content type header */
                gen_headers = t3sprintf_alloc(
                    "Content-type: multipart/form-data; boundary=%s\r\n"
                    "Content-length: %d\r\n",
                    boundary + 2, putlen);
            }
            else
            {
                /* 
                 *   No files are involved, so send in x-www-url-encoded
                 *   format.  Note that we're doing special POST encoding.
                 */
                post_encoding = TRUE;

                /* URL-encode the data */
                putdata = payload->urlencode(putlen);

                /* prepare the content type header */
                gen_headers = t3sprintf_alloc(
                    "Content-type: application/x-www-form-urlencoded\r\n"
                    "Content-length: %d\r\n",
                    putlen);
            }
        }

        /* if we didn't do special POST encoding, prepare the content body */
        if (!post_encoding)
        {
            /* 
             *   for any verb other than POST, we can only send a single file
             *   item - verify that that's what we have 
             */
            OS_HttpPayloadItem *f;
            if (part_cnt > 1 || (f = payload->get(0))->stream == 0)
            {
                status = ErrParams;
                goto done;
            }

            /* build the headers for the request to send the file item */
            gen_headers = t3sprintf_alloc(
                "Content-type: %s\r\n"
                "Content-length: %lu\r\n",
                f->mime_type, f->stream->get_len());

            /* allocate the put buffer */
            putlen = f->stream->get_len();
            putdata = (char *)t3malloc(putlen);
            if (putdata == 0)
            {
                status = ErrNoMem;
                goto done;
            }

            /* read the stream into the buffer */
            err_try
            {
                f->stream->read_bytes(putdata, putlen);
            }
            err_catch(exc)
            {
                status = ErrReadFile;
                goto done;
            }
            err_end;
        }
    }

    /* add the custom headers, if provided */
    if (send_headers_len != 0)
    {
        /* add the custom headers to our generated headers, if any */
        char *new_headers = t3sprintf_alloc(
            "%s%.*s\r\n",
            gen_headers != 0 ? gen_headers : "",
            (int)send_headers_len, send_headers);

        /* 
         *   if we generated our own headers, we're done with those now, as
         *   we've just copied those into the full header string 
         */
        if (gen_headers != 0)
            t3free(gen_headers);

        /* use the full combined generated + custom header string  */
        gen_headers = new_headers;
    }

    /* if we have headers, calculate the length */
    if (gen_headers != 0)
        gen_hdrlen = strlen(gen_headers);

    /* open the request */
    hreq = HttpOpenRequest(
        hsession, verb, resource, 0, 0, accept_types, flags, 0);
    if (hreq == 0)
    {
        status = ErrNoConn;
        goto done;
    }

    /* send the request */
    if (!HttpSendRequest(hreq, gen_headers, gen_hdrlen, putdata, putlen))
    {
        status = ErrNoConn;
        goto done;
    }

    /* get the status code */
    DWORD http_status, clen, siz = sizeof(http_status);
    if (!HttpQueryInfo(hreq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                       &http_status, &siz, 0))
    {
        status = ErrNet;
        goto done;
    }

    /* use the HTTP status as the return status */
    status = (int)http_status;

    /* if the caller wants the headers, return them */
    if (reply_headers != 0)
    {
        /* 
         *   first, do the query without a buffer, to see how much space we
         *   need to allocate 
         */
        DWORD sz = 0;
        if (!HttpQueryInfo(hreq, HTTP_QUERY_RAW_HEADERS_CRLF, 0, &sz, 0)
            && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            /* allocate the buffer */
            *reply_headers = new char[sz + 1];

            /* retrieve the headers */
            if (HttpQueryInfo(hreq, HTTP_QUERY_RAW_HEADERS_CRLF,
                              *reply_headers, &sz, 0))
            {
                /* success - null-terminate the buffer */
                (*reply_headers)[sz] = '\0';
            }
            else
            {
                /* failed - discard the headers */
                delete [] *reply_headers;
                status = ErrGetHdrs;
                goto done;
            }
        }
    }

    /* make sure we got a success code or (if allowed) a redirect */
    if (!(status == 200
          || (status == 301 && location != 0)))
        goto done;

    /* if it's a redirect, pass back the location */
    if (status == 301)
    {
        /* query the Location header from the response */
        char lbuf[4096];
        siz = sizeof(lbuf);
        if (!HttpQueryInfo(hreq, HTTP_QUERY_LOCATION, lbuf, &siz, 0))
        {
            status = ErrNet;
            goto done;
        }

        /* pass it back to our caller as an allocated string */
        *location = lib_copy_str(lbuf, (size_t)siz);
    }

    /* get the content length */
    int varclen = FALSE;
    siz = sizeof(clen);
    if (!HttpQueryInfo(hreq,
                       HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                       &clen, &siz, 0) || clen == 0)
    {
        /* no explicit content length */
        clen = 0;
        varclen = TRUE;
    }

    /* read the results */
    err_try
    {
        for (;;)
        {
            DWORD bytes_read;
            char buf[1024];
            
            /* read the next chunk - abort on error */
            DWORD cur = (clen > sizeof(buf) || varclen ? sizeof(buf) : clen);
            if (!InternetReadFile(hreq, buf, cur, &bytes_read))
                goto done;
            
            /* if that's EOF, we're done */
            if (bytes_read == 0)
                break;
            
            /* store the current chunk in the output stream, if we have one */
            if (reply != 0)
                reply->write_bytes(buf, (size_t)bytes_read);
            
            /* deduct the amount read from the remaining amount, if known */
            if (!varclen)
                clen -= bytes_read;
        }
    }
    err_catch (exc)
    {
        /* failed to write to the reply object */
        status = ErrWriteFile;
        goto done;
    }
    err_end;

    /* success */
    ok = TRUE;

done:
    /* clean up data buffers for the sent data */
    if (putdata != 0)
        t3free(putdata);
    if (gen_headers != 0)
        t3free(gen_headers);
    if (part_hdrs != 0)
    {
        for (int i = 0 ; i < part_cnt ; ++i)
        {
            if (part_hdrs[0] != 0)
                t3free(part_hdrs[i]);
        }
        delete [] part_hdrs;
    }

    /* clean up the connection */
    if (hreq != 0)
        InternetCloseHandle(hreq);
    if (hsession != 0)
        InternetCloseHandle(hsession);
    if (hconn != 0)
        InternetCloseHandle(hconn);

    /* return the status code */
    return status;
}


