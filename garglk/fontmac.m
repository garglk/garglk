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

#import <Cocoa/Cocoa.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "garglk.h"

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

static NSMutableArray * gli_registered_fonts = nil;
static NSDistributedLock * gli_font_lock = nil;

void fontreplace(char *font, int type)
{
    if (!font || !strlen(font))
        return;

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSString * fontFamily = [NSString stringWithUTF8String: font];
    NSFontDescriptor * fontFamilyDescriptor =
        [[NSFontDescriptor fontDescriptorWithFontAttributes: nil] fontDescriptorWithFamily: fontFamily];

    NSArray * fontMatches =
        [fontFamilyDescriptor matchingFontDescriptorsWithMandatoryKeys: nil];

    for (NSFontDescriptor * sysfont in fontMatches)
    {
        /* find style for font */
        int style = FONTR;

        if (([sysfont symbolicTraits] & NSFontBoldTrait) && ([sysfont symbolicTraits] & NSFontItalicTrait))
            style = FONTZ;

        else if ([sysfont symbolicTraits] & NSFontBoldTrait)
            style = FONTB;

        else if ([sysfont symbolicTraits] & NSFontItalicTrait)
            style = FONTI;
        
        /* find path for font */
        CFURLRef urlRef = CTFontDescriptorCopyAttribute((CTFontDescriptorRef)sysfont, kCTFontURLAttribute);
        if (!urlRef)
            continue;

        CFStringRef fontPathRef = CFURLCopyFileSystemPath(urlRef, kCFURLPOSIXPathStyle);

        if (fontPathRef && CFStringGetLength(fontPathRef) > 0)
        {
            NSString * fontPath = (__bridge NSString *)fontPathRef;
            NSLog(@"fontPath: %@", fontPath);

            size_t pathLen = strlen([fontPath UTF8String]);
            char *filebuf = malloc(pathLen + 1);

            strcpy(filebuf, [fontPath UTF8String]);

            switch (type)
            {
                case MONOF:
                    monofont(filebuf, style);
                    break;

                case PROPF:
                    propfont(filebuf, style);
                    break;
            }

            CFRelease(fontPathRef);
        }

        CFRelease(urlRef);
    }

    [pool drain];
}

void fontload(void)
{
    // 'GARGLK_INI' should be the path to the Bundle's Resources directory
    const char * env = getenv("GARGLK_INI");
    if (!env)
        return;

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

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

    // obtain a list of all files in the Fonts directory
    NSString * fontFolder = [[NSString stringWithUTF8String: env] stringByAppendingPathComponent: @"Fonts"];
    NSArray * fontFiles = [[NSFileManager defaultManager] contentsOfDirectoryAtPath: fontFolder error: nil];

    // create a collection to hold the registered font URLs
    gli_registered_fonts = [NSMutableArray new];

    for (NSString * fontFile in fontFiles)
    {
        // convert font file name into a file URL
        NSString * fontPath = [fontFolder stringByAppendingPathComponent: fontFile];
        NSURL * fontURL = [NSURL fileURLWithPath: fontPath];

        // register font with system
        CFErrorRef error = NULL;
        if (!CTFontManagerRegisterFontsForURL((__bridge CFURLRef)fontURL, kCTFontManagerScopeProcess, &error))
        {
            CFStringRef msg = CFErrorCopyFailureReason(error);
            NSLog(@"fontload: %@", msg);
            CFRelease(msg);
            continue;
        }

        [gli_registered_fonts addObject: fontURL];
    }

    [pool drain];
}

void fontunload(void)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    if (gli_registered_fonts)
    {
        // uregister fonts
        for (NSURL * url in gli_registered_fonts)
        {
            CFURLRef urlRef = (__bridge CFURLRef)url;

            CFErrorRef error = NULL;
            if (!CTFontManagerUnregisterFontsForURL(urlRef, kCTFontManagerScopeProcess, &error))
            {
                CFStringRef msg = CFErrorCopyFailureReason(error);
                NSLog(@"fontunload: %@", msg);
                CFRelease(msg);
            }
        }

        // remove all objects
        [gli_registered_fonts removeAllObjects];
        [gli_registered_fonts release];
        gli_registered_fonts = nil;
    }

    if (gli_font_lock)
    {
        [gli_font_lock unlock];
        [gli_font_lock release];
        gli_font_lock = nil;
    }

    [pool drain];
}
