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

#ifdef SDL_VIDEO_DRIVER_OPENHARMONY

// !!! FIXME: these are defined as "const uint32_t VARNAME = VALUE;" in native_interface_xcomponent.h, which becomes a global variable in _our_ C code! Maybe C++ handles this differently...?
#define OH_XCOMPONENT_ID_LEN_MAX sdl_ohosevents_OH_XCOMPONENT_ID_LEN_MAX
#define OH_MAX_TOUCH_POINTS_NUMBER sdl_ohosevents_OH_MAX_TOUCH_POINTS_NUMBER
#include <ace/xcomponent/native_interface_xcomponent.h>

#include "SDL_openharmonyevents.h"
//#include "SDL_openharmonykeyboard.h"
#include "SDL_openharmonywindow.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_events_c.h"


#if 0
typedef struct {
    /** Unique identifier of a finger. */
    int32_t id;
    /** X coordinate of the touch point relative to the left edge of the screen. */
    float screenX;
    /** Y coordinate of the touch point relative to the upper edge of the screen. */
    float screenY;
    /** X coordinate of the touch point relative to the left edge of the element to touch. */
    float x;
    /** Y coordinate of the touch point relative to the upper edge of the element to touch. */
    float y;
    /** Touch type of the touch event. */
    OH_NativeXComponent_TouchEventType type;
    /** Contact area between the finger pad and the screen. */
    double size;
    /** Pressure of the current touch event. */
    float force;
    /** ID of the device where the current touch event is generated. */
    int64_t deviceId;
    /** Timestamp of the current touch event. */
    int64_t timeStamp;
    /** Array of the current touch points. */
    OH_NativeXComponent_TouchPoint touchPoints[OH_NATIVE_XCOMPONENT_MAX_TOUCH_POINTS_NUMBER];
    /** Number of current touch points. */
    uint32_t numPoints;
} OH_NativeXComponent_TouchEvent;
#endif

void SDL_OpenHarmonyDispatchTouchEvent(void *component, void *window)
{
    OH_NativeXComponent *xcomponent = (OH_NativeXComponent *) component;
    OH_NativeXComponent_TouchEvent event;
    if (OH_NativeXComponent_GetTouchEvent(xcomponent, window, &event) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;  // oh well.
    }

#if 0
    SDL_Log("TOUCH EVENT! id=%d screenX=%f screenY=%f x=%f y=%f type=%d size=%f force=%f devid=%lld timestamp=%lld numpoints=%d", (int) event.id, event.screenX, event.screenY, event.x, event.y, (int) event.type, (float) event.size, event.force, (long long) event.deviceId, (long long) event.timeStamp, (int) event.numPoints);
    for (int i = 0; i < event.numPoints; i++) {
        SDL_Log("TOUCH POINT #%d: id=%d screenX=%f screenY=%f x=%f y=%f type=%d size=%f force=%f timestamp=%lld ispressed=%s", i, (int) event.touchPoints[i].id, event.touchPoints[i].screenX, event.touchPoints[i].screenY, event.touchPoints[i].x, event.touchPoints[i].y, (int) event.touchPoints[i].type, (float) event.touchPoints[i].size, event.touchPoints[i].force, (long long) event.touchPoints[i].timeStamp, event.touchPoints[i].isPressed ? "true" : "false");
    }
#endif

    SDL_EventType sdleventtype = SDL_EVENT_FIRST;
    switch (event.type) {
        #define EVTYPEMAP(ohevent, sdlevent) case OH_NATIVEXCOMPONENT_##ohevent: sdleventtype = SDL_EVENT_FINGER_##sdlevent; break
        EVTYPEMAP(DOWN, DOWN);
        EVTYPEMAP(UP, UP);
        EVTYPEMAP(MOVE, MOTION);
        EVTYPEMAP(CANCEL, CANCELED);
        default: break;
        #undef EVTYPEMAP
    }

    if (sdleventtype == SDL_EVENT_FIRST) {  // FIRST == unknown event type.
        return;  // nothing to do.
    }

    int touchidx = 0;
    while (touchidx < event.numPoints) {
        if (event.touchPoints[touchidx].id == event.id) {
            break;
        }
        touchidx++;
    }

    if (touchidx >= event.numPoints) {
        return;  // uh...we don't have this touch...?
    }

    OH_NativeXComponent_TouchPointToolType tooltype;
    if (OH_NativeXComponent_GetTouchPointToolType(xcomponent, touchidx, &tooltype) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;   // bad bad bad
    }

    const float normalized_x = OPENHARMONY_Window->w ? (event.x / ((float) OPENHARMONY_Window->w)) : 0.0f;
    const float normalized_y = OPENHARMONY_Window->h ? (event.y / ((float) OPENHARMONY_Window->h)) : 0.0f;

    if (tooltype == OH_NATIVEXCOMPONENT_TOOL_TYPE_FINGER) {
        const SDL_TouchID touchid = (SDL_TouchID)(((uintptr_t)event.deviceId) + 1);
        SDL_AddTouch(touchid, SDL_TOUCH_DEVICE_DIRECT, NULL);
        if (sdleventtype == SDL_EVENT_FINGER_MOTION) {
            SDL_SendTouchMotion((Uint64) event.timeStamp, touchid, (SDL_FingerID)(((uintptr_t)event.id)+1), OPENHARMONY_Window, normalized_x, normalized_y, event.force);
        } else {
            SDL_SendTouch((Uint64) event.timeStamp, touchid, (SDL_FingerID)(((uintptr_t)event.id)+1), OPENHARMONY_Window, sdleventtype, normalized_x, normalized_y, event.force);
        }
        return;
    }

#if 0
wefrewr
    switch (tooltype) {

    /** Indicates invalid tool type. */
    OH_NATIVEXCOMPONENT_TOOL_TYPE_UNKNOWN = 0,
    /** Indicates a finger. */
    OH_NATIVEXCOMPONENT_TOOL_TYPE_FINGER,
    /** Indicates a stylus. */
    OH_NATIVEXCOMPONENT_TOOL_TYPE_PEN,
    /** Indicates an eraser. */
    OH_NATIVEXCOMPONENT_TOOL_TYPE_RUBBER,
    /** Indicates a brush. */
    OH_NATIVEXCOMPONENT_TOOL_TYPE_BRUSH,
    /** Indicates a pencil. */
    OH_NATIVEXCOMPONENT_TOOL_TYPE_PENCIL,
    /** Indicates a brush. */
    OH_NATIVEXCOMPONENT_TOOL_TYPE_AIRBRUSH,
    /** Indicates a mouse. */
    OH_NATIVEXCOMPONENT_TOOL_TYPE_MOUSE,
    /** Indicates a lens. */
    OH_NATIVEXCOMPONENT_TOOL_TYPE_LENS,

    OH_NativeXComponent* component, uint32_t pointIndex, OH_NativeXComponent_TouchPointToolType* toolType)
wefwef
#endif

}


void OPENHARMONY_InitEvents(void)
{
}

void OPENHARMONY_PumpEvents(SDL_VideoDevice *_this)
{
}

void OPENHARMONY_QuitEvents(void)
{
}

#endif // SDL_VIDEO_DRIVER_OPENHARMONY
