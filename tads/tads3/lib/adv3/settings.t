#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - settings file management
 *   
 *   This is a framework that the library uses to keep track of certain
 *   preference settings - things like the NOTIFY, FOOTNOTES, and EXITS
 *   settings. 
 *   
 *   The point of this framework is "global" settings - settings that apply
 *   not just to a particular game, but to all games that have a particular
 *   feature.  Things like NOTIFY, FOOTNOTES, and some other such features
 *   are part of the standard library, so they tend to be available in most
 *   games.  Furthermore, they tend to work more or less the same way in
 *   most games.  As a result, a given player will probably prefer to set
 *   the options a particular way for most or all games.  If a player
 *   doesn't like score notification, she'll probably dislike it across the
 *   board, not just in certain games.
 *   
 *   This module provides the internal, programmatic core for managing
 *   global preferences.  There's no UI in this part of the implementation;
 *   the adv3 library layers the UI on top via the settingsUI object, but
 *   other alternative UIs could be built using the API provided here.
 *   
 *   The framework is extensible - there's an easy, structured way for
 *   library extensions and games to add their own configuration variables
 *   that will be automatically managed by the framework.  All you have to
 *   do to create a new configuration variable is to create a SettingsItem
 *   object to represent it.  Once you've created the object, the library
 *   will automatically find it and manage it for you.
 *   
 *   This module is designed to be separable from the adv3 library, so that
 *   alternative libraries or stand-alone (non-library-based) games can
 *   reuse it.  This file has no dependencies on anything in adv3 (at
 *   least, it shouldn't).  
 */

#include <tads.h>
#include <file.h>


/* ------------------------------------------------------------------------ */
/*
 *   A settings item.  This encapsulates a single setting variable.  When
 *   we're saving or restoring default settings, we'll simply loop over all
 *   objects of this class to get or set the current settings.
 *   
 *   Note that we don't make any assumptions in this base class about the
 *   type of the value associated with this setting, how it's stored, or
 *   how it's represented in the external configuration file.  This means
 *   that each subclass has to provide the property or properties that
 *   store the item's value, and must also define the methods that operate
 *   on the value.
 *   
 *   If you want to force a particular default setting for a particular
 *   preference item, overriding the setting stored in the global
 *   preferences file, you can override that SettingsItem's
 *   settingFromText() method.  This is the method that interprets the
 *   information in the preferences file, so if you want to ignore the
 *   preferences file setting, override this method to set the hard-coded
 *   value of your choosing.  
 */
class SettingsItem: object
    /*
     *   The setting's identifier string.  This is the ID of the setting as
     *   it appears in the external configuration file.
     *   
     *   The ID should be chosen to ensure uniqueness.  To reduce the
     *   chances of name collisions, we suggest a convention of using a two
     *   part name: a prefix identifying the source of the name (an
     *   abbreviated version of the name of the library, library extension,
     *   or game), followed by a period as a separator, followed by a short
     *   descriptive name for the variable.  The library follows this
     *   convention by using names of the form "adv3.xxx" - the "adv3"
     *   prefix indicates the standard library.
     *   
     *   The ID should contain only letters, numbers, and periods.  Don't
     *   use spaces or punctuation marks (other than periods).
     *   
     *   Note that the ID string is for the program's use, not the
     *   player's, so this isn't something we translate to different
     *   languages.  Note, though, that the configuration file is a simple
     *   text file, so it wouldn't hurt to use a reasonably meaningful
     *   name, in case the user takes it upon herself to look at the
     *   contents of the file.  
     */
    settingID = ''

    /* 
     *   Display a message fragment that shows the current setting value.
     *   We use this to show the player exactly what we're saving or
     *   restoring in response to a SAVE DEFAULTS or RESTORE DEFAULTS
     *   command, so that there's no confusion about which settings are
     *   included.  In most cases, the best thing to show here is the
     *   command that selects the current setting: "NOTIFY ON," for
     *   example.  This is for the UI's convenience; it's not used by the
     *   settings manager itself.  
     */
    settingDesc = ""

    /*
     *   Should this item be included in listings shown to the user?  If
     *   this is true, the UI will include this setting in a display list
     *   of current settings shown to the user on request, by calling our
     *   settingDesc method.  
     */
    includeInListing = true

    /* 
     *   Get the textual representation of the setting - returns a string
     *   representing the setting as it should appear in the external
     *   configuration file.  We use this to write the setting to the file.
     *   
     *   Note that this is only needed if the default saveItem() method is
     *   used.  
     */
    settingToText() { /* subclasses must override */ }

    /* 
     *   Set the current value to the contents of the given string.  The
     *   string contains a textual representation of a setting value, as
     *   previously generated with settingToText().
     *   
     *   This is only needed if the default restoreItem() method is used.  
     */
    settingFromText(str) { /* subclasses must override */ }

    /*
     *   Load from a settings file.  By default, this simply calls the
     *   setting file object to load the data. 
     *   
     *   This implementation is suitable for any scalar type, so this won't
     *   need to be overwritten for subclasses that only need to load a
     *   single string value from the file.  Subclasses that implement
     *   complex (non-scalar) datatypes can override this as needed to read
     *   multiple line items from the file.  
     */
    restoreItem(s)
    {
        /* look up the file item by ID */
        local fileItem = s.getItem(settingID);

        /* 
         *   if this item appears in the file, retrieve its value; if not,
         *   restore my factory default setting 
         */
        settingFromText(fileItem != nil ? fileItem.val_ : factoryDefault);
    }

    /*
     *   Save to a settings file.  By default, this makes a string out of
     *   our value and updates or adds our corresponding entry in the file.
     *   
     *   This implementation is suitable for any scalar type, so this won't
     *   need to be overwritten for subclasses that only need to store a
     *   single string value in the file.  Subclasses that implement
     *   complex (non-scalar) datatypes can override this as needed to
     *   manipulate multiple line items in the file.
     */
    saveItem(s)
    {
        /* get the string representation of my value */
        local val = settingToText();

        /* add or replace it in the file */
        s.setItem(settingID, val);
    }

    /* 
     *   My "factory default" setting.  At pre-init time, before we've
     *   loaded the settings file for the first time, we'll run through all
     *   SettingsItems and store their pre-defined source-code settings
     *   here, as though we were saving the values to a file.  Later, when
     *   we load a file, if we find the file lacks an entry for this
     *   setting item, we'll simply re-load the factory default from this
     *   property. 
     */
    factoryDefault = nil
;

/*
 *   A binary settings item - this is for variables that have simple
 *   true/nil values. 
 */
class BinarySettingsItem: SettingsItem
    /* convert to text - use ON or OFF as the representation */
    settingToText() { return isOn ? 'on' : 'off'; }

    /* parse text */
    settingFromText(str)
    {
        /* convert to lower-case and strip off spaces */
        if (rexMatch('<space>*(<alpha>+)', str.toLower()) != nil)
            str = rexGroup(1)[3];

        /* get the new setting */
        isOn = (str.toLower() == 'on');
    }

    /* our value is true (on) or nil (off) */
    isOn = nil
;

/*
 *   A string settings item.  This is for variables that have scalar string
 *   values.  Value strings can contain anything except newlines.  
 */
class StringSettingsItem: SettingsItem
    /* convert to text */
    settingToText()
    {
        /* quote the value if necessary */
        return quoteValue(val);
    }

    /* parse text */
    settingFromText(str)
    {
        /* 
         *   If the value isn't quoted, use the value as-is, trimming off
         *   leading and trailing spaces.  If it at least starts with a
         *   quote, remove the leading and trailing quote (if present) and
         *   translate backslash sequences.  
         */
        if (rexMatch('^<space>*"', str))
        {
            /* it's quoted - remove the quotes and translate backslashes */
            val = rexReplace(
                [leadTrailSpPat, '\\"', '\\\\', '\\n', '\\r'], str,
                ['', '"', '\\', '\n', '\r']);
        }
        else
        {
            /* no leading quote; just trim spaces */
            rexMatch(trimSpPat, str);
            val = rexGroup(1)[3];
        }
    }

    leadTrailSpPat = static new RexPattern('^<space>+|<space>+$')
    trimSpPat = static new RexPattern('^<space>*(.*?)<space>*$')

    /* 
     *   Class method: quote a string value for storing in the file.  If
     *   the string has any leading or trailing spaces, starts with a
     *   double quote, or contains any newlines, we'll quote it; otherwise
     *   we'll return it as-is.  
     */
    quoteValue(str)
    {
        /* 
         *   if the value needs quoting, quote it, otherwise just return it
         *   unchanged 
         */
        if (rexSearch(needQuotePat, str))
        {
            /* 
             *   add quotes around the string, and backslash-escape any
             *   quotes, backslashes, or newlines within the string 
             */
            return '"' + val.findReplace(
                ['"', '\\', '\n', '\r'], ['\\"', '\\\\', '\\n', '\\r'])
                + '"';
        }
        else
        {
            /* quotes aren't needed*/
            return str;
        }
    }

    needQuotePat = static new RexPattern('^<space>+|^"|[\r\n]')

    /* our current value string */
    val = ''
;
    
    


/* ------------------------------------------------------------------------ */
/*
 *   The settings manager.  This object gathers up some global methods for
 *   managing the saved settings.  This base class provides only a
 *   programmatic interface - it doesn't have a user interface.  
 */
settingsManager: object
    /*
     *   Save the current settings.  This writes out the current settings
     *   to the global settings file.
     *   
     *   On any error, the method throws an exception.  Possible errors
     *   include:
     *   
     *   - FileCreationException indicates that the settings file couldn't
     *   be opened for writing.  
     */
    saveSettings()
    {
        /* retrieve the current settings */
        local s = retrieveSettings();

        /* if that failed, there's nothing more we can do */
        if (s == nil)
            return;

        /* 
         *   Update the file's contents with all of the current in-memory
         *   settings objects (applying the filter condition, if provided).
         */
        forEachInstance(SettingsItem, { item: item.saveItem(s) });

        /* write out the settings */
        storeSettings(s);
    }

    /* 
     *   Restore all of the settings.  If an error occurs, we'll throw an
     *   exception:
     *   
     *   - SettingsNotSupportedException - this is an older interpreter
     *   that doesn't support the "special files" feature, so we can't save
     *   or restore the default settings.  
     */
    restoreSettings()
    {
        /* retrieve the current settings */
        local s = retrieveSettings();

        /* 
         *   update all of the in-memory settings objects with the values
         *   from the file 
         */
        forEachInstance(SettingsItem, {item: item.restoreItem(s)});
    }

    /* 
     *   Retrieve the settings from the global settings file.  This returns
     *   a SettingsFileData object that describes the file's contents.
     *   Note that if there simply isn't an existing settings file, we'll
     *   successfully return a SettingsFileData object with no data - the
     *   absence of a settings file isn't an error, but is merely
     *   equivalent to an empty settings file.  
     */
    retrieveSettings()
    {
        local f;
        local s = new SettingsFileData();
        local linePat = new RexPattern(
            '<space>*(<alphanum|-|_|$|lsquare|rsquare|percent|dot>+)'
            + '<space>*=<space>*([^\n]*)\n?$');
        
        /* 
         *   Try opening the settings file.  Older interpreters don't
         *   support the "special files" feature; if the interpreter
         *   predates special file support, it'll throw a "string value
         *   required," since it won't recognize the special file ID value
         *   as a valid filename.  
         */
        try
        {
            /* open the "library defaults" special file */
            f = File.openTextFile(LibraryDefaultsFile, FileAccessRead);
        }
        catch (FileNotFoundException fnf)
        {
            /* 
             *   The interpreter supports the special file, but the file
             *   doesn't seem to exist.  Simply return the empty file
             *   contents object. 
             */
            return s;
        }
        catch (RuntimeError rte)
        {
            /* 
             *   if the error is "string value required," then we have an
             *   older interpreter that doesn't support special files -
             *   indicate this by returning nil 
             */
            if (rte.errno_ == 2019)
            {
                /* re-throw this as a SettingsNotSupportedException */
                throw new SettingsNotSupportedException();
            }

            /* other exceptions are unexpected, so re-throw them */
            throw rte;
        }

        /* read the file */
        for (;;)
        {
            local l;
            
            /* read the next line */
            l = f.readFile();

            /* stop if we've reached end of file */
            if (l == nil)
                break;

            /* parse the line */
            if (rexMatch(linePat, l) != nil)
            {
                /* 
                 *   it parsed - add the variable and its value to the
                 *   contents object 
                 */
                s.addItem(rexGroup(1)[3], rexGroup(2)[3]);
            }
            else
            {
                /* it doesn't parse, so just keep the line as a comment */
                s.addComment(l);
            }
        }

        /* done with the file - close it */
        f.closeFile();

        /* return the populated file contents object */
        return s;
    }

    /* store the given SettingsFileData to the global settings file */
    storeSettings(s)
    {
        /* 
         *   Open the "library defaults" file.  Note that we don't have to
         *   worry here about the old-interpreter situation that we handle
         *   in retrieveSettings() - if the interpreter doesn't support
         *   special files, we won't ever get this far, because we always
         *   have to retrieve the current file's contents before we can
         *   store the new contents.  
         */
        local f = File.openTextFile(LibraryDefaultsFile, FileAccessWrite);

        /* write each line of the file's contents */
        foreach (local item in s.lst_)
            item.writeToFile(f);

        /* done with the file - close it */
        f.closeFile();
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Exception: the settings file mechanism isn't supported on this
 *   interpreter.  This indicates that this is an older interpreter that
 *   doesn't support the "special files" feature, so we can't save or load
 *   the global settings file. 
 */
class SettingsNotSupportedException: Exception
;

/* ------------------------------------------------------------------------ */
/*
 *   SettingsFileData - this is an object we use to represent the contents
 *   of the configuration file. 
 */
class SettingsFileData: object
    construct()
    {
        /* 
         *   We store the contents of the file in two ways: as a list, in
         *   the same order in which the contents appear in the file; and
         *   as a lookup table keyed by variable name.  The list lets us
         *   preserve the parts of the file's contents that we don't need
         *   to change when we read it in and write it back out.  The
         *   lookup table makes it easy to look up particular variable
         *   values.  
         */
        tab_ = new LookupTable(16, 32);
        lst_ = new Vector(16);
    }

    /* 
     *   find an item - returns a SettinsFileItem for the key, or nil if
     *   there's no existing item 
     */
    getItem(id)
    {
        /* return the entry from our ID-keyed table */
        return tab_[id];
    }

    /* iterate over all data (non-comment) items in the file */
    forEach(func)
    {
        /* scan the list */
        foreach (local s in lst_)
        {
            /* if this is a data item, pass it to the callback */
            if (s.ofKind(SettingsFileItem))
                func(s.id_, s.val_);
        }
    }

    /* add a variable */
    addItem(id, val)
    {
        local item;
        
        /* create the item descriptor object */
        item = new SettingsFileItem(id, val);

        /* append it to our file-contents-ordered list */
        lst_.append(item);

        /* add it to the lookup table, keyed by the variable ID */
        tab_[id] = item;
    }

    /* set a variable, adding a new variable if it doesn't already exist */
    setItem(id, val)
    {
        /* look for an existing SettingsFileItem entry for my ID */
        local fileItem = getItem(id);
        
        /* update the item if it exists, otherwise add a new one */
        if (fileItem != nil)
            fileItem.val_ = val;
        else
            addItem(id, val);
    }

    /* add a comment line */
    addComment(str)
    {
        /* append a comment descriptor to the contents list */
        lst_.append(new SettingsFileComment(str));
    }

    /* delete an item */
    delItem(id)
    {
        /* if it's in our table, delete it */
        local item = tab_[id];
        if (item != nil)
        {
            /* delete it from the lookup table */
            tab_.removeElement(id);

            /* delete it from the file source list */
            lst_.removeElement(item);
        }
    }

    /* lookup table of values, keyed by variable name */
    tab_ = nil

    /* a list of SettingsFileItem objects giving the contents of the file */
    lst_ = nil
;

/*
 *   SettingsFileItem - this object describes a single item within an
 *   external settings file. 
 */
class SettingsFileItem: object
    construct(id, val)
    {
        id_ = id;
        val_ = val;
    }

    /* write this value to a file */
    writeToFile(f) { f.writeFile(id_ + ' = ' + val_ + '\n'); }

    /* the variable's ID */
    id_ = nil

    /* the string representation of the value */
    val_ = nil
;

/*
 *   SettingsFileComment - this object describes an unparsed line in the
 *   settings file.  We treat lines that don't match our parsing rules as
 *   comments.  We preserve the contents and order of these lines, but we
 *   don't otherwise try to interpret them. 
 */
class SettingsFileComment: object
    construct(str)
    {
        /* if it doesn't end in a newline, add a newline */
        if (!str.endsWith('\n'))
            str += '\n';

        /* remember the string */
        str_ = str;
    }

    /* write the comment line to a file */
    writeToFile(f) { f.writeFile(str_); }

    /* the text from the file */
    str_ = nil
;


