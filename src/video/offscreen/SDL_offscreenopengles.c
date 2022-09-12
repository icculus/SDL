/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_OFFSCREEN && SDL_VIDEO_OPENGL_EGL

#include "SDL_offscreenopengles.h"
#include "SDL_offscreenvideo.h"
#include "SDL_offscreenwindow.h"

/* EGL implementation of SDL OpenGL support */

int
OFFSCREEN_GLES_LoadLibrary(_THIS, const char* path)
{
    return ( (SDL_EGL_LoadLibraryOnly(_this, path) == 0) &&
             (SDL_EGL_InitializeOffscreen(_this, 0) == 0) &&
             (SDL_EGL_ChooseConfig(_this) == 0) ) ? 0 : -1;
}

SDL_GLContext
OFFSCREEN_GLES_CreateContext(_THIS, SDL_Window* window)
{
    OFFSCREEN_Window* offscreen_window = window->driverdata;

    SDL_GLContext context;
    context = SDL_EGL_CreateContext(_this, offscreen_window->egl_surface);

    return context;
}

int
OFFSCREEN_GLES_MakeCurrent(_THIS, SDL_Window* window, SDL_GLContext context)
{
    if (window) {
        EGLSurface egl_surface = ((OFFSCREEN_Window*)window->driverdata)->egl_surface;
        return SDL_EGL_MakeCurrent(_this, egl_surface, context);
    } else {
        return SDL_EGL_MakeCurrent(_this, NULL, NULL);
    }
}

int
OFFSCREEN_GLES_SwapWindow(_THIS, SDL_Window* window)
{
    OFFSCREEN_Window* offscreen_wind = window->driverdata;

    return SDL_EGL_SwapBuffers(_this, offscreen_wind->egl_surface);
}

#endif /* SDL_VIDEO_DRIVER_OFFSCREEN && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
