/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmmaincn.h - VM main startup - console helper version
Function
  Implements some helpers for the vmmain module and its users, for
  implementations using the standard system console (G_console and
  os_printz):

  - provides a CVmMainClientIfc implementation that writes errors to
  G_console (or os_printz, if no G_console is available), and ignores other
  notifications
  
Notes
  
Modified
  04/05/02 MJRoberts  - Creation
*/

#include "os.h"
#include "t3std.h"
#include "vmmain.h"
#include "vmconsol.h"


/* ------------------------------------------------------------------------ */
/*
 *   Client services interface for T3 VM - console-based version
 */
class CVmMainClientConsole: public CVmMainClientIfc
{
public:
    /* set plain ASCII mode */
    void set_plain_mode()
    {
        /* set plain mode in the OS-level console */
        os_plain();
    }

    /* create the console */
    CVmConsoleMain *create_console(struct vm_globals *vmg)
    {
        /* set up for global access */
        VMGLOB_PTR(vmg);

        /* create a standard console and return it */
        return new CVmConsoleMain(vmg0_);
    }

    /* delete the console */
    void delete_console(struct vm_globals *vmg, CVmConsoleMain *con)
    {
        /* set up for global access */
        VMGLOB_PTR(vmg);

        /* flush any pending buffered output */
        con->flush(vmg_ VM_NL_NONE);
        
        /* delete the output formatter */
        con->delete_obj(vmg0_);
    }

    /* initialize */
    void client_init(struct vm_globals *vmg,
                     const char *script_file, int script_quiet,
                     const char *log_file,
                     const char *cmd_log_file,
                     const char *banner_str,
                     int more_mode)
    {
        /* set up for global access */
        VMGLOB_PTR(vmg);

        /* set the [More] mode as desired */
        G_console->set_more_state(more_mode);

        /* if we have a script file, set up script input on the console */
        if (script_file != 0)
        {
            G_console->open_script_file(
                vmg_ script_file, script_quiet, FALSE);
        }

        /* if we have a log file, set up logging on the console */
        if (log_file != 0)
            G_console->open_log_file(vmg_ log_file);

        /* set up command logging on the console if desired */
        if (cmd_log_file != 0)
            G_console->open_command_log(vmg_ cmd_log_file, TRUE);

        /* tell the HTML renderer that we're a T3 caller */
        G_console->format_text(vmg_ "<?T3>");

        /* show the banner on the console, if desired */
        if (banner_str != 0)
        {
            G_console->format_text(vmg_ banner_str);
            G_console->write_blank_line(vmg0_);
        }
    }

    /* terminate */
    void client_terminate(struct vm_globals *) { }

    /* pre-execution initialization */
    void pre_exec(struct vm_globals *) { }

    /* post-execution termination/error termination */
    void post_exec(struct vm_globals *) { }
    void post_exec_err(struct vm_globals *) { }

    /* display an error */
    void display_error(struct vm_globals *vmg, const struct CVmException *exc,
                       const char *msg, int add_blank_line)
    {
        CVmConsole *con;
        size_t len;

        /* check for a newline at the end of the message */
        int has_nl = ((len = strlen(msg)) != 0 && msg[len-1] == '\n');
        
        /* set up for global access */
        VMGLOB_PTR(vmg);

        /* if we have globals, get the console */
        con = VMGLOB_IF_AVAIL(G_console);
            
        /* if we have a console, write to it */
        if (con != 0)
        {
            int old_obey;
            
            /* flush any pending buffered output */
            con->flush(vmg_ VM_NL_NONE);

            /* put the console in obey-whitespace mode for our message */
            old_obey = con->set_obey_whitespace(TRUE);

            /* display the message on the console */
            con->format_text(vmg_ msg);

            /* add a blank line if desired */
            if (add_blank_line)
                con->write_blank_line(vmg0_);

            /* restore console mode */
            con->set_obey_whitespace(old_obey);
        }
        else
        {
            /* display the error on the OS-level console */
            os_printz(msg);

            /* add a blank line if desired */
            if (add_blank_line)
            {
                /* add one newline */
                os_printz("\n");

                /* 
                 *   if the message itself didn't end with a newline, add
                 *   another newline 
                 */
                if (!has_nl)
                    os_printz("\n");
            }
        }
    }
};

