/* scarier-autosave.mm

   Spatterlight autosave/autorestore for the Scarier terp -- the ADRIFT <=4
   engine (gsc_main) and the ADRIFT 5 engine (gsc_a5_main) -- adapted from
   the geas implementation (terps/geas/geasglk-autosave.mm), which is in
   turn adapted from Bocfel's, which follows Andrew Plotkin's IosGlk
   autosave design.

   At every top-level command prompt the engine-state container built by
   os_glk.cpp goes into

     ~/Library/Application Support/Spatterlight/SCARE Files/Autosaves/(HASH)/autosave.glksave

   and the Glk library state into autosave.plist in the same directory.
   Both are written to temp names and renamed into place, so a crash
   mid-save leaves the previous good pair intact.  win_autosave() then
   tells the window server to snapshot the GUI under the same tag.

   This file owns only the files and the plist; everything engine-side
   (containers, window tags, call sites) lives in os_glk.cpp behind
   #ifdef SPATTERLIGHT.
*/

extern "C" {
#include "glk.h"
#include "glkimp.h"
#include "fileref.h"
}

#include <string>

#include "scarier-autosave.h"

#import "TempLibrary.h"

/* ---- file plumbing ------------------------------------------------------- */

static NSString *autosave_dirname(void)
{
    if (autosavedir == NULL)
        getautosavedir(const_cast<char *>(gsc_autosave_game_path()));
    if (autosavedir == NULL)
        return nil;
    NSString *dirname = [[NSFileManager defaultManager]
        stringWithFileSystemRepresentation:autosavedir length:strlen(autosavedir)];
    if (!dirname.length)
        return nil;
    return dirname;
}

bool scarier_autosave_exists(void)
{
    if (!gli_enable_autosave)
        return false;
    @autoreleasepool {
        NSString *dirname = autosave_dirname();
        if (!dirname)
            return false;
        NSString *gamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSString *libpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];
        NSFileManager *fileManager = [NSFileManager defaultManager];
        if (![fileManager fileExistsAtPath:gamepath])
            return false;
        if (![fileManager fileExistsAtPath:libpath]) {
            /* A glksave with no plist can't be restored; delete it so it
             * does not cause trouble later. */
            [fileManager removeItemAtPath:gamepath error:nil];
            return false;
        }
        return true;
    }
}

bool scarier_autosave_wanted(void)
{
    if (!gli_enable_autosave)
        return false;
    /* Match Bocfel: no autosave before the first real event, after mere
     * rearrange/redraw wakeups, or on timer events when the timer pref is
     * off. */
    if ((int)lasteventtype == -1 || lasteventtype == evtype_Arrange ||
        lasteventtype == evtype_Redraw ||
        (lasteventtype == evtype_Timer && !gli_enable_autosave_on_timer))
        return false;
    return true;
}

void scarier_autosave_discard(void)
{
    @autoreleasepool {
        NSString *dirname = autosave_dirname();
        if (!dirname)
            return;
        NSFileManager *fileManager = [NSFileManager defaultManager];
        for (NSString *name in @[ @"autosave.glksave", @"autosave.plist",
                                  @"autosave-bak.glksave", @"autosave-bak.plist" ])
            [fileManager removeItemAtPath:[dirname stringByAppendingPathComponent:name]
                                    error:nil];
    }
}

/* Move any current "final" file to its -bak name and the freshly written
 * temp file into the final position. */
static bool move_into_place(NSString *dirname, NSString *tmpname,
                            NSString *finalname, NSString *bakname)
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *tmppath = [dirname stringByAppendingPathComponent:tmpname];
    NSString *finalpath = [dirname stringByAppendingPathComponent:finalname];
    NSString *bakpath = [dirname stringByAppendingPathComponent:bakname];

    [fileManager removeItemAtPath:bakpath error:nil];
    [fileManager moveItemAtPath:finalpath toPath:bakpath error:nil];

    NSError *error = nil;
    if (![fileManager moveItemAtPath:tmppath toPath:finalpath error:&error]) {
        NSLog(@"scarier autosave: could not move %@ to final position: %@", tmpname, error);
        /* Put the old file back so the previous autosave stays usable. */
        [fileManager moveItemAtPath:bakpath toPath:finalpath error:nil];
        return false;
    }
    return true;
}

/* ---- the frontend state, as plist archive extras ------------------------- */

static ScarierGlkFrontendState frontend_state;

static void scarier_library_archive(TempLibrary *library, NSCoder *encoder)
{
    (void)library;
    const ScarierGlkFrontendState *st = &frontend_state;
    [encoder encodeInt32:st->mainwintag forKey:@"scarier_mainwintag"];
    [encoder encodeInt32:st->statuswintag forKey:@"scarier_statuswintag"];
    [encoder encodeInt32:st->sidewintag forKey:@"scarier_sidewintag"];
    [encoder encodeInt32:st->mapwintag forKey:@"scarier_mapwintag"];
    [encoder encodeInt32:st->gfxwintag forKey:@"scarier_gfxwintag"];
    [encoder encodeInt32:st->transcripttag forKey:@"scarier_transcripttag"];
    [encoder encodeInt32:st->inputlogtag forKey:@"scarier_inputlogtag"];
    [encoder encodeInt32:st->readlogtag forKey:@"scarier_readlogtag"];
    [encoder encodeInt32:st->soundchanneltag forKey:@"scarier_soundchanneltag"];
    for (int i = 0; i < 9; i++) {
        [encoder encodeInt32:st->a5_channeltags[i]
                      forKey:[NSString stringWithFormat:@"scarier_a5_channeltag%d", i]];
        [encoder encodeInt32:(int32_t)st->a5_chan_sound[i]
                      forKey:[NSString stringWithFormat:@"scarier_a5_chan_sound%d", i]];
    }
    [encoder encodeInt32:st->seen_input forKey:@"scarier_seen_input"];
    [encoder encodeInt32:(int32_t)st->title_image forKey:@"scarier_title_image"];
    [encoder encodeInt64:st->title_offset forKey:@"scarier_title_offset"];
    [encoder encodeInt64:st->title_length forKey:@"scarier_title_length"];
    [encoder encodeInt32:st->map_shown forKey:@"scarier_map_shown"];
    [encoder encodeInt32:st->map_at_top forKey:@"scarier_map_at_top"];
    [encoder encodeInt32:st->map_zoom forKey:@"scarier_map_zoom"];
    [encoder encodeInt32:st->rng_usenative forKey:@"scarier_rng_usenative"];
    for (int i = 0; i < 4; i++)
        [encoder encodeInt32:(int32_t)st->rng_state[i]
                      forKey:[NSString stringWithFormat:@"scarier_rng_state%d", i]];
}

static void scarier_library_unarchive(TempLibrary *library, NSCoder *decoder)
{
    (void)library;
    ScarierGlkFrontendState *st = &frontend_state;
    st->mainwintag = [decoder decodeInt32ForKey:@"scarier_mainwintag"];
    st->statuswintag = [decoder decodeInt32ForKey:@"scarier_statuswintag"];
    st->sidewintag = [decoder decodeInt32ForKey:@"scarier_sidewintag"];
    st->mapwintag = [decoder decodeInt32ForKey:@"scarier_mapwintag"];
    st->gfxwintag = [decoder decodeInt32ForKey:@"scarier_gfxwintag"];
    st->transcripttag = [decoder decodeInt32ForKey:@"scarier_transcripttag"];
    st->inputlogtag = [decoder decodeInt32ForKey:@"scarier_inputlogtag"];
    st->readlogtag = [decoder decodeInt32ForKey:@"scarier_readlogtag"];
    st->soundchanneltag = [decoder decodeInt32ForKey:@"scarier_soundchanneltag"];
    for (int i = 0; i < 9; i++) {
        st->a5_channeltags[i] = [decoder decodeInt32ForKey:
            [NSString stringWithFormat:@"scarier_a5_channeltag%d", i]];
        st->a5_chan_sound[i] = (uint32_t)[decoder decodeInt32ForKey:
            [NSString stringWithFormat:@"scarier_a5_chan_sound%d", i]];
    }
    st->seen_input = [decoder decodeInt32ForKey:@"scarier_seen_input"];
    st->title_image = (uint32_t)[decoder decodeInt32ForKey:@"scarier_title_image"];
    st->title_offset = [decoder decodeInt64ForKey:@"scarier_title_offset"];
    st->title_length = [decoder decodeInt64ForKey:@"scarier_title_length"];
    st->map_shown = [decoder decodeInt32ForKey:@"scarier_map_shown"];
    st->map_at_top = [decoder decodeInt32ForKey:@"scarier_map_at_top"];
    st->map_zoom = [decoder decodeInt32ForKey:@"scarier_map_zoom"];
    st->rng_usenative =
        [decoder containsValueForKey:@"scarier_rng_usenative"]
            ? [decoder decodeInt32ForKey:@"scarier_rng_usenative"] : -1;
    for (int i = 0; i < 4; i++)
        st->rng_state[i] = (uint32_t)[decoder
            decodeInt32ForKey:[NSString stringWithFormat:@"scarier_rng_state%d", i]];
}

/* ---- save ---------------------------------------------------------------- */

void scarier_autosave_write(const std::string &engine_state)
{
    /* Unconditional: an earlier scarier_autosave_exists() probe may already
     * have resolved the directory NAME (getautosavedir) without creating
     * the directory itself.  createDirectoryAtURL is a no-op when it
     * already exists. */
    create_autosavedir(const_cast<char *>(gsc_autosave_game_path()));

    @autoreleasepool {
        NSString *dirname = autosave_dirname();
        if (!dirname) {
            win_showerror("Could not create autosave directory name.");
            return;
        }

        /* 1. The game state. */
        NSString *tmpgamepath = [dirname stringByAppendingPathComponent:@"autosave-tmp.glksave"];
        NSData *gamedata = [NSData dataWithBytes:engine_state.data()
                                          length:engine_state.size()];
        NSError *error = nil;
        if (![gamedata writeToFile:tmpgamepath options:NSDataWritingAtomic error:&error]) {
            NSLog(@"scarier autosave: game state write failed: %@", error);
            return;
        }
        if (!move_into_place(dirname, @"autosave-tmp.glksave",
                             @"autosave.glksave", @"autosave-bak.glksave"))
            return;

        /* 2. The Glk library state, with the frontend's tags appended. */
        gsc_stash_frontend_state(&frontend_state);

        TempLibrary *library = [[TempLibrary alloc] init];

        [TempLibrary setExtraArchiveHook:scarier_library_archive];
        NSError *archiveError = nil;
        NSData *archiveData = [NSKeyedArchiver archivedDataWithRootObject:library
                                                    requiringSecureCoding:NO
                                                                    error:&archiveError];
        [TempLibrary setExtraArchiveHook:nil];

        if (!archiveData) {
            NSLog(@"scarier autosave: library serialize failed: %@", archiveError);
            return;
        }

        NSString *tmplibpath = [dirname stringByAppendingPathComponent:@"autosave-tmp.plist"];
        if (![archiveData writeToFile:tmplibpath options:NSDataWritingAtomic error:&archiveError]) {
            NSLog(@"scarier autosave: library write failed: %@", archiveError);
            return;
        }
        move_into_place(dirname, @"autosave-tmp.plist",
                        @"autosave.plist", @"autosave-bak.plist");

        /* 3. Have the window server snapshot the GUI under the same tag. */
        win_autosave(library.autosaveTag);
    }
}

/* ---- restore ------------------------------------------------------------- */

bool scarier_autosave_read_game(std::string *out)
{
    @autoreleasepool {
        NSString *dirname = autosave_dirname();
        if (!dirname)
            return false;
        NSString *gamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
        NSError *error = nil;
        NSData *gamedata = [NSData dataWithContentsOfFile:gamepath options:0 error:&error];
        if (!gamedata) {
            NSLog(@"scarier autorestore: could not read game state: %@", error);
            return false;
        }
        out->assign((const char *)gamedata.bytes, (size_t)gamedata.length);
        return true;
    }
}

bool scarier_autosave_restore_library(void)
{
    if (!gli_enable_autosave)
        return false;
    @autoreleasepool {
        NSString *dirname = autosave_dirname();
        if (!dirname)
            return false;
        NSString *libpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];

        NSError *error = nil;
        TempLibrary *newlib = nil;
        NSData *libdata = [NSData dataWithContentsOfFile:libpath options:0 error:&error];
        if (libdata) {
            NSKeyedUnarchiver *unarchiver =
                [[NSKeyedUnarchiver alloc] initForReadingFromData:libdata error:&error];
            if (unarchiver) {
                unarchiver.requiresSecureCoding = NO;
                [TempLibrary setExtraUnarchiveHook:scarier_library_unarchive];
                newlib = (TempLibrary *)[unarchiver decodeTopLevelObjectForKey:NSKeyedArchiveRootObjectKey
                                                                         error:&error];
                [TempLibrary setExtraUnarchiveHook:nil];
                [unarchiver finishDecoding];
            }
        }
        if (!newlib) {
            NSLog(@"scarier autorestore: could not restore library state: %@", error);
            return false;
        }
        [newlib updateFromLibrary];
        gsc_recover_frontend_state(&frontend_state);
        [newlib updateFromLibraryLate];
    }
    return true;
}
