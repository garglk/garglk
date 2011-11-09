#include "os.h"
#include "std.h"

int
main(argc, argv)
        int     argc;
        char    **argv;
{
        int trdmain(/*_ int argc, char *argv[] _*/);
        int err;
        static char buf[128];
        static char *newargv[2];

        if (argc < 2) {
                os_init(&argc, newargv, NULL, buf, 128);
                err = os0main(argc, newargv, trdmain, "", "config.tr");
        }
        else {
                os_init(NULL, NULL, NULL, NULL, 0);
                err = os0main(argc, argv, trdmain, "", "config.tr");
        }
        os_uninit();
        os_term();
        return(err);
}

