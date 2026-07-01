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

// !!! FIXME: which of these headers do we actually need?
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <node_api.h>
#include <node_api_types.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <AbilityKit/ability_runtime/application_context.h>
#include <napi/native_api.h>
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


// ArkTS/C bridging...

// Callbacks into our custom XComponent.
static void SDL_XComponent_OnSurfaceCreatedCallback(OH_NativeXComponent* component, void* window)
{
    extern void SDL_OpenHarmonyMainSurfaceCreated(void);  // this is in src/main/openharmony/SDL_sysmain_runapp.c
    SDL_OpenHarmonyMainSurfaceCreated();  // start the actual native code app if this is the first surface.
}

static void SDL_XComponent_OnFrameCallback(OH_NativeXComponent* component, uint64_t timestamp, uint64_t targetTimestamp)
{
    extern void SDL_OpenHarmonyOnFrameCallback(void);  // this is in src/main/openharmony/SDL_sysmain_callbacks.c
    SDL_OpenHarmonyOnFrameCallback();  // This fires SDL_AppInterate.
}

// !!! FIXME: write these.
static void SDL_XComponent_OnSurfaceChangedCallback(OH_NativeXComponent* component, void* window) {}
static void SDL_XComponent_OnSurfaceDestroyedCallback(OH_NativeXComponent* component, void* window) {}
static void SDL_XComponent_DispatchTouchEventCallback(OH_NativeXComponent* component, void* window) {}


// This is called by SDL_RegisterNativeInterfaces when the library is loaded, which sets up the entry points where
//  ArkTS code can call into our native code.
static napi_value SDL_Init_Native_Interfaces(napi_env env, napi_value exports)
{
    // Functions that we want to be able to call from ArkTS go here.
    // (declare them in C as `napi_value MyFunctionName(napi_env env, napi_callback_info info);`)
#if 0
    napi_property_descriptor desc[] = {
        { "myFunctionName", NULL, MyFunctionName, NULL, NULL, NULL, napi_default, NULL },
    };
    napi_define_properties(env, exports, SDL_arraysize(desc), desc);
#endif


    // Wire into our XComponent, so we can take control from C code. If any of this fails, I assume the app will either blow up or do nothing.
    napi_value exportInstance = NULL;
    if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance) != napi_ok) {
        return exports;
    }

    OH_NativeXComponent *nativeXComponent = NULL;
    if (napi_unwrap(env, exportInstance, (void **) &nativeXComponent) != napi_ok) {
        return exports;
    }

    static OH_NativeXComponent_Callback xcomp_callbacks = {  // this MUST be static! It keeps a pointer to this, and doesn't make a copy, afaict!
        .OnSurfaceCreated = SDL_XComponent_OnSurfaceCreatedCallback,
        .OnSurfaceChanged = SDL_XComponent_OnSurfaceChangedCallback,
        .OnSurfaceDestroyed = SDL_XComponent_OnSurfaceDestroyedCallback,
        .DispatchTouchEvent = SDL_XComponent_DispatchTouchEventCallback
    };
    OH_NativeXComponent_RegisterCallback(nativeXComponent, &xcomp_callbacks);
    OH_NativeXComponent_RegisterOnFrameCallback(nativeXComponent, SDL_XComponent_OnFrameCallback);

    return exports;
}

// This runs when libSDL3.so loads, and registers the NAPI module, so ArkTS can call into this to get going.
void __attribute__((constructor)) SDL_RegisterNativeInterfaces(void)
{
    static napi_module sdl_napi_module = {
        .nm_version = 1,
        .nm_flags = 0,
        .nm_filename = NULL,
        .nm_register_func = SDL_Init_Native_Interfaces,
        .nm_modname = "SDL3",
        .nm_priv = ((void*)0),
        .reserved = { 0 },
    };
    napi_module_register(&sdl_napi_module);
}

#endif // SDL_PLATFORM_OPENHARMONY

