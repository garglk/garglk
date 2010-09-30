/******************************************************************************
 *                                                                            *
 * Copyright (C) 2010 by Ben Cressey.                                         *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#import "Cocoa/Cocoa.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "garglk.h"

static ATSFontContainerRef gli_font_container = 0;
static NSDistributedLock * gli_font_lock = NULL;

static int gli_sys_monor = FALSE;
static int gli_sys_monob = FALSE;
static int gli_sys_monoi = FALSE;
static int gli_sys_monoz = FALSE;

static void monofont(char *file, int style)
{
    switch (style)
    {
        case FONTR:
        {
            if (!gli_sys_monor)
            {
                gli_conf_monor = file;

                if (!gli_sys_monob)
                    gli_conf_monob = file;

                if (!gli_sys_monoi)
                    gli_conf_monoi = file;

                if (!gli_sys_monoz && !gli_sys_monoi && !gli_sys_monob)
                    gli_conf_monoz = file;

                gli_sys_monor = TRUE;
            }
            return;
        }

        case FONTB:
        {
            if (!gli_sys_monob)
            {
                gli_conf_monob = file;

                if (!gli_sys_monoz && !gli_sys_monoi)
                    gli_conf_monoz = file;

                gli_sys_monob = TRUE;
            }
            return;
        }

        case FONTI:
        {
            if (!gli_sys_monoi)
            {
                gli_conf_monoi = file;

                if (!gli_sys_monoz)
                    gli_conf_monoz = file;

                gli_sys_monoi = TRUE;
            }
            return;
        }

        case FONTZ:
        {
            if (!gli_sys_monoz)
            {
                gli_conf_monoz = file;
                gli_sys_monoz = TRUE;
            }
            return;
        }
    }
}

static int gli_sys_propr = FALSE;
static int gli_sys_propb = FALSE;
static int gli_sys_propi = FALSE;
static int gli_sys_propz = FALSE;

static void propfont(char *file, int style)
{
    switch (style)
    {
        case FONTR:
        {
            if (!gli_sys_propr)
            {
                gli_conf_propr = file;

                if (!gli_sys_propb)
                    gli_conf_propb = file;

                if (!gli_sys_propi)
                    gli_conf_propi = file;

                if (!gli_sys_propz && !gli_sys_propi && !gli_sys_propb)
                    gli_conf_propz = file;

                gli_sys_propr = TRUE;
            }
            return;
        }

        case FONTB:
        {
            if (!gli_sys_propb)
            {
                gli_conf_propb = file;

                if (!gli_sys_propz && !gli_sys_propi)
                    gli_conf_propz = file;

                gli_sys_propb = TRUE;
            }
            return;
        }

        case FONTI:
        {
            if (!gli_sys_propi)
            {
                gli_conf_propi = file;

                if (!gli_sys_propz)
                    gli_conf_propz = file;

                gli_sys_propi = TRUE;
            }
            return;
        }

        case FONTZ:
        {
            if (!gli_sys_propz)
            {
                gli_conf_propz = file;
                gli_sys_propz = TRUE;
            }
            return;
        }
    }
}

void fontreplace(char *font, int type)
{
    if (!strlen(font))
        return;

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSEnumerator * sysfonts = [[[[NSFontDescriptor fontDescriptorWithFontAttributes:nil] 
                                 fontDescriptorWithFamily: [NSString stringWithCString: font 
                                                                              encoding: NSASCIIStringEncoding]]
                                matchingFontDescriptorsWithMandatoryKeys: nil]
                               objectEnumerator];
    id sysfont;
    int style;    
    FSRef fileref;
    ATSFontRef fontref;
    unsigned char * filebuf;

    while (sysfont = [sysfonts nextObject])
    {
        /* find style for font */
        style = FONTR;

        if (([sysfont symbolicTraits] & NSFontBoldTrait) && ([sysfont symbolicTraits] & NSFontItalicTrait))
            style = FONTZ;

        else if ([sysfont symbolicTraits] & NSFontBoldTrait)
            style = FONTB;

        else if ([sysfont symbolicTraits] & NSFontItalicTrait)
            style = FONTI;
        
        memset(&fileref, 0, sizeof(FSRef));
        memset(&fontref, 0, sizeof(ATSFontRef));

        /* find path for font */
        fontref = ATSFontFindFromPostScriptName((CFStringRef) [sysfont objectForKey: NSFontNameAttribute], kATSOptionFlagsDefault);

#ifdef __x86_64__
        ATSFontGetFileReference(fontref, &fileref);
#else
        FSSpec filespec;
        ATSFontGetFileSpecification(fontref, &filespec);
        FSpMakeFSRef(&filespec, &fileref);
#endif

        filebuf = malloc(4 * PATH_MAX);
        filebuf[0] = '\0';

        FSRefMakePath(&fileref, filebuf, 4 * PATH_MAX - 1);

        if (strlen(filebuf))
        {
            switch (type)
            {
                case MONOF:
                    monofont(filebuf, style);
                    break;

                case PROPF:
                    propfont(filebuf, style);
                    break;
            }
        }
    }

    [pool drain];
}

void fontload(void)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    char * env;
    FSRef fsRef;
    FSSpec fsSpec;

    env = getenv("GARGLK_INI");
    if (!env)
        return;
    
    gli_font_lock = [[NSDistributedLock alloc] initWithPath: [NSString stringWithFormat: @"%@/com.googlecode.garglk.fontlock", NSTemporaryDirectory()]];
    
    if (gli_font_lock)
    {
        while (![gli_font_lock tryLock])
        {
            if ([[NSDate date] timeIntervalSinceDate: [gli_font_lock lockDate]] > 30)
                [gli_font_lock breakLock];
            else
                [NSThread sleepUntilDate: [NSDate dateWithTimeIntervalSinceNow: 0.25]];
        }
    }

    NSString * fontFolder = [[NSString stringWithCString: env encoding: NSASCIIStringEncoding] 
                             stringByAppendingPathComponent: @"Fonts"];

    NSURL * fontURL = [NSURL fileURLWithPath: fontFolder];
    CFURLGetFSRef((CFURLRef) fontURL, &fsRef);
    FSGetCatalogInfo(&fsRef, kFSCatInfoNone, NULL, NULL, &fsSpec, NULL);

#ifdef __x86_64__
    ATSFontActivateFromFileReference(&fsRef, kATSFontContextLocal, kATSFontFormatUnspecified, NULL, kATSOptionFlagsDefault, &gli_font_container);
#else
    ATSFontActivateFromFileSpecification(&fsSpec, kATSFontContextLocal, kATSFontFormatUnspecified, NULL, kATSOptionFlagsDefault, &gli_font_container);
#endif

    [pool drain];
}

void fontunload(void)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    if (gli_font_container)
        ATSFontDeactivate(gli_font_container, NULL, kATSOptionFlagsDefault);
    
    if (gli_font_lock)
    {
        [gli_font_lock unlock];
        [gli_font_lock release];
    }

    [pool drain];
}
