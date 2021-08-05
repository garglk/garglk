/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmcore.cpp - T3 VM "core" interpreter - example main entrypoint
Function
  This is an example of how to link the T3 VM "core" into a separate
  application.  This is just a placeholder file; in a real system, the
  application itself would provide the main entrypoint, so this file would
  not be included in the build.

  In addition to the main entrypoint, we define a skeleton intrinsic
  function set.  A real application would fill in the definitions of the
  intrinsic functions so that they expose functionality of the application
  to the T3 program.  This way, the T3 program can call the host application.

  The core interpreter does not have any user interface features at all.
  It is the responsibility of the host application to provide access to user
  interface features.  In most cases, the whole point of linking the T3 VM
  into another application is to provide an alternative user interface to T3
  programs, so the entire extent of the work of integrating T3 and the host
  application is implementing the intrinsic function set to provide UI
  features to the T3 program.
Notes
  
Modified
  04/05/02 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdlib.h>

#include "t3std.h"
#include "vmglob.h"
#include "vmtype.h"
#include "vmmain.h"
#include "vmhost.h"
#include "vmcore.h"
#include "charmap.h"
#include "vmstr.h"


/* we need this #include only if we're using CVmHostIfcStdio (see below) */
#include "vmhostsi.h"

/* ------------------------------------------------------------------------ */
/*
 *   Client Interface implementation.  See the explanation in main() below
 *   for more details on the purpose of this class.
 *   
 *   This isn't a very fancy implementation.  Most real applications will
 *   want to provide implementations tied to their UI's.  
 */
class MyClientIfc: public CVmMainClientIfc
{
public:
    /* 
     *   Set plain ASCII mode - most GUI-type applications need do nothing
     *   here.  The purpose of this is to allow the user to set a
     *   console-mode application to use simple stream output only, for
     *   purposes such as interoperating with a text-to-speech converter.
     *   If the application doesn't have a way of switching between
     *   character mode and graphics mode, there's nothing for this routine
     *   to do.  
     */
    void set_plain_mode() { }

    /* 
     *   Create the main system console - we don't want a console, because
     *   we don't want to make assumptions about the nature of the user
     *   interface.
     *   
     *   In a real application, you would only want to create a console if
     *   you want to implement the stream-oriented output features of the
     *   standard TADS interpreter UI.  The console subsystem provides text
     *   formatting features (word wrapping, spacing, bold/highlighting, and
     *   so on) that are suitable for terminal-style display.  
     */
    class CVmConsoleMain *create_console(struct vm_globals *) { return 0; }

    /* delete the console - we never created one, so there's nothing to do */
    void delete_console(struct vm_globals *, class CVmConsoleMain *) { }

    /* 
     *   Initialize.  This particular application doesn't request scripting
     *   files, log files, command log files, or banner strings when calling
     *   the VM, so there's no need for us to deal with any of those
     *   possibilities. 
     */
    void client_init(struct vm_globals *,
                     const char * /*script_file*/, int /*script_quiet*/,
                     const char * /*log_file*/,
                     const char * /*cmd_log_file*/,
                     const char * /*banner_str*/,
                     int /*more_mode*/)
    {
        /* do nothing */
    }

    /* we don't do anything at initialization, so there's nothing to undo */
    void client_terminate(struct vm_globals *) { }

    /* pre-execution notification */
    void pre_exec(struct vm_globals *) { }

    /* post-execution notification */
    void post_exec(struct vm_globals *) { }

    /* post-execution notification with errors */
    void post_exec_err(struct vm_globals *) { }

    /* 
     *   Display an error message - we'll simply show the message on the
     *   standard output.  Real implementations would normally want to
     *   integrate this with their own user interface; on a GUI platform,
     *   for example, we might want to pop up an alert box with the mesage.  
     */
    void display_error(struct vm_globals *, const struct CVmException *exc,
                       const char *msg, int add_blank_line)
    {
        /* show the message */
        printf("%s\n", msg);

        /* add a blank line after it, if requested */
        if (add_blank_line)
            printf("\n");
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Main program entrypoint.  This is meant to be replaced by the actual
 *   host application.
 *   
 *   Don't worry if you're using an OS that uses something other than the
 *   standard Unix-style "main()" as its entrypoint - you're going to remove
 *   this entire file anyway and replace it with your own, so you can use
 *   whatever style of OS entrypoint is appropriate.  The only reason this
 *   is here is to serve as an example of how you invoke the T3 VM to get
 *   your compiled T3 program running in the first place.
 *   
 *   Note also that you don't have to call the VM directly from your
 *   entrypoint function.  We call the VM from main() only because we have
 *   nothing else useful to do.  You can do as much as you want to set up
 *   your program or even run interactively for a while before calling the
 *   VM to run the T3 program.  
 */
int main(int argc, char **argv)
{
    int stat;
    int load_from_exe;
    const char *image_file_name;

    /*
     *   For our purposes, we will assume that our argument vector contains
     *   only one argument, which is the name of the program to run.  If the
     *   arguments don't look right, terminate with an error.
     */
    if (argc != 2)
    {
        printf("usage: t3core <program-name>\n");
        exit(1);
    }

    /* 
     *   The image (.t3) file's name is given by the first argument.
     *   
     *   Some applications might want to bind the .t3 file directly into the
     *   application's executable file (the .exe file on Windows, for
     *   example) rather than loading a separate .t3 file.  Fortunately,
     *   this is easy.  Two steps are required.
     *   
     *   1.  After building the application executable and compiling the T3
     *   program to yield a .t3 file, use the appropriate OS-specific TADS
     *   tool to bind two together into a single executable file.  On
     *   DOS/Windows, this tool is 'maketrx32' - simply specify the name of
     *   your application executable, the name of your .t3 file, and the
     *   name of a new executable, and the tool will generate a new
     *   executable that has both files bound together.
     *   
     *   2.  In our call to vm_run_image(), rather than passing the name of
     *   a .t3 file to laod, we'd pass the name of the application
     *   executable file itself (this is simply argv[0] with our unix-style
     *   main(), but on other systems we might have to obtain this
     *   information some other way), and we'd pass TRUE for the
     *   'load_from_exe' argument to vm_run_image().  This will make the VM
     *   look for the .t3 file bound into the application executable using
     *   an appropriate OS-specific mechanism.  
     */
    image_file_name = argv[1];
    load_from_exe = FALSE;

    /*
     *   Create the "host interface."  For our purposes, we use the simple
     *   "stdio" host interface implementation, which the T3 source code
     *   provides for convenience.  The host interface lets the main
     *   application (the "host") communicate certain information about the
     *   execution environment to the VM, and carries out certain tasks on
     *   behalf of the VM; the purpose of this is to allow the VM adapt more
     *   easily to different application environments by deferring certain
     *   operations to the host application, so that the VM doesn't have to
     *   make assumptions about the host environment that might not always
     *   be true.
     *   
     *   Most real applications will not want to use the simple "stdio" host
     *   interface implementation, because this standard implementation does
     *   make lots of assumptions about the host environment that might not
     *   always be true.  That's why we have to create the object here - the
     *   VM can't create it, because it can't know what kind of object we'd
     *   want to use.
     *   
     *   If you do want to customize the host interface, you'll need to
     *   implement a C++ class as a subclass of CVmHostIfc - see vmhost.h.
     *   You'll need to provide an implementation for each method defined in
     *   CVmHostIfc.
     *   
     *   Note that we use "new" to allocate this object, rather than
     *   allocating it on the stack, because of a logistical detail:
     *   CVmHostIfcStdio actually allocates some memory upon creating an
     *   instance, which it frees when the CVmHostIfcStdio instance itself
     *   is destroyed.  If we allocated this instance on the stack, the
     *   instance wouldn't be destroyed until this function returns.
     *   However, we want to run a memory leak test (by calling
     *   t3_list_memory_blocks(), below) before we return from the function.
     *   If we allocated this object on the stack, it wouldn't be deleted
     *   until after we return, and so the memory it allocates won't be
     *   deleted until after we return, so our memory test would show those
     *   blocks still allocated and warn of a memory leak.  We deal with
     *   this by explicitly allocating it with 'new' here so that we can
     *   explicitly destroy it with 'delete' before running the memory
     *   check.  
     */
    CVmHostIfc *hostifc = new CVmHostIfcStdio(argv[0]);

    /*
     *   Create the "client interface" object.  This is similar in purpose
     *   to the host interface; this is defined as a separate interface
     *   because it provides functionality that is somewhat orthogonal to
     *   the host interface, so in some cases we might want to mix and match
     *   different pairs of client and host interface implementations.  For
     *   example, HTML TADS uses its own custom host interface, and most
     *   character-mode TADS interpreters use the "stdio" host interface,
     *   but both types of interpreters use the same "console" client
     *   interface implementation.
     *   
     *   There's only one standard system client interface implementation,
     *   which is based on the system "console."  The console is the TADS
     *   output formatter subsystem.  We do NOT use this standard
     *   implementation, because we don't want to depend on the console
     *   layer: the whole point of this core VM configuration is to provide
     *   a version of the VM without any UI dependencies, and including the
     *   console formatter introduces all kinds of dependencies.
     *   
     *   So, we define our own custom implementation of the client
     *   interface.  See the definition earlier in the file.  
     */
    MyClientIfc clientifc;

    /*
     *   Load and run the image file.  This is how we run the T3 program:
     *   this loads the program, sets everything up in the VM, and executes
     *   the program.  This call doesn't return until the program terminates.
     *   
     *   If your application runs on an event-oriented operating system,
     *   such as Windows or Macintosh, you might be wondering at this point
     *   exactly where you're supposed to put your message loop if the T3
     *   program is going to hog the CPU from now until the program
     *   terminates.  There are several approaches you can use:
     *   
     *   1.  You can use a separate thread to run the T3 program, and run
     *   the UI event loop in the main thread.  To do this, rather than
     *   calling the VM directly here, you'd intead spawn another thread,
     *   and let that thread call the VM.  So, the VM would run in that new
     *   thread, leaving the main thread free to proces UI events.  This
     *   would require proper synchronization any time one of your intrinsic
     *   functions needs to access UI features, but otherwise it would be
     *   pretty simple, because the VM is otherwise fairly self-contained.
     *   
     *   2.  You can write the event loop in the T3 program itself (i.e., in
     *   the interpreted code running under the VM).  You would have to
     *   write an intrinsic function to retrieve and dispatch events.  This
     *   approach would probably be a lot of work, because it would mean
     *   that you'd have to provide access to a fair chunk of your OS's GUI
     *   API in your intrinsic function set or sets.  In all likelihood,
     *   you've already implemented most or all of your UI in the C++ part
     *   of your application, and you won't have any desire to provide broad
     *   access to the low-level OS GUI API directly to the T3 program, so
     *   this option is probably not suitable in most cases.
     *   
     *   3.  You can do what HTML TADS does, which is run everything in one
     *   thread, and run *recursive* event loops in the intrinsic functions.
     *   In HTML TADS, the T3 program runs happily along, oblivious to the
     *   UI, until it wants to read some text from the keyboard or display
     *   some text on the console, at which point it has to call an
     *   intrinsic function.  The intrinsic makes the appropriate OS-level
     *   API calls to display the text or whatever.  If the intrinsic's
     *   purpose is to read some input from the user, HTML TADS runs a
     *   recursive event loop to allow the user to interact with the
     *   program.  The recursive event loop monitors a flag, controlled by
     *   the intrinsic, that indicates when the recursive loop is done.  For
     *   example, if the intrinsic's purpose is to wait for a keystroke from
     *   the user, the intrinsic tells the main window that it's waiting for
     *   a key, then calls the recursive event loop; the event loop simply
     *   reads and dispatches events as long as the "done" flag is false;
     *   and the main window, when it receives a keystroke event, notices
     *   that it's waiting for a key, so it stashes the keystroke for
     *   retrieval by the intrinsic and sets the "done" flag to true; the
     *   recursive event loop notices this and returns to the intrinsic,
     *   which finds the keystroke it wanted stashed away, and returns.  
     */
    vm_run_image_params params(&clientifc, hostifc, image_file_name);
    stat = vm_run_image(&params);

    /* we're done with the host interface object, so delete it */
    delete hostifc;

    /* 
     *   Show any unfreed memory.  This is purely for debugging purposes
     *   during development of T3 itself; most real applications that link
     *   in T3 won't want to bother with this, unless they're suspicious
     *   that T3 is leaking memory on them and want to check it.  
     */
    t3_list_memory_blocks(0);

    /* terminate with the status code from the VM */
    exit(stat);
}

/* ------------------------------------------------------------------------ */
/*
 *   Define the "core-sample" intrinsic function set.  This is an example of
 *   how to define native C++ functions that can be called from TADS code
 *   running in the VM.  
 */

/*
 *   displayText(str) - display a text string. 
 */
void CVmBifSample::display_text(VMG_ uint argc)
{
    const char *strp;
    size_t len;

    /* 
     *   Check to make sure we have the right number of arguments.  'argc'
     *   tells us how many arguments we received from the T3 program. 
     */
    check_argc(vmg_ argc, 1);

    /* 
     *   Get the first argument, which is the string to display.
     *   
     *   This will give us a pointer to a string in internal format, which
     *   has a two-byte length prefix.  So, once we have the string, we must
     *   get the length from the string pointer, and then skip the length
     *   prefix to get to the real text of the string.
     *   
     *   Arguments from the T3 program to a native function always appear on
     *   the VM stack, which we can access using the pop_xxx_val() functions.
     *   The arguments appear on the stack in order such that the first
     *   pop_xxx_val() gives us the first argument, the second pop_xxx_val()
     *   gives us the second argument, and so on.  The pop_xxx_val()
     *   functions REMOVE an argument from the stack - once it's removed, we
     *   can get to the next one.  We MUST remove EXACTLY the number of
     *   arguments that we receive before we return.
     */
    strp = pop_str_val(vmg0_);
    len = vmb_get_len(strp);
    strp += VMB_LEN;

    /*
     *   Okay, we have our string, but it's in UTF-8 format, which is an
     *   encoding format for Unicode.  We don't want to display Unicode; we
     *   want to display the local character set.  How do we do this?
     *   Fortunately, T3 provides a handy character mapping subsystem that
     *   will let us convert the string fairly automatically.  The VM also
     *   gives us a pre-loaded mapper for this specific kind of conversion,
     *   in the object G_cmap_to_ui.  G_cmap_to_ui will map characters from
     *   UTF-8 to the local User Interface character set.
     *   
     *   To avoid the need to allocate a gigantic string buffer to convert
     *   the characters, the mapper lets us map in chunks of any size.  So,
     *   we'll simply map and display chunks until we run out of string.
     */
    while (len != 0)
    {
        char buf[128];
        size_t cur_out;
        size_t cur_in;

        /* 
         *   Map as much as we can into our buffer.  This will set cur_out to
         *   the number of bytes in the local character set (the output of
         *   the conversion), and will set cur_in to the number of bytes we
         *   used from the Unicode string (the input). 
         */
        cur_out = G_cmap_to_ui->map_utf8(buf, sizeof(buf),
                                         strp, len, &cur_in);

        /* 
         *   Show the local characters.
         *   
         *   "%.*s" is just like "%s", but the ".*" tells printf to show
         *   exactly the number of characters in the int argument before the
         *   string, instead of showing everything until it finds a null byte
         *   in the string.  This is important because map_utf8 does NOT
         *   null-terminate the result.  
         */
        printf("%.*s", (int)cur_out, buf);

        /* 
         *   skip the characters of input we just translated, so that on the
         *   next iteration of this loop we'll translate the next bunch of
         *   characters 
         */
        strp += cur_in;
        len -= cur_in;
    }
}

/*
 *   readText() - read text from the keyboard and return it as a string. 
 */
void CVmBifSample::read_text(VMG_ uint argc)
{
    char buf[128];
    size_t len;
    vm_obj_id_t str_id;
    CVmObjString *str_obj;
    
    /* check to make sure we have the right number of arguments */
    check_argc(vmg_ argc, 0);

    /* 
     *   Read a string from the keyboard.  Use fgets() rather than plain
     *   gets(), because fgets() lets us limit the buffer size and thus avoid
     *   any chance of a buffer overflow.  (Someone should tell the people at
     *   Microsoft about this - it would probably cut out about eighty
     *   percent of those emergency internet security alerts that require
     *   everyone to download an IE patch every couple of weeks. :)
     */
    fgets(buf, sizeof(buf), stdin);

    /*
     *   One small detail about fgets: if the input ended with a newline,
     *   there will be a newline in the buffer.  Remove it if it's there. 
     */
    if ((len = strlen(buf)) != 0 && buf[len - 1] == '\n')
        buf[len-1] = '\0';

    /*
     *   As in display_text(), we have to deal with character set mapping
     *   before we can send the string back to the TADS program.  This time,
     *   we want to perform the conversion from the local character set to
     *   Unicode.  Again, T3 provides a handy conversion object for our
     *   convenience - this time, it's called G_cmap_from_ui.
     *   
     *   In order to return a string to the TADS program, we have to allocate
     *   a new string object.  First, let's see how big a string we need to
     *   allocate, by calling the character mapper with no buffer space at
     *   all - the mapper will run through the string and check to see how
     *   big it will be after conversion, but it won't actually store
     *   anything.  
     */
    len = G_cmap_from_ui->map_str(0, 0, buf);

    /* 
     *   Allocate a new string to contain the return value.  This gives us
     *   back an "object ID" value, which we can convert into an internal C++
     *   string object pointer using the vm_objp() formula shown. 
     */
    str_id = CVmObjString::create(vmg_ FALSE, len);
    str_obj = (CVmObjString *)vm_objp(vmg_ str_id);

    /*
     *   The string object has a buffer of the size we requested, which is
     *   the size we already know we need to contain the mapped string.  So,
     *   we can call the mapper again to have it perform the actual mapping
     *   into our string buffer. 
     */
    G_cmap_from_ui->map_str(str_obj->cons_get_buf(), len, buf);

    /*
     *   One last step: we must return the string object to the caller.  To
     *   do this, use the retval_obj() function to return the ID of the
     *   string object. 
     */
    retval_obj(vmg_ str_id);
}

/* ------------------------------------------------------------------------ */
/*
 *   This demonstration program is designed for command-line UIs, so we don't
 *   have any special UI initialization based on the loaded image file.  We
 *   can therefore just stub out this routine.
 */
void os_init_ui_after_load(class CVmBifTable *, class CVmMetaTable *)
{
}
