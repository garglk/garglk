// Copyright (C) 2010 by Ben Cressey.
//
// This file is part of Gargoyle.
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Gargoyle is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Gargoyle; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#import <Cocoa/Cocoa.h>

#include <cstdlib>
#include <string>

#include "font.h"
#include "glk.h"
#include "garglk.h"

static NSMutableArray *gli_registered_fonts = nil;
static NSDistributedLock *gli_font_lock = nil;

bool garglk::fontreplace(const std::string &font, FontType type)
{
    if (font.empty()) {
        return false;
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSString *fontFamily = [NSString stringWithUTF8String: font.c_str()];
    NSFontDescriptor *fontFamilyDescriptor =
        [[NSFontDescriptor fontDescriptorWithFontAttributes: nil] fontDescriptorWithFamily:fontFamily];

    NSArray *fontMatches =
        [fontFamilyDescriptor matchingFontDescriptorsWithMandatoryKeys: nil];

    FontFiller filler(type);

    for (NSFontDescriptor *sysfont in fontMatches) {
        // find path for font
        CFURLRef urlRef = static_cast<CFURLRef>(CTFontDescriptorCopyAttribute((CTFontDescriptorRef)sysfont, kCTFontURLAttribute));
        if (!urlRef) {
            continue;
        }

        CFStringRef fontPathRef = CFURLCopyFileSystemPath(urlRef, kCFURLPOSIXPathStyle);

        if (fontPathRef && CFStringGetLength(fontPathRef) > 0) {
            NSString *fontPath = (__bridge NSString *)fontPathRef;
            NSLog(@"fontPath: %@", fontPath);

            std::string file = [fontPath UTF8String];
            auto traits = [sysfont symbolicTraits];

            if ((traits & NSFontBoldTrait) && (traits & NSFontItalicTrait)) {
                filler.add(FontFiller::Style::BoldItalic, file);
            } else if (traits & NSFontBoldTrait) {
                filler.add(FontFiller::Style::Bold, file);
            } else if (traits & NSFontItalicTrait) {
                filler.add(FontFiller::Style::Italic, file);
            } else {
                filler.add(FontFiller::Style::Regular, file);
            }

            CFRelease(fontPathRef);
        }

        CFRelease(urlRef);
    }

    [pool drain];

    return filler.fill();
}

void fontload()
{
    // 'GARGLK_RESOURCES' should be the path to the Bundle's Resources directory
    const char *env = std::getenv("GARGLK_RESOURCES");
    if (!env) {
        return;
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    gli_font_lock = [[NSDistributedLock alloc] initWithPath: [NSString stringWithFormat: @"%@/com.googlecode.garglk.fontlock", NSTemporaryDirectory()]];

    if (gli_font_lock) {
        while (![gli_font_lock tryLock]) {
            if ([[NSDate date] timeIntervalSinceDate: [gli_font_lock lockDate]] > 30) {
                [gli_font_lock breakLock];
            } else {
                [NSThread sleepUntilDate: [NSDate dateWithTimeIntervalSinceNow: 0.25]];
            }
        }
    }

    // obtain a list of all files in the Fonts directory
    NSString *fontFolder = [[NSString stringWithUTF8String: env] stringByAppendingPathComponent: @"Fonts"];
    NSArray *fontFiles = [[NSFileManager defaultManager] contentsOfDirectoryAtPath: fontFolder error: nil];

    // create a collection to hold the registered font URLs
    gli_registered_fonts = [NSMutableArray new];

    for (NSString *fontFile in fontFiles) {
        // convert font file name into a file URL
        NSString *fontPath = [fontFolder stringByAppendingPathComponent: fontFile];
        NSURL *fontURL = [NSURL fileURLWithPath: fontPath];

        // register font with system
        CFErrorRef error = nullptr;
        if (!CTFontManagerRegisterFontsForURL((__bridge CFURLRef)fontURL, kCTFontManagerScopeProcess, &error)) {
            CFStringRef msg = CFErrorCopyFailureReason(error);
            NSLog(@"fontload: %@", msg);
            CFRelease(msg);
            continue;
        }

        [gli_registered_fonts addObject:fontURL];
    }

    [pool drain];
}

void fontunload()
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    if (gli_registered_fonts) {
        // uregister fonts
        for (NSURL *url in gli_registered_fonts) {
            CFURLRef urlRef = (__bridge CFURLRef)url;

            CFErrorRef error = nullptr;
            if (!CTFontManagerUnregisterFontsForURL(urlRef, kCTFontManagerScopeProcess, &error)) {
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

    if (gli_font_lock) {
        [gli_font_lock unlock];
        [gli_font_lock release];
        gli_font_lock = nil;
    }

    [pool drain];
}
