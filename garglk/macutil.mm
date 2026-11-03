// Copyright (C) 2026 by the Gargoyle developers.
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

#include "garglk.h"

void garglk_mac_set_dock_policy(bool hide)
{
    // IPC interpreter children create a QApplication (needed for Qt timers /
    // event processing) but must not appear as separate Dock apps; the parent
    // Gargoyle.app owns all game windows.
    if (hide) {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
    } else {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    }
}

void garglk_mac_set_windows_menu(void *ns_menu)
{
    auto *menu = static_cast<NSMenu *>(ns_menu);
    // Calling setWindowsMenu: again on the same NSMenu makes AppKit append
    // another copy of Fill / Center / Move & Resize / … each time the app
    // is reactivated. Only (re)register when the menu instance changes.
    if (menu == nullptr || [NSApp windowsMenu] == menu) {
        return;
    }
    [NSApp setWindowsMenu:menu];
}

void garglk_mac_miniaturize_key_window()
{
    [[NSApp keyWindow] performMiniaturize:nil];
}

void garglk_mac_zoom_key_window()
{
    [[NSApp keyWindow] performZoom:nil];
}

void garglk_mac_arrange_in_front()
{
    [NSApp arrangeInFront:nil];
}
