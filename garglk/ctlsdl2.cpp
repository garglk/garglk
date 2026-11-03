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

#ifdef _WIN32
#define SDL_MAIN_HANDLED
#endif

#include <array>

#include <SDL.h>

#include "glk.h"
#include "garglk.h"

// Opens the first attached game controller, if any; see
// gli_controller_poll() below for button polling and input dispatch.

static SDL_GameController *gli_controller = nullptr;

namespace {

// The buttons gli_controller_poll() tracks for edge (press/release)
// detection, and the Glk keycode each dispatches to
// gli_input_handle_key() on press (see event.cpp) -- the same
// function keyboard input already goes through, so focus tracking,
// scrollback paging, etc. all work identically regardless of input
// source. Dispatch is press-edge only, single-shot: holding a button
// does not auto-repeat (matching how a physical key held down would
// need OS-level key-repeat, which this doesn't emulate).
//
// SDL_CONTROLLER_BUTTON_START has no mapping yet: opening Gargoyle's
// config overlay is an app-level Qt action, not a Glk keycode, and is
// deferred to whenever that gets wired up.
struct TrackedButton {
    SDL_GameControllerButton button;
    glui32 keycode;
    bool pressed = false;
};

std::array<TrackedButton, 8> gli_tracked_buttons {{
    { SDL_CONTROLLER_BUTTON_DPAD_UP, keycode_Up },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN, keycode_Down },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT, keycode_Left },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, keycode_Right },
    { SDL_CONTROLLER_BUTTON_A, keycode_Return },
    { SDL_CONTROLLER_BUTTON_B, keycode_Escape },
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, keycode_PageUp },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, keycode_PageDown },
}};

} // namespace

void gli_initialize_controller()
{
    if (!gli_conf_controller) {
        return;
    }

    SDL_SetMainReady();

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        gli_strict_warning("controller: SDL init failed\n");
        gli_strict_warning(SDL_GetError());
        return;
    }

    // Unlike the SDL2/SDL3 sound backends (sndsdl2.cpp, sndsdl3.cpp),
    // no SDL_QuitSubSystem()/handle-close teardown is registered here:
    // the OS reclaims everything on process exit, and neither backend
    // bothers with explicit cleanup either.

    // Button state is read directly via SDL_GameControllerGetButton()
    // in gli_controller_poll() rather than via the SDL event queue, so
    // tell SDL not to generate queued events for this subsystem;
    // otherwise nothing ever drains them and the queue grows for the
    // life of the process.
    SDL_GameControllerEventState(SDL_IGNORE);

    int num_joysticks = SDL_NumJoysticks();
    if (num_joysticks < 0) {
        gli_strict_warning("controller: unable to enumerate joysticks\n");
        gli_strict_warning(SDL_GetError());
        return;
    }

    for (int i = 0; i < num_joysticks; i++) {
        if (SDL_IsGameController(i)) {
            gli_controller = SDL_GameControllerOpen(i);
            if (gli_controller == nullptr) {
                gli_strict_warning("controller: failed to open controller\n");
                gli_strict_warning(SDL_GetError());
                continue;
            }
            break;
        }
    }
}

// Detects press edges on the tracked buttons and dispatches the
// mapped Glk keycode for each. gli_controller_poll() always runs on
// the Qt main thread (it's driven by a QTimer owned by the app-wide
// QApplication), the same thread all other input handling runs on, so
// calling gli_input_handle_key() here is no different from it being
// called from View::keyPressEvent() -- except that the caller (unlike
// keyPressEvent(), which unconditionally sets sysqt.cpp's private
// refresh_needed flag) has no access to that flag, so it can't signal
// "something happened, repaint" itself. Returning whether any key was
// dispatched lets the caller do that instead.
bool gli_controller_poll()
{
    if (gli_controller == nullptr) {
        return false;
    }

    SDL_GameControllerUpdate();

    bool dispatched = false;

    for (auto &tracked : gli_tracked_buttons) {
        bool pressed = SDL_GameControllerGetButton(gli_controller, tracked.button) != 0;
        if (pressed && !tracked.pressed) {
            gli_input_handle_key(tracked.keycode);
            dispatched = true;
        }
        tracked.pressed = pressed;
    }

    return dispatched;
}
