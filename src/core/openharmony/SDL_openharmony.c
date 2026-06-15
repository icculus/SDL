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

#ifdef SDL_PLATFORM_OPENHARMONY

#include <deviceinfo.h>

#include "SDL_openharmony.h"

int SDL_GetOpenHarmonySDKVersion(void)
{
    static int sdk_version;
    if (!sdk_version) {
        sdk_version = OH_GetSdkApiVersion();
    }
    return sdk_version;
}

void SDL_DebugLogOpenHarmonyInfo(void)
{
    static bool already_logged = false;
    if (!already_logged) {
        already_logged = true;
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "SDL OpenHarmony system/device info:");
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Device type: %s", OH_GetDeviceType());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Manufacturer: %s", OH_GetManufacture());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Brand: %s", OH_GetBrand());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Market name: %s", OH_GetMarketName());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Product series: %s", OH_GetProductSeries());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Product model: %s", OH_GetProductModel());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Software model: %s", OH_GetSoftwareModel());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Hardware model: %s", OH_GetHardwareModel());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Bootloader version: %s", OH_GetBootloaderVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - ABI list: %s", OH_GetAbiList());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Security patch tag: %s", OH_GetSecurityPatchTag());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Display version: %s", OH_GetDisplayVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Incremental version: %s", OH_GetIncrementalVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - OS release type: %s", OH_GetOsReleaseType());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - OS full name: %s", OH_GetOSFullName());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - SDK API version: %d", SDL_GetOpenHarmonySDKVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - First API version: %d", OH_GetFirstApiVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Version ID: %s", OH_GetVersionId());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Build type: %s", OH_GetBuildType());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Build user: %s", OH_GetBuildUser());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Build host: %s", OH_GetBuildHost());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Build time: %s", OH_GetBuildTime());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Build root hash: %s", OH_GetBuildRootHash());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Distribution OS name: %s", OH_GetDistributionOSName());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Distribution OS version: %s", OH_GetDistributionOSVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Distribution OS API version: %d", OH_GetDistributionOSApiVersion());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, " - Distribution OS release type: %s", OH_GetDistributionOSReleaseType());
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "");
    }
}

#endif // SDL_PLATFORM_OPENHARMONY

