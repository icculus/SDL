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

#include "../../events/SDL_events_c.h"

// !!! FIXME: work around some C++isms that leaked into OpenHarmony headers.
// !!! FIXME: HACK to prevent <AbilityKit/ability_runtime/start_options.h> from including. It has '&' instead of '*' for some args, which suggests it's only been tested with C++.
#define ABILITY_RUNTIME_START_OPTIONS_H
typedef enum AbilityRuntime_StartOptions AbilityRuntime_StartOptions;


// !!! FIXME: <rawfile/raw_file.h> has functions with C++ references. Prevent raw_file_manager.h from including it, and define the parts we need here.
#define GLOBAL_RAW_FILE_H
typedef struct RawFile RawFile;
typedef struct RawFile64 RawFile64;
typedef struct { int fd; long start; long length; } RawFileDescriptor;
typedef struct { int fd; int64_t start; int64_t length; } RawFileDescriptor64;
int64_t OH_ResourceManager_GetRawFileSize64(RawFile64 *rawFile) __attribute__((__availability__(ohos, introduced=11.0.0)));
int OH_ResourceManager_SeekRawFile64(const RawFile64 *rawFile, int64_t offset, int whence) __attribute__((__availability__(ohos, introduced=11.0.0)));
int64_t OH_ResourceManager_ReadRawFile64(const RawFile64 *rawFile, void *buf, int64_t length) __attribute__((__availability__(ohos, introduced=11.0.0)));
int64_t OH_ResourceManager_GetRawFileRemainingLength64(const RawFile64 *rawFile) __attribute__((__availability__(ohos, introduced=11.0.0)));
int64_t OH_ResourceManager_GetRawFileOffset64(const RawFile64 *rawFile) __attribute__((__availability__(ohos, introduced=11.0.0)));
void OH_ResourceManager_CloseRawFile64(RawFile64 *rawFile) __attribute__((__availability__(ohos, introduced=11.0.0)));
bool OH_ResourceManager_GetRawFileDescriptor64(const RawFile64 *rawFile, RawFileDescriptor64 *descriptor) __attribute__((__availability__(ohos, introduced=11.0.0)));
bool OH_ResourceManager_ReleaseRawFileDescriptor64(const RawFileDescriptor64 *descriptor) __attribute__((__availability__(ohos, introduced=11.0.0)));


// !!! FIXME: these are defined as "const uint32_t VARNAME = VALUE;" in native_interface_xcomponent.h, which becomes a global variable in _our_ C code! Maybe C++ handles this differently...?
#define OH_XCOMPONENT_ID_LEN_MAX sdl_core_ohos_OH_XCOMPONENT_ID_LEN_MAX
#define OH_MAX_TOUCH_POINTS_NUMBER sdl_core_ohos_OH_MAX_TOUCH_POINTS_NUMBER
#include <ace/xcomponent/native_interface_xcomponent.h>

#include <AbilityKit/ability_runtime/application_context.h>
#include <deviceinfo.h>
#include <rawfile/raw_file_manager.h>
#include <hilog/log.h>
#include <stdlib.h>

// !!! FIXME: which of these headers do we actually need?
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <node_api.h>
#include <node_api_types.h>
#include <napi/native_api.h>

#include "SDL_openharmony.h"
#include "../../video/openharmony/SDL_openharmonyvideo.h"
#include "../../video/openharmony/SDL_openharmonyevents.h"

static napi_ref ability_object_ref = NULL;
static napi_ref atmanager_ref = NULL;
static napi_ref window_ref = NULL;
static NativeResourceManager *native_resource_mgr = NULL;
static napi_threadsafe_function req_permissions_threadsafefn = NULL;
static napi_threadsafe_function open_url_threadsafefn = NULL;
static napi_threadsafe_function change_sysbars_threadsafefn = NULL;
static napi_threadsafe_function change_screensaver_threadsafefn = NULL;
static char *system_locale = NULL;

int SDL_GetOpenHarmonySDKVersion(void)
{
    static int sdk_version;
    if (!sdk_version) {
        sdk_version = OH_GetSdkApiVersion();
    }
    return sdk_version;
}

bool SDL_IsOpenHarmonyPhone(void)
{
    static int isphone = -1;
    if (isphone == -1) {
        const char *devtype = OH_GetDeviceType();
        isphone = (devtype && ((SDL_strcmp(devtype, "phone") == 0) || (SDL_strcmp(devtype, "default") == 0))) ? 1 : 0;
    }
    return (isphone == 1);
}

bool SDL_IsOpenHarmonyTablet(void)
{
    static int istablet = -1;
    if (istablet == -1) {
        const char *devtype = OH_GetDeviceType();
        istablet = (devtype && (SDL_strcmp(devtype, "tablet") == 0)) ? 1 : 0;
    }
    return (istablet == 1);
}

bool SDL_IsOpenHarmonyTV(void)
{
    static int istv = -1;
    if (istv == -1) {
        const char *devtype = OH_GetDeviceType();
        istv = (devtype && (SDL_strcmp(devtype, "tv") == 0)) ? 1 : 0;
    }
    return (istv == 1);
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
    }
}

const char *SDL_GetOpenHarmonySystemLocale(void)
{
    return system_locale;
}

const char *SDL_GetOpenHarmonyInternalStoragePath(void)
{
    static char *files_path = NULL;
    if (!files_path) {
        char *path = NULL;
        int32_t writelen = 0;
        size_t slen = 128;
        while (true) {
            void *ptr = SDL_realloc(path, slen + 1);  // +1 to save space for null terminator.
            if (!ptr) {
                SDL_free(path);
                return NULL;
            }
            path = (char *) ptr;

            const AbilityRuntime_ErrorCode rc = OH_AbilityRuntime_ApplicationContextGetFilesDir(path, slen, &writelen);
            if (rc == ABILITY_RUNTIME_ERROR_CODE_NO_ERROR) {
                break;
            } else if (rc != ABILITY_RUNTIME_ERROR_CODE_PARAM_INVALID) {
                SDL_SetError("OH_AbilityRuntime_ApplicationContextGetBundleName failed: %d", (int) rc);
                SDL_free(path);
                return NULL;
            }
            slen *= 2;  // try again with a bigger buffer.
        }

        files_path = (char *) SDL_realloc(path, SDL_strlen(path) + 1);  // shrink it down.
        if (!files_path) {
            files_path = path;  // oh well, _don't_ shrink it down...
        }
    }

    return files_path;
}


// Filesystem stuff...

bool SDL_OpenHarmonyRawFileOpen(void **puserdata, const char *fileName, const char *mode)
{
    if (SDL_strncmp(fileName, "assets://", 9) == 0) {
        fileName += 9;
    }

    SDL_assert(native_resource_mgr != NULL);   // ArkTS should have sent us this at startup.
    SDL_assert(puserdata != NULL);

    if (mode && (SDL_strcmp(mode, "r") != 0) && (SDL_strcmp(mode, "rb") != 0)) {
        return SDL_SetError("RawFile access is read-only");
    }

    RawFile64 *rf64 = OH_ResourceManager_OpenRawFile64(native_resource_mgr, fileName);
    if (!rf64) {
        return SDL_SetError("Failed to open RawFile");
    }

    *puserdata = rf64;
    return true;
}

Sint64 SDL_OpenHarmonyRawFileSize(void *userdata)
{
    return (Sint64) OH_ResourceManager_GetRawFileSize64((RawFile64 *) userdata);
}

Sint64 SDL_OpenHarmonyRawFileSeek(void *userdata, Sint64 offset, SDL_IOWhence whence)
{
    const int ohwhence = (int) whence;  // these values happen to match.
    if (OH_ResourceManager_SeekRawFile64((const RawFile64 *) userdata, (int64_t) offset, ohwhence) < 0) {
        SDL_SetError("RawFile seek failed");
        return -1;
    }
    return (Sint64) OH_ResourceManager_GetRawFileOffset64((const RawFile64 *) userdata);
}

size_t SDL_OpenHarmonyRawFileRead(void *userdata, void *buffer, size_t size, SDL_IOStatus *status)
{
    const size_t br = (size_t) OH_ResourceManager_ReadRawFile64((const RawFile64 *) userdata, buffer, (int64_t) size);  // this returns 0 on eof/error, not a negative, so just cast to size_t.
    if (br < size) {
        if (OH_ResourceManager_GetRawFileRemainingLength64((const RawFile64 *) userdata) == 0) {
            *status = SDL_IO_STATUS_EOF;
        } else {
            *status = SDL_IO_STATUS_ERROR;
            SDL_SetError("RawFile read failed");
        }
    }
    return br;
}

bool SDL_OpenHarmonyRawFileClose(void *userdata)
{
    OH_ResourceManager_CloseRawFile64((RawFile64 *) userdata);
    return true;
}

bool SDL_OpenHarmonyEnumerateAssetDirectory(const char *path, SDL_EnumerateDirectoryCallback cb, void *userdata)
{
    const char *origpath = path;
    if (SDL_strncmp(path, "assets://", 9) == 0) {
        path += 9;
    }

    SDL_assert(native_resource_mgr != NULL);   // ArkTS should have sent us this at startup.
    RawDir *rawdir = OH_ResourceManager_OpenRawDir(native_resource_mgr, path);
    if (!rawdir) {
        return SDL_SetError("RawDir open failed");
    }

    SDL_EnumerationResult result = SDL_ENUM_CONTINUE;
    const int total = OH_ResourceManager_GetRawFileCount(rawdir);
    for (int i = 0; (i < total) && (result == SDL_ENUM_CONTINUE); i++) {
        const char *fname = OH_ResourceManager_GetRawFileName(rawdir, i);
        result = cb(userdata, origpath, fname);
    }

    OH_ResourceManager_CloseRawDir(rawdir);

    return (result != SDL_ENUM_FAILURE);
}

bool SDL_OpenHarmonyGetAssetPathInfo(const char *path, SDL_PathInfo *info)
{
    if (SDL_strncmp(path, "assets://", 9) == 0) {
        path += 9;
    }

    SDL_assert(native_resource_mgr != NULL);   // ArkTS should have sent us this at startup.
    SDL_zerop(info);
    if (OH_ResourceManager_IsRawDir(native_resource_mgr, path)) {
        info->type = SDL_PATHTYPE_DIRECTORY;
    } else {
        RawFile64 *rf64 = OH_ResourceManager_OpenRawFile64(native_resource_mgr, path);
        if (!rf64) {
            return SDL_SetError("No such file or directory");
        }
        info->type = SDL_PATHTYPE_FILE;
        info->size = (Uint64) OH_ResourceManager_GetRawFileSize64(rf64);
        OH_ResourceManager_CloseRawFile64(rf64);
    }
    return true;
}


// ArkTS/C bridging...

typedef struct RequestPermissionData
{
    char *permission;
    SDL_RequestOpenHarmonyPermissionCallback callback;
    void *callback_userdata;
} RequestPermissionData;

// AsyncCallback when CallJSRequestPermissions() finishes its work.
static napi_value SDL_NAPI_RequestPermissionResult(napi_env env, napi_callback_info info)
{
    RequestPermissionData *data = NULL;
    size_t argc = 2;
    napi_value argv[2];
    napi_get_cb_info(env, info, &argc, argv, NULL, (void **) &data);

    napi_value authResults = NULL; napi_get_named_property(env, argv[1], "authResults", &authResults);  // Array<number>
    napi_value result = NULL; napi_get_element(env, authResults, 0, &result);
    int32_t result32 = -1; napi_get_value_int32(env, result, &result32);

    const bool granted = (result32 == 0);
    data->callback(data->callback_userdata, data->permission, granted);

    SDL_free(data->permission);
    SDL_free(data);

    napi_value retval = NULL; napi_get_undefined(env, &retval);
    return retval;
}

// this function is called from the main Javascript thread when it's convenient to fire it.
static void CallJSRequestPermissions(napi_env env, napi_value js_callback, void *context, void *userdata)
{
    RequestPermissionData *data = (RequestPermissionData *) userdata;

    //atmanager.requestPermissionsFromUser(context: Context, permissionList: Array<Permissions>, requestCallback: AsyncCallback<PermissionRequestResult>): void;
    napi_value ability = NULL; napi_get_reference_value(env, ability_object_ref, &ability);
    napi_value atmanager = NULL; napi_get_reference_value(env, atmanager_ref, &atmanager);
    napi_value fn = NULL;
    napi_get_named_property(env, atmanager, "requestPermissionsFromUser", &fn);
    napi_value args[3];
    napi_get_named_property(env, ability, "context", &args[0]);
    napi_create_array_with_length(env, 1, &args[1]);
    napi_value str = NULL; napi_create_string_utf8(env, data->permission, NAPI_AUTO_LENGTH, &str);
    napi_set_element(env, args[1], 0, str);
    napi_create_function(env, NULL, 0, SDL_NAPI_RequestPermissionResult, data, &args[2]);
    napi_value rc = NULL; napi_call_function(env, atmanager, fn, SDL_arraysize(args), args, &rc);
    // okay, assuming this worked out, we'll get a callback to SDL_NAPI_RequestPermissionResult() at some point in the future (if we haven't already).
}

bool SDL_RequestOpenHarmonyPermission(const char *permission, SDL_RequestOpenHarmonyPermissionCallback cb, void *userdata)
{
    RequestPermissionData *data = NULL;

    if (!permission) {
        return SDL_InvalidParamError("permission");
    } else if (!cb) {
        return SDL_InvalidParamError("cb");
    } else if (!atmanager_ref) {
        return SDL_SetError("atManager not initialized");
    } else if ((data = (RequestPermissionData *) SDL_calloc(1, sizeof (*data))) == NULL) {
        return false;
    } else if ((data->permission = SDL_strdup(permission)) == NULL) {
        SDL_free(data);
        return false;
    }

    data->callback = cb;
    data->callback_userdata = userdata;

    return (napi_call_threadsafe_function(req_permissions_threadsafefn, data, napi_tsfn_nonblocking) == napi_ok);
}

// AsyncCallback when CallJSOpenURL() finishes its work.
static napi_value SDL_NAPI_OpenURLResult(napi_env env, napi_callback_info info)
{
    char **url = NULL;
    size_t argc = 1;
    napi_value argv[1];
    napi_get_cb_info(env, info, &argc, argv, NULL, (void **) &url);
    SDL_free(url);
    napi_value retval = NULL; napi_get_undefined(env, &retval);
    return retval;
}

// this function is called from the main Javascript thread when it's convenient to fire it.
static void CallJSOpenURL(napi_env env, napi_value js_callback, void *context, void *userdata)
{
    char *url = (char *) userdata;
    napi_value ability = NULL; napi_get_reference_value(env, ability_object_ref, &ability);
    napi_value abcontext = NULL; napi_get_named_property(env, ability, "context", &abcontext);
    napi_value startAbility = NULL; napi_get_named_property(env, abcontext, "startAbility", &startAbility);
    napi_value want = NULL; napi_create_object(env, &want);
    napi_value uri = NULL; napi_create_string_utf8(env, url, NAPI_AUTO_LENGTH, &uri);
    napi_set_named_property(env, want, "uri", uri);
    SDL_free(url);
    napi_value fn = NULL; napi_create_function(env, NULL, 0, SDL_NAPI_OpenURLResult, url, &fn);
    napi_value args[2] = { want, fn };
    napi_value rc = NULL; napi_call_function(env, abcontext, startAbility, SDL_arraysize(args), args, &rc);
}

bool SDL_OpenHarmonyOpenURL(const char *url)
{
    char *data = NULL;
    if (!url) {
        return SDL_InvalidParamError("url");
    } else if (!ability_object_ref) {
        return SDL_SetError("Ability not initialized");
    } else if ((data = SDL_strdup(url)) == NULL) {
        return false;
    }
    return (napi_call_threadsafe_function(open_url_threadsafefn, data, napi_tsfn_nonblocking) == napi_ok);
}


// AsyncCallback when CallJSChangeSysBars() finishes its work.
static napi_value SDL_NAPI_ChangeSysBarsResult(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL; napi_get_undefined(env, &retval);
    return retval;
}

// this function is called from the main Javascript thread when it's convenient to fire it.
static void CallJSChangeSysBars(napi_env env, napi_value js_callback, void *context, void *userdata)
{
    const intptr_t flags = (const intptr_t) userdata;
    const bool status_bar = (flags & (1 << 0)) != 0;
    const bool navigation_bar = (flags & (1 << 1)) != 0;
    int arrlen = 0;
    if (status_bar) { arrlen++; }
    if (navigation_bar) { arrlen++; }

    napi_value arr = NULL; napi_create_array_with_length(env, arrlen, &arr);
    arrlen = 0;
    if (status_bar) {
        napi_value str = NULL; napi_create_string_utf8(env, "status", NAPI_AUTO_LENGTH, &str);
        napi_set_element(env, arr, arrlen++, str);
    }
    if (navigation_bar) {
        napi_value str = NULL; napi_create_string_utf8(env, "navigation", NAPI_AUTO_LENGTH, &str);
        napi_set_element(env, arr, arrlen++, str);
    }

    napi_value fn = NULL; napi_create_function(env, NULL, 0, SDL_NAPI_ChangeSysBarsResult, NULL, &fn);
    napi_value window = NULL; napi_get_reference_value(env, window_ref, &window);
    napi_value setSystemBarEnable = NULL; napi_get_named_property(env, window, "setSystemBarEnable", &setSystemBarEnable);
    napi_value args[2] = { arr, fn };
    napi_value rc = NULL; napi_call_function(env, window, setSystemBarEnable, SDL_arraysize(args), args, &rc);
}

bool SDL_OpenHarmonyToggleSystemBars(bool status_bar, bool navigation_bar)
{
    const intptr_t flags = (status_bar ? (1 << 0) : 0) | (navigation_bar ? (1 << 1) : 0);
    if (!window_ref) {
        return SDL_SetError("Window not initialized");
    }
    return (napi_call_threadsafe_function(change_sysbars_threadsafefn, (void *) flags, napi_tsfn_nonblocking) == napi_ok);
}

// AsyncCallback when CallJSChangeScreenSaver() finishes its work.
static napi_value SDL_NAPI_ChangeScreenSaverResult(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL; napi_get_undefined(env, &retval);
    return retval;
}

// this function is called from the main Javascript thread when it's convenient to fire it.
static void CallJSChangeScreenSaver(napi_env env, napi_value js_callback, void *context, void *userdata)
{
    napi_value enable = NULL; napi_get_boolean(env, (userdata != NULL), &enable);
    napi_value fn = NULL; napi_create_function(env, NULL, 0, SDL_NAPI_ChangeScreenSaverResult, NULL, &fn);
    napi_value window = NULL; napi_get_reference_value(env, window_ref, &window);
    napi_value setWindowKeepScreenOn = NULL; napi_get_named_property(env, window, "setWindowKeepScreenOn", &setWindowKeepScreenOn);
    napi_value args[2] = { enable, fn };
    napi_value rc = NULL; napi_call_function(env, window, setWindowKeepScreenOn, SDL_arraysize(args), args, &rc);
}

bool SDL_OpenHarmonyChangeScreenSaver(bool enable)
{
    if (!window_ref) {
        return SDL_SetError("Window not initialized");
    }
    return (napi_call_threadsafe_function(change_screensaver_threadsafefn, (void *) (size_t) (enable ? 0x1 : 0x0), napi_tsfn_nonblocking) == napi_ok);
}


// Callbacks into our custom XComponent.
static void SDL_XComponent_OnSurfaceCreatedCallback(OH_NativeXComponent* component, void* window)
{
    SDL_assert(native_resource_mgr != NULL);   // ArkTS should have sent us this at startup. Are you using our startup scripts?
    extern void SDL_OpenHarmonyMainSurfaceCreated(void);  // this is in src/main/openharmony/SDL_sysmain_runapp.c
    SDL_OpenHarmonyVideoSurfaceCreated(component, window);  // this is in src/video/openharmony/SDL_openharmonyvideo.c
    SDL_OpenHarmonyMainSurfaceCreated();  // start the actual native code app if this is the first surface.
}

static void SDL_XComponent_OnFrameCallback(OH_NativeXComponent* component, uint64_t timestamp, uint64_t targetTimestamp)
{
    extern void SDL_OpenHarmonyOnFrameCallback(void);  // this is in src/main/openharmony/SDL_sysmain_callbacks.c
    SDL_OpenHarmonyOnFrameCallback();  // This fires SDL_AppInterate.
}

static void SDL_XComponent_OnSurfaceChangedCallback(OH_NativeXComponent* component, void* window)
{
    SDL_OpenHarmonyVideoSurfaceChanged(component, window);  // this is in src/video/openharmony/SDL_openharmonyvideo.c
}

static void SDL_XComponent_OnSurfaceDestroyedCallback(OH_NativeXComponent* component, void* window)
{
    SDL_OpenHarmonyVideoSurfaceDestroyed(component, window);  // this is in src/video/openharmony/SDL_openharmonyvideo.c
}

static void SDL_XComponent_DispatchTouchEventCallback(OH_NativeXComponent* component, void* window)
{
    SDL_OpenHarmonyDispatchTouchEvent(component, window);  // this is in src/video/openharmony/SDL_openharmonyvideo.c
}

static void SDL_XComponent_DispatchMouseEventCallback(OH_NativeXComponent* component, void* window)
{
    SDL_OpenHarmonyDispatchMouseEvent(component, window);  // this is in src/video/openharmony/SDL_openharmonyvideo.c
}

static void SDL_XComponent_DispatchHoverEventCallback(OH_NativeXComponent* component, bool isHover)
{
    // !!! FIXME: use this?
}


static char *CreateSDLStringFromNAPIValue(napi_env env, napi_value val)
{
    char *retval = NULL;
    size_t buflen = 0;
    if (napi_get_value_string_utf8(env, val, NULL, 0, &buflen) != napi_ok) {
        return NULL;
    } else if ((retval = (char *) SDL_malloc(buflen + 1)) == NULL) {
        return NULL;
    } else if (napi_get_value_string_utf8(env, val, retval, buflen + 1, &buflen) != napi_ok) {
        SDL_free(retval);
        return NULL;
    }
    return retval;
}

// Called when windowStage.loadContent finishes.
static napi_value SDL_NAPI_LoadContentResult(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value err = NULL;
    napi_get_cb_info(env, info, &argc, &err, NULL, NULL);
    napi_value code = NULL; napi_get_named_property(env, err, "code", &code);
    int32_t code32 = -1; napi_get_value_int32(env, code, &code32);
    
    if (code32 != 0) {
        OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "Failed to load the content page! Aborting!");
        exit(1);
    }
    OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "Succeeded in loading the content page. Startup may now continue.");

    napi_value retval = NULL; napi_get_undefined(env, &retval);
    return retval;
}

// Our native version of UIAbility.onDestroy().
static napi_value SDL_NAPI_UIAbilityOnDestroy(napi_env env, napi_callback_info info)
{
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    SDL_SendQuit();
    SDL_OnApplicationWillTerminate();
    napi_value retval = NULL; napi_get_undefined(env, &retval);
    return retval;
}

// Our native version of UIAbility.onForeground().
static napi_value SDL_NAPI_UIAbilityOnForeground(napi_env env, napi_callback_info info)
{
    SDL_OnApplicationWillEnterForeground();
    SDL_OnApplicationDidEnterForeground();
    napi_value retval = NULL; napi_get_undefined(env, &retval);
    return retval;
}

// Our native version of UIAbility.onBackround().
static napi_value SDL_NAPI_UIAbilityOnBackground(napi_env env, napi_callback_info info)
{
    SDL_OnApplicationWillEnterBackground();
    SDL_OnApplicationDidEnterBackground();
    napi_value retval = NULL; napi_get_undefined(env, &retval);
    return retval;
}

// Our native version of UIAbility.onWindowStageCreate().
static napi_value SDL_NAPI_UIAbilityOnWindowStageCreate(napi_env env, napi_callback_info info)
{
    // grab the window object, load "pages/Index" to continue startup.
    OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "%{public}s", SDL_FUNCTION);
    size_t argc = 1;
    napi_value self = NULL;
    napi_value windowStage = NULL;
    napi_get_cb_info(env, info, &argc, &windowStage, &self, NULL);

    napi_value getMainWindowSync = NULL; napi_get_named_property(env, windowStage, "getMainWindowSync", &getMainWindowSync);
    napi_value window = NULL; napi_call_function(env, windowStage, getMainWindowSync, 0, NULL, &window);
    napi_create_reference(env, window, 1, &window_ref);

    napi_value loadContent = NULL; napi_get_named_property(env, windowStage, "loadContent", &loadContent);
    napi_value pagesIndexStr = NULL; napi_create_string_utf8(env, "pages/Index", NAPI_AUTO_LENGTH, &pagesIndexStr);
    napi_value cb = NULL; napi_create_function(env, NULL, 0,  SDL_NAPI_LoadContentResult, NULL, &cb);
    napi_value args[2] = { pagesIndexStr, cb };
    napi_value rc = NULL; napi_call_function(env, windowStage, loadContent, SDL_arraysize(args), args, &rc);

    napi_value retval = NULL; napi_get_undefined(env, &retval);
    return retval;
}

// Our native version of UIAbility.onWindowStageDestroy().
static napi_value SDL_NAPI_UIAbilityOnWindowStageDestroy(napi_env env, napi_callback_info info)
{
    napi_value retval = NULL; napi_get_undefined(env, &retval);
    return retval;
}

// Our native version of UIAbility.onMemoryLevel().
static napi_value SDL_NAPI_UIAbilityOnMemoryLevel(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value level = NULL;
    napi_get_cb_info(env, info, &argc, &level, NULL, NULL);
    int32_t level32 = -1; napi_get_value_int32(env, level, &level32);

    if (level == 2 /*MEMORY_LEVEL_CRITICAL*/) {
        SDL_OnApplicationDidReceiveMemoryWarning();
    }

    napi_value retval = NULL; napi_get_undefined(env, &retval);
    return retval;
}

static void TakeOverUIAbility(napi_env env, napi_value ability)
{
    napi_value constructor = NULL; napi_get_named_property(env, ability, "constructor", &constructor);
    napi_value prototype = NULL; napi_get_named_property(env, constructor, "prototype", &prototype);
    napi_value fn;
    fn = NULL; napi_create_function(env, "onDestroy", NAPI_AUTO_LENGTH, SDL_NAPI_UIAbilityOnDestroy, NULL, &fn); napi_set_named_property(env, prototype, "onDestroy", fn);
    fn = NULL; napi_create_function(env, "onForeground", NAPI_AUTO_LENGTH, SDL_NAPI_UIAbilityOnForeground, NULL, &fn); napi_set_named_property(env, prototype, "onForeground", fn);
    fn = NULL; napi_create_function(env, "onBackground", NAPI_AUTO_LENGTH, SDL_NAPI_UIAbilityOnBackground, NULL, &fn); napi_set_named_property(env, prototype, "onBackground", fn);
    fn = NULL; napi_create_function(env, "onWindowStageCreate", NAPI_AUTO_LENGTH, SDL_NAPI_UIAbilityOnWindowStageCreate, NULL, &fn); napi_set_named_property(env, prototype, "onWindowStageCreate", fn);
    fn = NULL; napi_create_function(env, "onWindowStageDestroy", NAPI_AUTO_LENGTH, SDL_NAPI_UIAbilityOnWindowStageDestroy, NULL, &fn); napi_set_named_property(env, prototype, "onWindowStageDestroy", fn);
    fn = NULL; napi_create_function(env, "onMemoryLevel", NAPI_AUTO_LENGTH, SDL_NAPI_UIAbilityOnMemoryLevel, NULL, &fn); napi_set_named_property(env, prototype, "onMemoryLevel", fn);
}

// ArkTS calls this once near startup to pass us the Ability, so we can call back into Javascript as necessary.
static napi_value SDL_NAPI_ProvideArkTSObjects(napi_env env, napi_callback_info info)
{
    SDL_assert(!native_resource_mgr);  // don't call this more than once!

    // we don't bother cleaning up most things in this function, because they are intended to live as long as the process.
    #define expected_argc 3
    size_t argc = expected_argc;
    napi_value argv[expected_argc] = { NULL };
    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (argc != expected_argc) {
        // if you hit this, we probably changed either this C code or the ArkTS code in EntryAbility.ets and you need to update one or both to get the back in sync.
        OH_LOG_Print(LOG_APP, LOG_FATAL, LOG_DOMAIN, "SDL/STARTUP", "ProvideArkTSObjects: expected %{public}d objects, but got %{public}d! Script is out of sync? Aborting!", (int) expected_argc, (int) argc);
        exit(1);
    }
    #undef expected_argc

    napi_value ability = argv[0];
    napi_value atmanager = argv[1];
    napi_value locale = argv[2];
    napi_create_reference(env, ability, 1, &ability_object_ref);
    napi_create_reference(env, atmanager, 1, &atmanager_ref);

    TakeOverUIAbility(env, ability);

    napi_value context = NULL; napi_get_named_property(env, ability, "context", &context);
    napi_value resourceManager = NULL; napi_get_named_property(env, context, "resourceManager", &resourceManager);
    native_resource_mgr = OH_ResourceManager_InitNativeResourceManager(env, resourceManager);

    // Store off a copy of the locale string.
    napi_value language = NULL; napi_get_named_property(env, locale, "language", &language);
    napi_value region = NULL; napi_get_named_property(env, locale, "region", &region);
    char *language_sdl = CreateSDLStringFromNAPIValue(env, language);
    char *region_sdl = CreateSDLStringFromNAPIValue(env, region);
    if (language_sdl && region_sdl) {
        if (SDL_asprintf(&system_locale, "%s_%s", language_sdl, region_sdl) < 0) {
            system_locale = NULL;
        }
    }
    SDL_free(language_sdl);
    SDL_free(region_sdl);

    // Set up some threadsafe functions, for calling back into ArkTS from the main thread, regardless of what thread native code is operating from.
    napi_value permname = NULL; napi_create_string_utf8(env, "SDL_RequestOpenHarmonyPermission", NAPI_AUTO_LENGTH, &permname);
    napi_create_threadsafe_function(env, NULL, NULL, permname, 0, 1, NULL, NULL, NULL, CallJSRequestPermissions, &req_permissions_threadsafefn);

    napi_value urlname = NULL; napi_create_string_utf8(env, "SDL_OpenHarmonyOpenURL", NAPI_AUTO_LENGTH, &urlname);
    napi_create_threadsafe_function(env, NULL, NULL, urlname, 0, 1, NULL, NULL, NULL, CallJSOpenURL, &open_url_threadsafefn);

    napi_value sysbarsname = NULL; napi_create_string_utf8(env, "SDL_OpenHarmonyChangeSystemBars", NAPI_AUTO_LENGTH, &sysbarsname);
    napi_create_threadsafe_function(env, NULL, NULL, sysbarsname, 0, 1, NULL, NULL, NULL, CallJSChangeSysBars, &change_sysbars_threadsafefn);

    napi_value screensavername = NULL; napi_create_string_utf8(env, "SDL_OpenHarmonyChangeScreenSaver", NAPI_AUTO_LENGTH, &screensavername);
    napi_create_threadsafe_function(env, NULL, NULL, screensavername, 0, 1, NULL, NULL, NULL, CallJSChangeScreenSaver, &change_screensaver_threadsafefn);

    return NULL;
}


// This is called by SDL_RegisterNativeInterfaces when the library is loaded, which sets up the entry points where
//  ArkTS code can call into our native code.
static napi_value SDL_Init_Native_Interfaces(napi_env env, napi_value exports)
{
    // Functions that we want to be able to call from ArkTS go here.
    // (declare them in C as `napi_value MyFunctionName(napi_env env, napi_callback_info info);`)
    napi_property_descriptor desc[] = {
        { "provideArkTSObjects", NULL, SDL_NAPI_ProvideArkTSObjects, NULL, NULL, NULL, napi_default, NULL },
    };
    napi_define_properties(env, exports, SDL_arraysize(desc), desc);

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

    static OH_NativeXComponent_MouseEvent_Callback xcomp_mouse_callbacks = {
        .DispatchMouseEvent = SDL_XComponent_DispatchMouseEventCallback,
        .DispatchHoverEvent = SDL_XComponent_DispatchHoverEventCallback
    };
    OH_NativeXComponent_RegisterMouseEventCallback(nativeXComponent, &xcomp_mouse_callbacks);

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

