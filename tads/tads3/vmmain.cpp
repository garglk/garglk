#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmmain.cpp - T3 VM main entrypoint - execute an image file
Function
  Main entrypoint for executing a T3 image file.  Loads an image file
  and begins execution.  Returns when execution terminates.
Notes
  
Modified
  10/07/99 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "os.h"
#include "t3std.h"
#include "vmerr.h"
#include "vmfile.h"
#include "vmimage.h"
#include "vmrun.h"
#include "vmimgrb.h"
#include "vmmain.h"
#include "vmhost.h"
#include "vmhostsi.h"
#include "vminit.h"
#include "vmpredef.h"
#include "vmobj.h"
#include "vmvsn.h"
#include "charmap.h"
#include "vmsave.h"
#include "vmtype.h"
#include "vmrunsym.h"
#include "vmmcreg.h"
#include "vmbifreg.h"
#include "vmbiftad.h"
#include "sha2.h"
#include "vmnet.h"


/* ------------------------------------------------------------------------ */
/*
 *   Convert a directory path parameter to an absolute path, and return an
 *   allocated copy of the string.
 */
static char *path_param_to_abs(const char *param)
{
    /* make sure the path is expressed in absolute format */
    char abuf[OSFNMAX];
    os_get_abs_filename(abuf, sizeof(abuf), param);

    /* return a copy of the path */
    return lib_copy_str(abuf);
}

/*
 *   Extract the path from a filename, convert it to an absolute path, and
 *   return an allocated copy of the string. 
 */
static char *file_param_to_abs_path(const char *param)
{
    /* extract the path portion */
    char path[OSFNMAX];
    os_get_path_name(path, sizeof(path), param);

    /* return it in absolute form */
    return path_param_to_abs(path);
}


/* ------------------------------------------------------------------------ */
/*
 *   Execute an image file.  If an exception occurs, we'll display a
 *   message on the console, and we'll return the error code; we'll return
 *   zero on success.  If an error occurs, we'll fill in 'errbuf' with a
 *   message describing the problem.  
 */
int vm_run_image(const vm_run_image_params *params)
{
    CVmFile *fp = 0;
    CVmImageLoader *volatile loader = 0;
    CVmImageFile *volatile imagefp = 0;
    unsigned long image_file_base = 0;
    int retval;
    vm_globals *vmg__;

    /* 
     *   get the full absolute path for the image file, to ensure that future
     *   references are independent of working directory changes 
     */
    os_get_abs_filename(G_os_gamename, sizeof(G_os_gamename),
                        params->image_file_name);

    /* presume we will return success */
    retval = 0;

    /* make sure the local time zone settings are initialized */
    os_tzset();

    /* create the file object */
    fp = new CVmFile();

    /* initialize the VM */
    vm_init_options opts(params->hostifc, params->clientifc,
                         params->charset, params->log_charset);
    vm_initialize(&vmg__, &opts);

    /* catch any errors that occur during loading and running */
    err_try
    {
        /* if we have a resource root path, tell the host interface */
        if (params->res_dir != 0)
            params->hostifc->set_res_dir(params->res_dir);
        
        /* 
         *   Save the default working directory for file operations, if
         *   specified.  If it's not specified, use the image file directory
         *   as the default. 
         */
        if (params->file_dir != 0)
            G_file_path = path_param_to_abs(params->file_dir);
        
        /* set the sandbox directory (for file safety restrictions) */
        if (params->sandbox_dir != 0)
            G_sandbox_path = path_param_to_abs(params->sandbox_dir);
        else
            G_sandbox_path = file_param_to_abs_path(params->image_file_name);
        
        /* set the networking configuration in the globals */
        G_net_config = params->netconfig;
        
        /* tell the client system to initialize */
        params->clientifc->client_init(
            VMGLOB_ADDR,
            params->script_file, params->script_quiet,
            params->log_file, params->cmd_log_file,
            params->show_banner ? T3VM_BANNER_STRING : 0,
            params->more_mode);
        
        /* open the byte-code file */
        if (params->load_from_exe)
        {
            /* find the image within the executable */
            osfildef *exe_fp = os_exeseek(G_os_gamename, "TGAM");
            if (exe_fp == 0)
                err_throw(VMERR_NO_IMAGE_IN_EXE);

            /* 
             *   set up to read from the executable at the location of the
             *   embedded image file that we just found 
             */
            image_file_base = osfpos(exe_fp);
            fp->set_file(exe_fp, image_file_base);
        }
        else
        {
            /* reading from a normal file - open the file */
            fp->open_read(G_os_gamename, OSFTT3IMG);
        }

        /* create the loader */
        imagefp = new CVmImageFileExt(fp);
        loader = new CVmImageLoader(imagefp, G_os_gamename, image_file_base);

        /* load the image */
        loader->load(vmg0_);

        /* let the client prepare for execution */
        params->clientifc->pre_exec(VMGLOB_ADDR);

        /* if desired, seed the random number generator */
        if (params->seed_rand)
            CVmBifTADS::randomize(vmg_ 0);

#ifdef TADSNET
        /* 
         *   if we have a network configuration, get the game's IFID - this
         *   is needed to identify the game to the storage server 
         */
        if (params->netconfig != 0)
        {
            char *ifid = vm_get_ifid(params->hostifc);
            if (ifid != 0)
            {
                params->netconfig->set("ifid", ifid);
                t3free(ifid);
            }
        }
#endif /* TADSNET */

        /* run the program from the main entrypoint */
        loader->run(vmg_ params->prog_argv, params->prog_argc,
                    0, 0, params->saved_state);

        /* tell the client we're done with execution */
        params->clientifc->post_exec(VMGLOB_ADDR);
    }
    err_catch(exc)
    {
        /* 
         *   report anything except DEBUG VM HALT, which is special: it
         *   indicates that the user explicitly terminated execution via the
         *   debugger UI, so there's obviously no need to report an error 
         */
        if (exc->get_error_code() != VMERR_DBG_HALT)
        {
            /* tell the client execution failed due to an error */
            params->clientifc->post_exec_err(VMGLOB_ADDR);
            
            /* note the error code for returning to the caller */
            retval = exc->get_error_code();
            
            /* get the message for the error */
            char errbuf[512];
            CVmRun::get_exc_message(vmg_ exc, errbuf, sizeof(errbuf), TRUE);
            
            /* display the message */
            params->clientifc->display_error(VMGLOB_ADDR, exc, errbuf, FALSE);
        }
    }
    err_end;

    /* done with the file base path and sandbox path */
    lib_free_str(G_file_path);
    lib_free_str(G_sandbox_path);

    /* unload the image */
    if (loader != 0)
        loader->unload(vmg0_);

    /* delete the loader and the image file object */
    if (loader != 0)
        delete loader;
    if (imagefp != 0)
        delete imagefp;

    /* notify the client */
    params->clientifc->client_terminate(VMGLOB_ADDR);

#ifdef TADSNET
    /* 
     *   Delete the network configuration, if we have one.  Note that a
     *   network configuration might have been created dyanmically during
     *   execution even if the caller didn't originally provide one.  
     */
    if (G_net_config != 0)
        delete G_net_config;
#endif

    /* terminate the VM */
    vm_terminate(vmg__, params->clientifc);

    /* delete the file */
    if (fp != 0)
        delete fp;

    /* return the status code */
    return retval;
}

/* ------------------------------------------------------------------------ */
/*
 *   Read an option argument from an option that allows its arguments to be
 *   either appended directly to the end of the option (as in "-otest") or
 *   to follow as the subsequent vector item (as in "-o test").  optlen
 *   gives the length of the option prefix itself, including the hyphen
 *   prefix (so an option "-o" would have length 2).  We'll increment
 *   *curarg if we consume the extra vector position.  Returns null if there
 *   isn't an argument for the option.  
 */
static char *get_opt_arg(int argc, char **argv, int *curarg, int optlen)
{
    /* 
     *   if we have an argument (i.e., any non-empty text) appended directly
     *   to the option vector item, we don't need to consume an extra vector
     *   position 
     */
    if (argv[*curarg][optlen] != '\0')
    {
        /* the argument is merely the remainder of this vector item */
        return &argv[*curarg][optlen];
    }

    /* advance to the next vector position */
    ++(*curarg);

    /* if we don't have any vector items left, there's no argument */
    if (*curarg >= argc)
        return 0;

    /* this vector item is the next argument */
    return argv[*curarg];
}


/* ------------------------------------------------------------------------ */
/*
 *   Create the network configuration object, if we haven't already.
 */
#ifdef TADSNET
static void create_netconfig(
    CVmMainClientIfc *clientifc, TadsNetConfig *&netconfig, char **argv)
{
    if (netconfig == 0)
    {
        /* create the object */
        netconfig = new TadsNetConfig();

        /* build the path to the web config file */
        char confname[OSFNMAX], confpath[OSFNMAX];
        os_get_special_path(confpath, sizeof(confpath), argv[0],
                            OS_GSP_T3_SYSCONFIG);
        os_build_full_path(confname, sizeof(confname),
                           confpath, "tadsweb.config");
        
        /* if there's a net configuration file, read it */
        osfildef *fp = osfoprb(confname, OSFTTEXT);
        if (fp != 0)
        {
            /* read the file into the configuration object */
            netconfig->read(fp, clientifc);
            
            /* done with the file */
            osfcls(fp);
        }
        else
        {
            /* warn that the file is missing */
            char *msg = t3sprintf_alloc(
                "Warning: -webhost mode specified, but couldn't "
                "read web configuration file \"%s\"", confname);
            clientifc->display_error(0, 0, msg, FALSE);
            t3free(msg);
        }
    }
}
#endif /* TADSNET */

/* ------------------------------------------------------------------------ */
/*
 *   Main Entrypoint for command-line invocations.  For simplicity, a
 *   normal C main() or equivalent entrypoint can invoke this routine
 *   directly, using the usual argc/argv conventions.
 *   
 *   Returns a status code suitable for use with exit(): OSEXSUCC if we
 *   successfully loaded and ran an executable, OSEXFAIL on failure.  If
 *   an error occurs, we'll fill in 'errbuf' with a message describing the
 *   problem.  
 */
int vm_run_image_main(CVmMainClientIfc *clientifc,
                      const char *executable_name,
                      int argc, char **argv, 
                      int defext, int test_mode,
                      CVmHostIfc *hostifc)
{
    char image_file_name[OSFNMAX];
    vm_run_image_params params(clientifc, hostifc, image_file_name);
    int curarg;
    int stat;
    int found_image = FALSE;
    int hide_usage = FALSE;
    int usage_err = FALSE;
#ifdef TADSNET
    char *storage_sid = 0;
#endif

    /* check to see if we can load from the .exe file */
    {
        /* look for an image file attached to the executable */
        osfildef *fp = os_exeseek(argv[0], "TGAM");
        if (fp != 0)
        {
            /* close the file */
            osfcls(fp);
            
            /* note that we want to load from the executable */
            params.load_from_exe = TRUE;
        }

        /* 
         *   if loading from the exe file, get the custom saved game suffix,
         *   if defined 
         */
        if (params.load_from_exe
            && (fp = os_exeseek(argv[0], "SAVX")) != 0)
        {
            ushort len;
            char buf[OSFNMAX];
            
            /* read the length and the name */
            if (!osfrb(fp, &len, sizeof(len))
                && len < sizeof(buf)
                && !osfrb(fp, buf, len))
            {
                /* null-terminate the string */
                buf[len] = '\0';

                /* save it in the OS layer */
                os_set_save_ext(buf);
            }

            /* close the file */
            osfcls(fp);
        }
    }

    /* scan options */
    for (curarg = 1 ; curarg < argc && argv[curarg][0] == '-' ; ++curarg)
    {
        /* 
         *   if the argument is just '-', it means we're explicitly leaving
         *   the image filename blank and skipping to the arguments to the VM
         *   program itself 
         */
        if (argv[curarg][1] == '\0')
            break;
        
        /* check the argument */
        switch(argv[curarg][1])
        {
        case 'b':
            if (strcmp(argv[curarg], "-banner") == 0)
            {
                /* make a note to show the banner */
                params.show_banner = TRUE;
            }
            else
                goto opt_error;
            break;

        case 'c':
            if (strcmp(argv[curarg], "-cs") == 0)
            {
                /* set the console character set name */
                if (++curarg < argc)
                    params.charset = argv[curarg];
                else
                    goto opt_error;
            }
            else if (strcmp(argv[curarg], "-csl") == 0)
            {
                /* set the log file character set name */
                if (++curarg < argc)
                    params.log_charset = argv[curarg];
                else
                    goto opt_error;
            }
            else
                goto opt_error;
            break;

        case 'n':
            if (argv[curarg][2] == 's')
            {
                /* network safety level - parse the ## arguments */
                const char *cli = &argv[curarg][3];
                const char *srv = &argv[curarg][4];

                /* if there's no server level, use the one level for both */
                if (*cli == '\0' || *srv == '\0')
                    srv = cli;

                /* validate the arguments */
                if (*cli < '0' || *cli > '2'
                    || *srv < '0' || *srv > '2')
                    goto opt_error;

                /* set the level */
                hostifc->set_net_safety(*cli - '0', *srv - '0');
            }
            else if (strcmp(argv[curarg], "-nobanner") == 0)
            {
                /* make a note not to show the banner */
                params.show_banner = FALSE;
            }
            else if (strcmp(argv[curarg], "-norand") == 0
                     || strcmp(argv[curarg], "-norandomize") == 0)
            {
                /* make a note not to seed the RNG */
                params.seed_rand = FALSE;
            }
            else if (strcmp(argv[curarg], "-nomore") == 0)
            {
                /* turn off MORE mode */
                params.more_mode = FALSE;
            }
            else
                goto opt_error;
            break;
            
        case 's':
            if (argv[curarg][2] == 'd')
            {
                /* sandbox directory */
                if (++curarg < argc)
                    params.sandbox_dir = argv[curarg];
                else
                    goto opt_error;
            }
            else
            {
                /* 
                 *   file safety level - get the read and write levels, if
                 *   specified separately 
                 */
                const char *rd = &argv[curarg][2];
                const char *wrt = &argv[curarg][3];

                /* if there's no write level, base it on the read level */
                if (*rd == '\0' || *wrt == '\0')
                    wrt = rd;
                    
                /* check the ranges */
                if (*rd < '0' || *rd > '4'
                    || *wrt < '0' || *wrt > '4'
                    || *(wrt+1) != '\0')
                {
                    /* invalid level */
                    goto opt_error;
                }
                else
                {
                    /* set the level in the host application */
                    hostifc->set_io_safety(*rd - '0', *wrt - '0');
                }
            }
            break;

        case 'i':
        case 'I':
            /* 
             *   read from a script file (little 'i' reads silently, big 'I'
             *   echoes the log as it goes) - the next argument, or the
             *   remainder of this argument, is the filename 
             */
            params.script_quiet = (argv[curarg][1] == 'i');
            params.script_file = get_opt_arg(argc, argv, &curarg, 2);
            if (params.script_file == 0)
                goto opt_error;
            break;

        case 'l':
            /* log output to file */
            params.log_file = get_opt_arg(argc, argv, &curarg, 2);
            if (params.log_file == 0)
                goto opt_error;
            break;

        case 'o':
            /* log commands to file */
            params.cmd_log_file = get_opt_arg(argc, argv, &curarg, 2);
            if (params.cmd_log_file == 0)
                goto opt_error;
            break;

        case 'p':
            /* check what follows */
            if (strcmp(argv[curarg], "-plain") == 0)
            {
                /* tell the client to set plain ASCII mode */
                clientifc->set_plain_mode();
                break;
            }
            else
                goto opt_error;
            break;

        case 'r':
            /* get the name of the saved state file to restore */
            params.saved_state = get_opt_arg(argc, argv, &curarg, 2);
            if (params.saved_state == 0)
                goto opt_error;
            break;

        case 'R':
            /* note the resource root directory */
            params.res_dir = get_opt_arg(argc, argv, &curarg, 2);
            if (params.res_dir == 0)
                goto opt_error;
            break;

        case 'd':
            /* note the working directory for file operations */
            params.file_dir = get_opt_arg(argc, argv, &curarg, 2);
            if (params.file_dir == 0)
                goto opt_error;
            break;

        case 'X':
            /* special system operations */
            if (strcmp(argv[curarg], "-Xinterfaces") == 0 && curarg+1 < argc)
            {
                /* write out the interface report, then terminate */
                vm_interface_report(clientifc, argv[++curarg]);
                stat = OSEXSUCC;
                goto done;
            }
            else
                goto opt_error;
            break;

#ifdef TADSNET
        case 'w':
            /* -webhost, -websid, -webimage */
            if (strcmp(argv[curarg], "-webhost") == 0 && curarg+1 < argc)
            {
                /* if necessary, create the web hosting config object */
                create_netconfig(clientifc, params.netconfig, argv);

                /* set the web host name */
                params.netconfig->set("hostname", argv[++curarg]);
            }
            else if (strcmp(argv[curarg], "-websid") == 0
                     && curarg+1 < argc)
            {
                /* web storage server SID - save it for later */
                storage_sid = argv[++curarg];
            }
            else if (strcmp(argv[curarg], "-webimage") == 0
                     && curarg+1 < argc)
            {
                /* 
                 *   Original URL of image file.  This is provided for
                 *   logging purposes only; 
                 */
                create_netconfig(clientifc, params.netconfig, argv);
                params.netconfig->set("image.url", argv[++curarg]);
            }
            else
            {
                /* invalid option - flag an error */
                goto opt_error;
            }
            break;
#endif /* TADSNET */

        default:
        opt_error:
            /* discard remaining arguments */
            curarg = argc;

            /* note the error */
            usage_err = TRUE;
            break;
        }
    }

    /* 
     *   If there was no usage error so far, but we don't have an image
     *   filename argument, try to find the image file some other way.  If we
     *   found an image file embedded in the executable, don't even bother
     *   looking for an image-file argument - we can only run the embedded
     *   image in this case.  
     */
    if (usage_err)
    {
        /* there was a usage error - don't bother looking for an image file */
    }
    else if (!params.load_from_exe
             && curarg + 1 <= argc
             && strcmp(argv[curarg], "-") != 0)
    {
        /* the last argument is the image file name */
        strcpy(image_file_name, argv[curarg]);
        found_image = TRUE;

        /* 
         *   If the given filename exists, use it as-is; otherwise, if
         *   we're allowed to add an extension, try applying a default
         *   extension of "t3" (formerly "t3x") to the given name.  
         */
        if (defext && osfacc(image_file_name))
        {
            /* the given name doesn't exist - try a default extension */
            os_defext(image_file_name, "t3");             /* formerly "t3x" */
        }
    }
    else
    {
        /* 
         *   if we're loading from the executable, try using the executable
         *   filename as the image file 
         */
        if (params.load_from_exe
            && os_get_exe_filename(image_file_name, sizeof(image_file_name),
                                   argv[0]))
            found_image = TRUE;

        /* 
         *   If we still haven't found an image file, try to get the image
         *   file from the saved state file, if one was specified.  Don't
         *   attempt this if we're loading the image from the executable, as
         *   we don't want to allow running a different image file in that
         *   case.  
         */
        if (!params.load_from_exe && !found_image && params.saved_state != 0)
        {
            osfildef *save_fp;

            /* open the saved state file */
            save_fp = osfoprb(params.saved_state, OSFTT3SAV);
            if (save_fp != 0)
            {
                /* get the name of the image file */
                if (CVmSaveFile::restore_get_image(
                    save_fp, image_file_name, sizeof(image_file_name)) == 0)
                {
                    /* we successfully obtained the filename */
                    found_image = TRUE;
                }

                /* close the file */
                osfcls(save_fp);
            }
        }

        /* 
         *   if we haven't found the image, and the host system provides a
         *   way of asking the user for a filename, try that 
         */
        if (!params.load_from_exe && !found_image)
        {
            /* ask the host system for a game name */
            switch (hostifc->get_image_name(image_file_name,
                                            sizeof(image_file_name)))
            {
            case VMHOST_GIN_IGNORED:
                /* no effect - we have no new information */
                break;

            case VMHOST_GIN_CANCEL:
                /* 
                 *   the user cancelled the dialog - we don't have a
                 *   filename, but we also don't want to show usage, since
                 *   the user chose not to proceed 
                 */
                hide_usage = TRUE;
                break;
                
            case VMHOST_GIN_ERROR:
                /* 
                 *   an error occurred showing the dialog - there's not
                 *   much we can do except show the usage message 
                 */
                break;

            case VMHOST_GIN_SUCCESS:
                /* that was successful - we have an image file now */
                found_image = TRUE;
                break;
            }
        }
    }

    /* 
     *   if we don't have an image file name by this point, we can't
     *   proceed - show the usage message and terminate 
     */
    if (usage_err || !found_image)
    {
        char buf[OSFNMAX + 1024];

        /* show the usage message if allowed */
        if (params.load_from_exe && !usage_err)
        {
            sprintf(buf, "An error occurred loading the T3 VM program from "
                    "the embedded data file.  This application executable "
                    "file might be corrupted.\n");

            /* display the message */
            clientifc->display_error(0, 0, buf, FALSE);
        }
        else if (!hide_usage)
        {
            /* build the usage message */
            sprintf(buf,
                    "%s\n"
                    "usage: %s [options] %sarguments]\n"
                    "options:\n"
                    "  -banner - show the version/copyright banner\n"
                    "  -cs xxx - use character set 'xxx' for keyboard "
                    "and display\n"
                    "  -csl xxx - use character set 'xxx' for log files\n"
                    "  -d path - set default directory for file operations\n"
                    "  -i file - read command input from file (quiet mode)\n"
                    "  -I file - read command input from file (echo mode)\n"
                    "  -l file - log all console input/output to file\n"
                    "  -norand - don't seed the random number generator\n"
                    "  -ns##   - set the network safety level for Client "
                    "and Server (each # is from\n"
                    "            0 to 2 - 0 is the least restrictive, 2 "
                    "allows no network access)\n"
                    "  -o file - log console input to file\n"
                    "  -plain  - run in plain mode (no cursor positioning, "
                    "colors, etc.)\n"
                    "  -r file - restore saved state from file\n"
                    "  -R dir  - set directory for external resources\n"
                    "  -s##    - set file safety level for read & write "
                    "(each # is from 0 to 4;\n"
                    "            0 is the least restrictive, 4 prohibits all "
                    "file access)\n"
                    "  -sd dir - set the sandbox directory for file "
                    "safety restrictions\n"
                    "\n"
                    "If provided, the optional extra arguments are passed "
                    "to the program's\n"
                    "main entrypoint.\n",
                    T3VM_BANNER_STRING, executable_name,
                    params.load_from_exe ? "[- " : "<image-file-name> [");
            
            /* display the message */
            clientifc->display_error(0, 0, buf, FALSE);
        }
        
        /* return failure */
        stat = OSEXFAIL;
        goto done;
    }

    /* 
     *   if we're in test mode, replace the first argument to the program
     *   with its root name, so that we don't include any path information
     *   in the argument list 
     */
    if (test_mode && curarg <= argc && argv[curarg] != 0)
        argv[curarg] = os_get_root_name(argv[curarg]);

#ifdef TADSNET
    /*
     *   if there's a storage server SID, add it to the net configuration
     */
    if (params.netconfig != 0 && storage_sid != 0)
        params.netconfig->set("storage.sessionid", storage_sid);
#endif /* TADSNET */

    /* pass the arguments beyond the image file name to the .t3's main() */
    params.prog_argv = argv + curarg;
    params.prog_argc = argc - curarg;

    /* run the program */
    stat = vm_run_image(&params);

done:
    /* return the status code */
    return stat;
}

/* ------------------------------------------------------------------------ */
/*
 *   Copy a filename with a given size limit.  This works essentially like
 *   strncpy(), but we guarantee that the result is null-terminated even if
 *   it's truncated.  
 */
static void strcpy_limit(char *dst, const char *src, size_t limit)
{
    size_t copy_len;

    /* get the length of the source string */
    copy_len = strlen(src);

    /* 
     *   if it exceeds the available space, leaving space for the null
     *   terminator, limit the copy to the available space 
     */
    if (copy_len > limit - 1)
        copy_len = limit - 1;

    /* copy as much as we can */
    memcpy(dst, src, copy_len);

    /* null-terminate what we managed to copy */
    dst[copy_len] = '\0';
}

/* ------------------------------------------------------------------------ */
/*
 *   Given a saved game file, try to identify the game file that created the
 *   saved game.  
 */
static int vm_get_game_file_from_savefile(const char *savefile,
                                          char *fname, size_t fnamelen)
{

    /* open the saved game file */
    osfildef *fp = osfoprb(savefile, OSFTBIN);

    /* if that failed, there's no way to read the game file name */
    if (fp == 0)
        return FALSE;

    /* presume failure */
    int ret = FALSE;

    /* read the first few bytes */
    char buf[128];
    if (!osfrb(fp, buf, 16))
    {
        /* check for a saved game signature we recognize */
        if (memcmp(buf, "TADS2 save/g\012\015\032\000", 16) == 0)
        {
            /* 
             *   It's a TADS 2 saved game with embedded .GAM file
             *   information.  The filename immediately follows the signature
             *   (the 15 bytes we just matched), with a two-byte length
             *   prefix.  Seek to the length prefix and read it.  
             */
            size_t len;
            if (!osfseek(fp, 16, OSFSK_SET) && !osfrb(fp, buf, 2))
                len = osrp2(buf);
            else
                len = 0;

            /* limit the read length to our caller's available buffer */
            if (len > fnamelen - 1)
                len = fnamelen - 1;
            
            /* read the filename */
            if (!osfrb(fp, fname, len))
            {
                /* null-terminate it */
                fname[len] = '\0';
                
                /* success */
                ret = TRUE;
            }
        }
        else if (memcmp(buf, "T3-state-v", 10) == 0)
        {
            /* 
             *   It's a T3 saved state file.  The image filename is always
             *   embedded in this type of file, so seek back to the start of
             *   the file and read the filename.
             *   
             *   Note that restore_get_image() returns zero on success, so we
             *   want to return true if and only if that routine returns
             *   zero.  
             */
            osfseek(fp, 0, OSFSK_SET);
            ret = (CVmSaveFile::restore_get_image(fp, fname, fnamelen) == 0);
        }
        else
        {
            /* 
             *   it's not a signature we know, so it must not be a saved
             *   state file (at least not one we can deal with)
             */
        }
    }

    /* we're done with the file now, so close it */
    osfcls(fp);

    /* return the result */
    return ret;
}


/* ------------------------------------------------------------------------ */
/*
 *   Parse a command line to determine the name of the game file specified
 *   by the arguments.  We'll return the index of the argv element that
 *   specifies the name of the game, or a negative number if there is no
 *   filename argument.
 *   
 *   Note that our parsing will work for TADS 2 or TADS 3 interpreter
 *   command lines, so this routine can be used to extract the filename from
 *   an ambiguous command line in order to check the file for its type and
 *   thereby resolve which interpreter to use.  
 */
int vm_get_game_arg(int argc, const char *const *argv,
                    char *buf, size_t buflen, int *engine_type)
{
    /* presume we won't find a file to restore */
    const char *restore_file = 0;

    /* presume we won't identify the engine type based soley on arguments */
    *engine_type = VM_GGT_INVALID;

    /* 
     *   Scan the arguments for the union of the TADS 2 and TADS 3 command
     *   line options.  Start at the second element of the argument vector,
     *   since the first element is the executable name.  Keep going until
     *   we run out of options, each of which must start with a '-'.
     *   
     *   Note that we don't care about the meanings of the options - we
     *   simply want to skip past them.  This is more complicated than
     *   merely looking for the first argument without a '-' prefix, because
     *   some of the options allow arguments, and an option argument can
     *   sometimes - depending on the argument - take the next position in
     *   the argument vector after the option itself.  So, we must determine
     *   the meaning of each option well enough that we can tell whether or
     *   not the next argv element after the option is part of the option.  
     */
    int i;
    for (i = 1 ; i < argc && argv[i][0] == '-' ; ++i)
    {
        /* 
         *   check the first character after the hyphen to determine which
         *   option we have 
         */
        switch(argv[i][1])
        {
        case 'b':
            /* 
             *   tads 3 "-banner" (no arguments, so just consume it and
             *   continue) 
             */
            break;

        case 'c':
            /* 
             *   tads 3 "-cs charset" or "-csl charset"; tads 2 "-ctab-" or
             *   "-ctab tab" 
             */
            if (strcmp(argv[i], "-ctab") == 0
                || strcmp(argv[i], "-cs") == 0
                || strcmp(argv[i], "-csl") == 0)
            {
                /* there's another argument giving the filename */
                ++i;
            }
            break;

        case 'd':
            /* tads 2 "-double[+-]" (no arguments), tads 3 -d <path> */
            if (memcmp(argv[i], "-double", 7) == 0
                && (argv[i][7] == '\0'
                    || argv[i][7] == '+'
                    || argv[i][7] == '-'))
            {
                /* tads 2 -double - no further arguments */
            }
            else if (argv[i][2] == '\0')
            {
                /* tads 3 -d <path> */
                ++i;
            }
            break;

        case 'm':
            /* tads 2 "-msSize", "-mhSize", "-mSize" */
            switch(argv[i][2])
            {
            case 's':
            case 'h':
                /* 
                 *   argument required - if nothing follows the second
                 *   letter, consume the next vector item as the option
                 *   argument 
                 */
                if (argv[i][3] == '\0')
                    ++i;
                break;

            case '\0':
                /* 
                 *   argument required, but nothing follows the "-m" -
                 *   consume the next vector item 
                 */
                ++i;
                break;

            default:
                /* argument required and present */
                break;
            }
            break;

        case 't':
            /* tads 2 "-tfFile", "-tsSize", "-tp[+-]", "-t[+0]" */
            switch(argv[i][2])
            {
            case 'f':
            case 's':
                /* 
                 *   argument required - consume the next vector item if
                 *   nothing is attached to this item 
                 */
                if (argv[i][3] == '\0')
                    ++i;
                break;
                
            case 'p':
                /* no arguments */
                break;

            default:
                /* no arguments */
                break;
            }
            break;

        case 'u':
            /* 
             *   tads 2 "-uSize" - argument required, so consume the next
             *   vector item if necessary 
             */
            if (argv[i][2] == '\0')
                ++i;
            break;

        case 'n':
            /* 
             *   tads3 "-ns", "-nobanner", "-norand" take no arguments (-ns
             *   does have extra parameter characters, but they have to be
             *   concatenated as part of the same argv entry, as in "-ns11") 
             */
            break;

        case 's':
            if (argv[i][2] == 'd')
            {
                /* tads 3 -sd <dir> - skip the <dir> parameter */
                ++i;
            }
            else if (argv[i][2] == '\0')
            {
                /* 
                 *   tads 2/3 "-s#" (#=0,1,2,3,4); in tads 2, the # can be
                 *   separated by a space from the -s, so we'll allow it this
                 *   way in general 
                 */
                ++i;
            }
            break;

        case 'i':
            /* tads 2/3 "-iFile" - consume an argument */
            if (argv[i][2] == '\0')
                ++i;
            break;

        case 'l':
            /* tads 2/3 "-lFile" - consume an argument */
            if (argv[i][2] == '\0')
                ++i;
            break;

        case 'o':
            /* tads 2/3 "-oFile" - consume an argument */
            if (argv[i][2] == '\0')
                ++i;
            break;

        case 'p':
            /* tads 2/3 "-plain"; tads 2 "-p[+-]" - no arguments */
            break;

        case 'R':
            /* tads 3 '-Rdir' - consume an argument */
            if (argv[i][2] == '\0')
                ++i;
            break;

        case 'r':
            /* tads 2/3 "-rFile" - consume an argument */
            if (argv[i][2] != '\0')
            {
                /* the file to be restored is appended to the "-r" */
                restore_file = argv[i] + 2;
            }
            else
            {
                /* the file to be restored is the next argument */
                ++i;
                restore_file = argv[i];
            }
            break;

        case 'X':
            /* TADS 3 special system commands */
            if (strcmp(argv[i], "-Xinterfaces") == 0)
            {
                /* 
                 *   this is a stand-alone command that doesn't require a
                 *   game file; return the engine type directly 
                 */
                strcpy_limit(buf, "", buflen);
                *engine_type = VM_GGT_TADS3;
                return TRUE;
            }
            break;

        case 'w':
            /* tads 3 -webhost, -websid */
            if (strcmp(argv[i], "-webhost") == 0
                || strcmp(argv[i], "-websid") == 0)
            {
                /* this takes an additional argument */
                ++i;
            }
            break;
        }
    }

    /*
     *   We have no more options, so the next argument is the game filename.
     *   If we're out of argv elements, then no game filename was directly
     *   specified, in which case we'll try looking for a file spec in the
     *   restore file, if present.  
     */
    if (i < argc)
    {
        /* there's a game file argument - copy it to the caller's buffer */
        strcpy_limit(buf, argv[i], buflen);

        /* return success */
        return TRUE;
    }
    else if (restore_file != 0)
    {
        /* 
         *   There's no game file argument, but there is a restore file
         *   argument.  Try identifying the game file from the original game
         *   file specification stored in the saved state file.  
         */
        return vm_get_game_file_from_savefile(restore_file, buf, buflen);
    }
    else
    {
        /* 
         *   there's no game file or restore file argument, so they've given
         *   us nothing to go on - there's no game file 
         */
        return FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Given the name of a file, determine the engine (TADS 2 or TADS 3) for
 *   which the file was compiled, based on the file's signature.  Returns
 *   one of the VM_GGT_xxx codes.  
 */
static int vm_get_game_type_for_file(const char *filename)
{
    osfildef *fp;
    char buf[16];
    int ret;
    
    /* try opening the filename exactly as given */
    fp = osfoprb(filename, OSFTBIN);

    /* if the file doesn't exist, tell the caller */
    if (fp == 0)
        return VM_GGT_NOT_FOUND;

    /* read the first few bytes of the file, where the signature resides */
    if (osfrb(fp, buf, 16))
    {
        /* 
         *   error reading the file - any valid game file is going to be at
         *   least long enough to hold the number of bytes we asked for, so
         *   it must not be a valid file 
         */
        ret = VM_GGT_INVALID;
    }
    else
    {
        /* check the signature we read against the known signatures */
        if (memcmp(buf, "TADS2 bin\012\015\032", 12) == 0)
            ret = VM_GGT_TADS2;
        else if (memcmp(buf, "T3-image\015\012\032", 11) == 0)
            ret = VM_GGT_TADS3;
        else
            ret = VM_GGT_INVALID;
    }

    /* close the file */
    osfcls(fp);

    /* return the version identifier */
    return ret;
}

/*
 *   Given a game file argument, determine which engine (TADS 2 or TADS 3)
 *   should be used to run the game.  
 */
int vm_get_game_type(const char *filename,
                     char *actual_fname, size_t actual_fname_len,
                     const char *const *defexts, size_t defext_count)
{
    int good_count;
    int last_good_ver;
    size_t i;

    /* 
     *   If the exact filename as given exists, determine the file type
     *   directly from this file without trying any default extensions.
     */
    if (osfacc(filename) == 0)
    {
        /* the actual filename is exactly what we were given */
        if (actual_fname_len != 0)
        {
            /* copy the filename, limiting it to the buffer length */
            strncpy(actual_fname, filename, actual_fname_len);
            actual_fname[actual_fname_len - 1] = '\0';
        }

        /* return the type according to the file's signature */
        return vm_get_game_type_for_file(filename);
    }
    
    /* presume we won't find any good files using default extensions */
    good_count = 0;

    /* try each default extension supplied */
    for (i = 0 ; i < defext_count ; ++i)
    {
        int cur_ver;
        char cur_fname[OSFNMAX];

        /* 
         *   build the default filename from the given filename and the
         *   current default suffix 
         */
        strcpy(cur_fname, filename);
        os_defext(cur_fname, defexts[i]);

        /* get the version for this file */
        cur_ver = vm_get_game_type_for_file(cur_fname);
        
        /* if it's a valid code, note it and remember it */
        if (vm_ggt_is_valid(cur_ver))
        {
            /* it's a valid file - count it */
            ++good_count;

            /* remember its version as the last good file's version */
            last_good_ver = cur_ver;

            /* remember its name as the last good file's name */
            if (actual_fname_len != 0)
            {
                /* copy the filename, limiting it to the buffer length */
                strncpy(actual_fname, cur_fname, actual_fname_len);
                actual_fname[actual_fname_len - 1] = '\0';
            }
        }
    }

    /*
     *   If we had exactly one good match, return it.  We will already have
     *   filled in actual_fname with the last good filename, so all we need
     *   to do in this case is return the version ID for the last good file. 
     */
    if (good_count == 1)
        return last_good_ver;

    /*
     *   If we didn't find any matches, tell the caller there is no match
     *   for the given filename. 
     */
    if (good_count == 0)
        return VM_GGT_NOT_FOUND;

    /* we found more than one match, so the type is ambiguous */
    return VM_GGT_AMBIG;
}

/* ------------------------------------------------------------------------ */
/*
 *   Generate the interface report 
 */
void vm_interface_report(CVmMainClientIfc *cli, const char *fname)
{
    int i;
    char nm[256];
    char buf[300];
    char *p;

    /* open the output file */
    osfildef *fp = osfopwt(fname, OSFTTEXT);
    if (fp == 0)
    {
        cli->display_error(
            0, 0, "-Xinterfaces failed: unable to open output file", FALSE);
        return;
    }
    
    /* start the XML file */
    os_fprintz(fp, "<?xml version=\"1.0\"?>\n"
               "<tads3 xmlns="
               "\"http://www.tads.org/xmlns/tads3/UpdateData\">\n");

    /* write the metaclass interface list */
    os_fprintz(fp, " <metaclasses>\n");
    for (i = 0 ; G_meta_reg_table[i].meta != 0 ; ++i)
    {
        /* copy the name string */
        lib_strcpy(nm, sizeof(nm),
                   (*G_meta_reg_table[i].meta)->get_meta_name());

        /* null-terminate at the version delimiter */
        p = strchr(nm, '/');
        *p++ = '\0';

        /* display the information */
        t3sprintf(buf, sizeof(buf),
                  "  <metaclass>\n"
                  "   <id>%s</id>\n"
                  "   <version>%s</version>\n"
                  "  </metaclass>\n",
                  nm, p);
        os_fprintz(fp, buf);
    }
    os_fprintz(fp, " </metaclasses>\n");

    /* write the built-in function information */
    os_fprintz(fp, " <functionsets>\n");
    for (i = 0 ; G_bif_reg_table[i].func_set_id != 0 ; ++i)
    {
        /* get the function set name/version string */
        lib_strcpy(nm, sizeof(nm), G_bif_reg_table[i].func_set_id);

        /* null-terminate at the version delimiter */
        p = strchr(nm, '/');
        *p++ = '\0';

        /* display the information */
        t3sprintf(buf, sizeof(buf),
                  "  <functionset>\n"
                  "   <id>%s</id>\n"
                  "   <version>%s</version>\n"
                  "  </functionset>\n",
                  nm, p);
        os_fprintz(fp, buf);
    }
    os_fprintz(fp, " </functionsets>\n");

    /* end the XML file */
    os_fprintz(fp, "</tads3>\n");

    /* done with the file */
    osfcls(fp);
}

/* ------------------------------------------------------------------------ */
/*
 *   Retrieve the IFID from the gameinfo.txt resource 
 */
char *vm_get_ifid(CVmHostIfc *hostifc)
{
    /* first, find the gameinfo.txt resource */
    unsigned long siz;
    osfildef *fp = hostifc->find_resource("gameinfo.txt", 12, &siz);

    /* if we didn't find the resource, there's no IFID */
    if (fp == 0)
        return 0;

    /* 
     *   allocate a buffer to load the gameinfo.txt contents into (leave room
     *   for an extra newline at the end, for uniformity) 
     */
    char *buf = (char *)t3malloc(siz + 1);
    if (buf == 0)
        return 0;

    /* presume we won't find an IFID */
    char *ifid = 0;

    /* load the contents */
    if (!osfrb(fp, buf, siz))
    {
        char *p, *dst;
        
        /* get the buffer end pointer */
        char *endp = buf + siz;
        
        /* change CR-LF, LF-CR, and CR to '\n' */
        for (p = dst = buf ; p < endp ; )
        {
            if (*p == 10)
            {
                /* LF - store as '\n', and skip the CR in an LF-CR pair */
                *dst++ = '\n';
                if (++p < endp && *p == 13)
                    ++p;
            }
            else if (*p == 13)
            {
                /* CR - store as '\n', and skip the LF in a CR-LF pair */
                *dst++ = '\n';
                if (++p < endp && *p == 10)
                    ++p;
            }
            else
            {
                /* copy others as-is */
                *dst++ = *p++;
            }
        }

        /* note the new end pointer */
        endp = dst;

        /* remove comments and blank lines, and merge continuation lines */
        for (p = dst = buf ; p < endp ; )
        {
            /* skip leading whitespace */
            for ( ; p < endp && isspace(*p) ; ++p) ;

            /* if that's the end of the buffer, we're done */
            if (p == endp)
                break;

            /* skip blank lines */
            if (*p == '\n')
            {
                ++p;
                continue;
            }

            /* skip comment lines */
            if (*p == '#')
            {
                /* skip to the end of the line */
                for ( ; p < endp && *p != '\n' ; ++p) ;
                continue;
            }

            /* 
             *   We're on a line with actual contents.  Merge continuation
             *   lines: a continuation line is a line following this line
             *   that starts with a space and contains at least one
             *   non-whitespace character.  
             */
            while (p < endp)
            {
                /* scan and copy to the end of this line */
                for ( ; p < endp && *p != '\n' ; *dst++ = *p++) ;

                /* check the next line to see if it's a continuation line */
                if (p < endp && *(p+1) != '\n' && isspace(*(p+1)))
                {
                    /* scan ahead for non-whitespace */
                    char *q;
                    for (q = p + 1 ; q < endp && *q != '\n' && isspace(*q) ;
                         ++q) ;

                    /* if we found something, it's a continuation line */
                    if (q < endp && *q != '\n')
                    {
                        /* convert the newline to whitespace in the output */
                        *dst++ = ' ';

                        /* skip the whitespace */
                        p = q;

                        /* resume scanning this line */
                        continue;
                    }
                }

                /* 
                 *   It's not a continuation line.  Store a newline at the
                 *   end of the line. 
                 */
                *dst++ = '\n';

                /* skip the newline */
                if (p < endp)
                    ++p;

                /* done processing this line */
                break;
            }
        }

        /* note the new end pointer */
        endp = dst;

        /* 
         *   Okay, the buffer is in a nice canonical format, so we can search
         *   for the IFID name/value pair. 
         */
        for (p = buf ; p < endp ; ++p)
        {
            /* if we're at the IFID line, retrieve the value */
            if (memicmp(p, "ifid:", 5) == 0)
            {
                /* this is the IFID line - skip the IFID: and whitespace */
                for (p += 5 ; p < endp && isspace(*p) ; ++p) ;

                /* 
                 *   the IFID is the portion up to the next space, newline,
                 *   or comma 
                 */
                char *start;
                for (start = p ; p < endp && *p != '\n' && !isspace(*p) ;
                     ++p) ;

                /* allocate a copy of the IFID */
                ifid = lib_copy_str(start, p - start);

                /* we're done */
                break;
            }
            
            /* scan ahead to the end of the line */
            for ( ; p < endp && *p != '\n' ; ++p) ;
        }
    }

    /* done with the gameinfo.txt file and the buffer we loaded it into */
    osfcls(fp);
    t3free(buf);

    /* return the IFID, if we found one */
    return ifid;
}


