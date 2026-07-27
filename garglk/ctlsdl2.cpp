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

#include <cstdlib>

#include <SDL.h>

#include "glk.h"
#include "garglk.h"

// Opens the first attached game controller, if any. Button polling and
// input dispatch are added separately; this is just subsystem
// init/teardown.

static SDL_GameController *gli_controller = nullptr;

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
