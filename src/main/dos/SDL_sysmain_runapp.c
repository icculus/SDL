/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "SDL_internal.h"

#ifdef SDL_PLATFORM_DOS

#include <sys/nearptr.h>

// this locks .data, .bss, .text, and the stack. In SDL_RunApp(), we'll adjust this flag so future malloc() calls aren't locked by default.
#include <crt0.h>
int _crt0_startup_flags = _CRT0_FLAG_LOCK_MEMORY; // | _CRT_FLAG_NONMOVE_SBRK;

const char *SDL_argv0 = NULL;

int SDL_RunApp(int argc, char *argv[], SDL_main_func mainFunction, void * reserved)
{
    _crt0_startup_flags &= ~_CRT0_FLAG_LOCK_MEMORY;  // don't lock further allocations by default...so data, code, and stack are locked but not buffers from future malloc() calls.

    if (!__djgpp_nearptr_enable()) {
        fprintf(stderr, "__djgpp_nearptr_enable() failed!\n");
        return 1;
    }

    SDL_argv0 = argv ? argv[0] : NULL;

    SDL_SetMainReady();
    return mainFunction(argc, argv);
}

#endif

