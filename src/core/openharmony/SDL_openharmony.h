/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_openharmony_h
#define SDL_openharmony_h

// Set up for C function definitions, even when using C++
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

void SDL_DebugLogOpenHarmonyInfo(void);
const char *SDL_GetOpenHarmonySystemLocale(void);

bool SDL_OpenHarmonyOpenURL(const char *url);

bool SDL_IsOpenHarmonyPhone(void);
bool SDL_IsOpenHarmonyTablet(void);
bool SDL_IsOpenHarmonyTV(void);

bool SDL_OpenHarmonyRawFileOpen(void **puserdata, const char *fileName, const char *mode);
Sint64 SDL_OpenHarmonyRawFileSize(void *userdata);
Sint64 SDL_OpenHarmonyRawFileSeek(void *userdata, Sint64 offset, SDL_IOWhence whence);
size_t SDL_OpenHarmonyRawFileRead(void *userdata, void *buffer, size_t size, SDL_IOStatus *status);
bool SDL_OpenHarmonyRawFileClose(void *userdata);
bool SDL_OpenHarmonyEnumerateAssetDirectory(const char *path, SDL_EnumerateDirectoryCallback cb, void *userdata);
bool SDL_OpenHarmonyGetAssetPathInfo(const char *path, SDL_PathInfo *info);

#define SDL_PlatformEnumerateAssetDirectory SDL_OpenHarmonyEnumerateAssetDirectory
#define SDL_GetPlatformInternalStoragePath SDL_GetOpenHarmonyInternalStoragePath
#define SDL_PlatformGetAssetPathInfo SDL_OpenHarmonyGetAssetPathInfo

// Ends C function definitions when using C++
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif // SDL_openharmony_h
