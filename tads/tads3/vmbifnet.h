/* Copyright (c) 2005 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmbifnet.h - function set definition - TADS Networking
Function
  The networking function set provides access to the HTTP server feature,
  which allows running the UI in a web browser.
Notes
  
Modified
  03/02/05 MJRoberts  - Creation
*/

#ifndef VMBIFNET_H
#define VMBIFNET_H

#include "os.h"
#include "vmbif.h"
#include "utf8.h"

class CVmBifNet: public CVmBif
{
public:
    /* function vector */
    static vm_bif_desc bif_table[];

    /* attach/detach - static initialization and cleanup */
    static void attach(VMG0_);
    static void detach(VMG0_);

    /* connect to the web UI */
    static void connect_ui(VMG_ uint argc);

    /* get a network event */
    static void get_event(VMG_ uint argc);

    /* get the local host name */
    static void get_hostname(VMG_ uint argc);

    /* get the local host IP address */
    static void get_host_ip(VMG_ uint argc);

    /* get the storage server URL */
    static void get_storage_url(VMG_ uint argc);

    /* get the launch host address */
    static void get_launch_host_addr(VMG_ uint argc);

    /* send a network request */
    static void send_net_request(VMG_ uint argc);
};


#endif /* VMBIFNET_H */

/* ------------------------------------------------------------------------ */
/*
 *   Function set vector.  Define this only if VMBIF_DEFINE_VECTOR has been
 *   defined, so that this file can be included for the prototypes alone
 *   without defining the function vector.
 *   
 *   Note that this vector is specifically defined outside of the section of
 *   the file protected against multiple inclusion.  
 */
#ifdef VMBIF_DEFINE_VECTOR

/* TADS input/output extension functions */
vm_bif_desc CVmBifNet::bif_table[] =
{
    { &CVmBifNet::connect_ui, 2, 0, FALSE },
    { &CVmBifNet::get_event, 0, 1, FALSE },
    { &CVmBifNet::get_hostname, 0, 0, FALSE },
    { &CVmBifNet::get_host_ip, 0, 0, FALSE },
    { &CVmBifNet::get_storage_url, 0, 0, FALSE },
    { &CVmBifNet::get_launch_host_addr, 0, 0, FALSE },
    { &CVmBifNet::send_net_request, 2, 0, TRUE }
};

#endif /* VMBIF_DEFINE_VECTOR */
