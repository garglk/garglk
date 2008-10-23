int 
main(argc, argv)
        int     argc;
        char    *argv[];
{
    int tcdmain(/*_ int argc, char *argv[] _*/);
    
    os_init(&argc, argv, (char *)0, (char *)0, 0);
    os0main(argc, argv, tcdmain, "i", "config.tc");
    os_uninit();
    os_term();
}

