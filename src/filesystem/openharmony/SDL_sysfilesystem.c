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

#ifdef SDL_FILESYSTEM_OPENHARMONY


// !!! FIXME: HACK to prevent <AbilityKit/ability_runtime/start_options.h> from including. It has '&' instead of '*' for some args, which suggests it's only been tested with C++.
#define ABILITY_RUNTIME_START_OPTIONS_H
typedef enum AbilityRuntime_StartOptions AbilityRuntime_StartOptions;


#include <sys/stat.h>
#include <AbilityKit/ability_runtime/application_context.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// System dependent filesystem routines

#include "../SDL_sysfilesystem.h"

char *SDL_SYS_GetBasePath(void)
{
    int32_t writelen = 0;
    size_t slen = 128;
    char *retval = NULL;
    while (true) {
        void *ptr = SDL_realloc(retval, slen + 1);
        if (!ptr) {
            SDL_free(retval);
            return NULL;
        }
        retval = (char *) ptr;

        const AbilityRuntime_ErrorCode rc = OH_AbilityRuntime_ApplicationContextGetFilesDir(retval, slen, &writelen);
        if (rc == ABILITY_RUNTIME_ERROR_CODE_NO_ERROR) {
            break;
        } else if (rc != ABILITY_RUNTIME_ERROR_CODE_PARAM_INVALID) {
            SDL_SetError("OH_AbilityRuntime_ApplicationContextGetBundleName failed: %d", (int) rc);
            SDL_free(retval);
            return NULL;
        }
        slen *= 2;  // try again with a bigger buffer.
    }

    void *ptr = SDL_realloc(retval, writelen + 2);  // try to shrink the buffer.
    if (ptr) {
        retval = (char *) ptr;
    }
    retval[writelen] = '/';
    retval[writelen] = '\0';
    return retval;
}

char *SDL_SYS_GetExeName(void)
{
    char buffer[128];
    int32_t writelen = 0;
    const AbilityRuntime_ErrorCode rc = OH_AbilityRuntime_ApplicationContextGetBundleName(buffer, (int32_t) sizeof (buffer), &writelen);
    if (rc != ABILITY_RUNTIME_ERROR_CODE_NO_ERROR) {
        SDL_SetError("OH_AbilityRuntime_ApplicationContextGetBundleName failed: %d", (int) rc);
        return NULL;
    }
    return SDL_strdup(buffer);
}

char *SDL_SYS_GetPrefPath(const char *org, const char *app)
{
    int32_t writelen = 0;
    size_t slen = 128;
    char *prefdir = NULL;
    while (true) {
        void *ptr = SDL_realloc(prefdir, slen + 1);
        if (!ptr) {
            SDL_free(prefdir);
            return NULL;
        }
        prefdir = (char *) ptr;

        const AbilityRuntime_ErrorCode rc = OH_AbilityRuntime_ApplicationContextGetPreferencesDir(prefdir, slen, &writelen);
        if (rc == ABILITY_RUNTIME_ERROR_CODE_NO_ERROR) {
            break;
        } else if (rc != ABILITY_RUNTIME_ERROR_CODE_PARAM_INVALID) {
            SDL_SetError("OH_AbilityRuntime_ApplicationContextGetBundleName failed: %d", (int) rc);
            SDL_free(prefdir);
            return NULL;
        }
        slen *= 2;  // try again with a bigger buffer.
    }

    char *retval = NULL;
    if (SDL_asprintf(&retval, "%s/%s/", prefdir, app) < 0) {
        retval = NULL;
    }

    mkdir(prefdir, 0755);
    mkdir(retval, 0755);

    SDL_free(prefdir);

    return retval;
}

char *SDL_SYS_GetUserFolder(SDL_Folder folder)
{
    SDL_Unsupported();
    return NULL;
}

#endif // SDL_FILESYSTEM_OPENHARMONY
