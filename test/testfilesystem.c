/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple test of filesystem functions. */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;
    char *base_path;
    char *pref_path;

Uint64 prevticks = 0;
#define STOPWATCH(what) { const Uint64 now = SDL_GetTicks(); SDL_Log("STOPWATCH: Step '%s' took %u ticks", what, (unsigned int) (now - prevticks)); prevticks = now; }
STOPWATCH("Startup");

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
STOPWATCH("SDLTest_CommonCreateState");
    if (state == NULL) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
STOPWATCH("SDL_LogSetPriority");

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }
STOPWATCH("SDLTest_CommonDefaultArgs");

    if (SDL_Init(0) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init() failed: %s\n", SDL_GetError());
        return 1;
    }
STOPWATCH("SDL_Init");

    base_path = SDL_GetBasePath();
STOPWATCH("SDL_GetBasePath");
    if (base_path == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find base path: %s\n",
                     SDL_GetError());
    } else {
        SDL_Log("base path: '%s'\n", base_path);
        SDL_free(base_path);
    }

    pref_path = SDL_GetPrefPath("libsdl", "test_filesystem");
STOPWATCH("SDL_GetPrefPath(\"libsdl\")");
    if (pref_path == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find pref path: %s\n",
                     SDL_GetError());
    } else {
        SDL_Log("pref path: '%s'\n", pref_path);
        SDL_free(pref_path);
    }

    pref_path = SDL_GetPrefPath(NULL, "test_filesystem");
STOPWATCH("SDL_GetPrefPath(NULL)");
    if (pref_path == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find pref path without organization: %s\n",
                     SDL_GetError());
    } else {
        SDL_Log("pref path: '%s'\n", pref_path);
        SDL_free(pref_path);
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
