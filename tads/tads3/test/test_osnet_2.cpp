#include <stdio.h>
#include <stdlib.h>

#include "t3std.h"
#include "os.h"
#include "osifc.h"
#include "osifcnet.h"
#include "vmnet.h"
#include "vmrun.h"


/* dummy entrypoint for HTTPRequest intrinsic class construction */
vm_obj_id_t TadsHttpRequest::prep_event_obj(VMG_ int *, int *)
{
    return VM_INVALID_OBJ;
}

/* dummy entrypoints for exception throwing from vmnet.cpp */
void CVmRun::push_string(VMG_ const char *, size_t) { }
void CVmRun::push_stringf(VMG_ const char *, ...) { }
void CVmRun::throw_new_rtesub(VMG_ vm_obj_id_t, uint, int) { }


/* main program entrypoint */
int main(int argc, char **argv)
{
    /* initialize the network/thread package */
    os_net_init(0);

    /* create the incoming request queue */
    TadsMessageQueue *req_queue = new TadsMessageQueue(0);

    /* create the http listener */
    TadsHttpListenerThread *ht = new TadsHttpListenerThread(
        VM_INVALID_OBJ, req_queue, 65535);
    TadsListener *l = TadsListener::launch("localhost", 0, ht);

    int port;
    char *ip;
    if (ht->get_local_addr(ip, port))
    {
        printf("http listener on %s port %d\n"
               "http://%s:%d/status?psw=%s\n\n",
               ip, port,
               ip, port, ht->get_password());
        lib_free_str(ip);
    }

    /* wait until done */
    printf("Enter commands - Q=quit\n");
    for (;;)
    {
        char buf[128];
        if (fgets(buf, sizeof(buf), stdin) == 0)
            break;

        if (buf[0] == 'q' || buf[0] == 'Q')
            break;

        printf("Unrecognized command\n");
    }

    /* shut down the http listener and wait for it to finish */
    l->shutdown();
    ht->wait();

    /* done with the listener */
    l->release_ref();

    /* clean up the network/thread package */
    os_net_cleanup();
}

