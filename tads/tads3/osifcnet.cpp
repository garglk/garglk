#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osifcnet.cpp - portable implementation for OS network interface
Function
  
Notes
  
Modified
  08/19/10 MJRoberts  - Creation
*/

#include "t3std.h"
#include "osifcnet.h"
#include "vmtype.h"
#include "vmdatasrc.h"

/* ------------------------------------------------------------------------ */
/*
 *   Payload item 
 */

/* payload item destruction */
OS_HttpPayloadItem::~OS_HttpPayloadItem()
{
    lib_free_str(name);
    lib_free_str(val);
    lib_free_str(mime_type);
    if (stream != 0)
        delete stream;
}

/* create a payload item consisting of a simple name/value pair */
OS_HttpPayloadItem::OS_HttpPayloadItem(const char *name, const char *val)
{
    init();
    this->name = lib_copy_str(name);
    this->val = lib_copy_str(val != 0 ? val : "");
}

/* create a payload item consisting of a simple name/value pair */
OS_HttpPayloadItem::OS_HttpPayloadItem(const char *name, size_t name_len,
                                       const char *val, size_t val_len)
{
    init();
    this->name = lib_copy_str(name, name_len);
    this->val = lib_copy_str(val, val_len);
}

/* create a payload item for a PUT/POST file upload */
OS_HttpPayloadItem::OS_HttpPayloadItem(
    const char *name, const char *filename,
    const char *mime_type, CVmDataSource *contents)
{
    init();
    this->name = lib_copy_str(name);
    this->val = lib_copy_str(filename != 0 ? filename : "");
    this->mime_type = lib_copy_str(mime_type != 0 ? mime_type : "");
    this->stream = contents;
}

/* create a payload item for a PUT/POST file upload */
OS_HttpPayloadItem::OS_HttpPayloadItem(
    const char *name, size_t name_len,
    const char *filename, size_t filename_len,
    const char *mime_type, size_t mime_type_len,
    CVmDataSource *contents)
{
    init();
    this->name = lib_copy_str(name, name_len);
    this->val = lib_copy_str(filename != 0 ? filename : "", filename_len);
    this->mime_type = lib_copy_str(
        mime_type != 0 ? mime_type : "", mime_type_len);
    this->stream = contents;
}

/* initialize - clear the fields */
void OS_HttpPayloadItem::init()
{
    name = 0;
    val = 0;
    stream = 0;
    mime_type = 0;
    nxt = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Request payload data 
 */


/* construction */
OS_HttpPayload::OS_HttpPayload()
{
    /* clear the item list initially */
    first_item = last_item = 0;
}

/* destruction */
OS_HttpPayload::~OS_HttpPayload()
{
    /* delete our item list */
    OS_HttpPayloadItem *cur, *nxt;
    for (cur = first_item ; cur != 0 ; cur = nxt)
    {
        nxt = cur->nxt;
        delete cur;
    }
}

/* add a form field */
void OS_HttpPayload::add(const char *name, const char *val)
{
    add(new OS_HttpPayloadItem(name, val));
}

/* add a form field in TADS-string (VMB_LEN prefixed) format */
void OS_HttpPayload::add_tstr(const char *name, const char *val)
{
    add(new OS_HttpPayloadItem(name + VMB_LEN, vmb_get_len(name),
                               val + VMB_LEN, vmb_get_len(val)));
}

/* add a form file upload */
void OS_HttpPayload::add(const char *name, const char *filename,
                         const char *mime_type, CVmDataSource *stream)
{
    add(new OS_HttpPayloadItem(name, filename, mime_type, stream));
}

/* add a form file upload */
void OS_HttpPayload::add(const char *name, size_t name_len,
                         const char *filename, size_t filename_len,
                         const char *mime_type, size_t mime_type_len,
                         CVmDataSource *stream)
{
    add(new OS_HttpPayloadItem(
        name, name_len, filename, filename_len,
        mime_type, mime_type_len, stream));
}

/* add an item */
void OS_HttpPayload::add(OS_HttpPayloadItem *item)
{
    if (last_item != 0)
        last_item->nxt = item;
    else
        first_item = item;
    last_item = item;
}

/* get the number of items */
int OS_HttpPayload::count_items() const
{
    /* run through the list and count the items */
    int cnt = 0;
    for (OS_HttpPayloadItem *cur = first_item ; cur != 0 ; cur = cur->nxt)
        ++cnt;

    /* return the number of items */
    return cnt;
}

/* get the nth item */
OS_HttpPayloadItem *OS_HttpPayload::get(int n) const
{
    /* scan into the list by 'n' items */
    OS_HttpPayloadItem *cur;
    for (cur = first_item ; cur != 0 && n != 0 ; cur = cur->nxt, --n) ;

    /* return the item where we stopped */
    return cur;
}

/* 
 *   Does the payload contain any multipart file data?  This returns true if
 *   any of the fields are uploaded files, false if they're all simple form
 *   fields.  
 */
int OS_HttpPayload::is_multipart() const
{
    /* check to see if any of the payload parts are files */
    for (OS_HttpPayloadItem *cur = first_item ; cur != 0 ; cur = cur->nxt)
    {
        /* if this is a file item, we have a multipart payload */
        if (cur->stream != 0)
            return TRUE;
    }

    /* we didn't find any file uploads, so it's a simple form */
    return FALSE;
}

/* 
 *   URL-encode a string into a buffer; returns the required length, not
 *   counting the terminating null byte.  If the buffer is null, we'll simply
 *   calculate and return the length.  
 */
static size_t urlencodestr(char *dst, const char *str)
{
    size_t len;
    for (len = 0 ; *str != '\0' ; ++str)
    {
        switch (*str)
        {
        case '!':
        case '*':
        case '\'':
        case '(':
        case ')':
        case ';':
        case ':':
        case '@':
        case '&':
        case '=':
        case '+':
        case '$':
        case ',':
        case '/':
        case '?':
        case '#':
        case '[':
        case ']':
            /* encode these as % sequences */
            len += 3;
            if (dst != 0)
            {
                sprintf(dst, "%%%02x", (int)*str);
                dst += 3;
            }
            break;

        case ' ':
            /* encode space as '+' */
            ++len;
            if (dst != 0)
                *dst++ = '+';
            break;

        default:
            /* encode anything else as-is */
            ++len;
            if (dst != 0)
                *dst++ = *str;
            break;
        }
    }

    /* add a null terminator (but don't count it in the output length) */
    if (dst != 0)
        *dst = '\0';

    /* return the calculated length */
    return len;
}


/*
 *   Create an application/x-www-form-urlencoded buffer from the payload. 
 */
char *OS_HttpPayload::urlencode(size_t &len) const
{
    /* 
     *   start by scanning the fields to figure the total buffer length we'll
     *   need 
     */
    OS_HttpPayloadItem *item;
    for (len = 0, item = first_item ; item != 0 ; item = item->nxt)
    {
        /* 
         *   if this isn't the first item, we'll need a '&' to separate it
         *   from the prior item 
         */
        if (item != first_item)
            len += 1;
        
        /* 
         *   add the item's length: "name=value", with url encoding for both
         *   strings 
         */
        len += urlencodestr(0, item->name) + 1 + urlencodestr(0, item->val);
    }
    
    /* allocate the buffer */
    char *putdata = (char *)t3malloc(len + 1);
    if (putdata == 0)
        return 0;
    
    /* build the buffer */
    char *p;
    for (p = putdata, item = first_item ; item != 0 ; item = item->nxt)
    {
        /* if this isn't the first item, add the '&' */
        if (item != first_item)
            *p++ = '&';
        
        /* add "name=urlencodestr(value)" */
        p += urlencodestr(p, item->name);
        *p++ = '=';
        p += urlencodestr(p, item->val);
    }

    /* return the allocated buffer */
    return putdata;
}
