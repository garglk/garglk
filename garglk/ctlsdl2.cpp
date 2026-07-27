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
#include <cstdlib>
#include <iostream>

#include <SDL.h>

#include "glk.h"
#include "garglk.h"

// Opens the first attached game controller, if any. Button polling and
// input dispatch are added separately; this is just subsystem
// init/teardown.

static SDL_GameController *gli_controller = nullptr;

namespace {

// The buttons gli_controller_poll() tracks for edge (press/release)
// detection. Only the buttons the default input mapping will use are
// tracked; this is not a general-purpose "read every button" API.
struct TrackedButton {
    SDL_GameControllerButton button;
    const char *name;
    bool pressed = false;
};

std::array<TrackedButton, 9> gli_tracked_buttons {{
    { SDL_CONTROLLER_BUTTON_DPAD_UP, "dpad_up" },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN, "dpad_down" },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT, "dpad_left" },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, "dpad_right" },
    { SDL_CONTROLLER_BUTTON_A, "a" },
    { SDL_CONTROLLER_BUTTON_B, "b" },
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, "left_shoulder" },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "right_shoulder" },
    { SDL_CONTROLLER_BUTTON_START, "start" },
}};

} // namespace

static void gli_shutdown_controller()
{
    if (gli_controller != nullptr) {
        SDL_GameControllerClose(gli_controller);
        gli_controller = nullptr;
    }

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void gli_initialize_controller()
{
    SDL_SetMainReady();

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        gli_strict_warning("controller: SDL init failed\n");
        gli_strict_warning(SDL_GetError());
        return;
    }

    if (std::atexit(gli_shutdown_controller) != 0) {
        gli_strict_warning("controller: unable to register atexit handler");
    }

    // Button state is read directly via SDL_GameControllerGetButton()
    // in gli_controller_poll() rather than via the SDL event queue, so
    // tell SDL not to generate queued events for this subsystem;
    // otherwise nothing ever drains them and the queue grows for the
    // life of the process.
    SDL_GameControllerEventState(SDL_IGNORE);

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
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

// Detects press/release edges on the tracked buttons and logs them.
// This is a temporary diagnostic: real input dispatch (synthetic
// QKeyEvents) replaces the logging in a follow-up change.
void gli_controller_poll()
{
    if (gli_controller == nullptr) {
        return;
    }

    SDL_GameControllerUpdate();

    for (auto &tracked : gli_tracked_buttons) {
        bool pressed = SDL_GameControllerGetButton(gli_controller, tracked.button) != 0;
        if (pressed != tracked.pressed) {
            tracked.pressed = pressed;
            std::cerr << "controller: " << tracked.name << (pressed ? " pressed" : " released") << '\n';
        }
    }
}
