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

#ifdef SDL_VIDEO_DRIVER_DOSVESA

#include "../../events/SDL_events_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../SDL_sysvideo.h"
#include "SDL_dosvideo.h"
#include "SDL_dosevents_c.h"

// Scancode table: https://www.plantation-productions.com/Webster/www.artofasm.com/DOS/pdf/apndxc.pdf
static const SDL_Scancode DOSVESA_ScancodeMapping[] = {  // index is the scancode from the IRQ1 handler bitwise-ANDed against 0x7F.
    /* 0x00 */ SDL_SCANCODE_UNKNOWN,
    /* 0x01 */ SDL_SCANCODE_ESCAPE,
    /* 0x02 */ SDL_SCANCODE_1,
    /* 0x03 */ SDL_SCANCODE_2,
    /* 0x04 */ SDL_SCANCODE_3,
    /* 0x05 */ SDL_SCANCODE_4,
    /* 0x06 */ SDL_SCANCODE_5,
    /* 0x07 */ SDL_SCANCODE_6,
    /* 0x08 */ SDL_SCANCODE_7,
    /* 0x09 */ SDL_SCANCODE_8,
    /* 0x0A */ SDL_SCANCODE_9,
    /* 0x0B */ SDL_SCANCODE_0,
    /* 0x0C */ SDL_SCANCODE_MINUS,
    /* 0x0D */ SDL_SCANCODE_EQUALS,
    /* 0x0E */ SDL_SCANCODE_BACKSPACE,
    /* 0x0F */ SDL_SCANCODE_TAB,

    /* 0x10 */ SDL_SCANCODE_Q,
    /* 0x11 */ SDL_SCANCODE_W,
    /* 0x12 */ SDL_SCANCODE_E,
    /* 0x13 */ SDL_SCANCODE_R,
    /* 0x14 */ SDL_SCANCODE_T,
    /* 0x15 */ SDL_SCANCODE_Y,
    /* 0x16 */ SDL_SCANCODE_U,
    /* 0x17 */ SDL_SCANCODE_I,
    /* 0x18 */ SDL_SCANCODE_O,
    /* 0x19 */ SDL_SCANCODE_P,
    /* 0x1A */ SDL_SCANCODE_LEFTBRACKET,
    /* 0x1B */ SDL_SCANCODE_RIGHTBRACKET,
    /* 0x1C */ SDL_SCANCODE_RETURN,
    /* 0x1D */ SDL_SCANCODE_LCTRL,
    /* 0x1E */ SDL_SCANCODE_A,
    /* 0x1F */ SDL_SCANCODE_S,

    /* 0x20 */ SDL_SCANCODE_D,
    /* 0x21 */ SDL_SCANCODE_F,
    /* 0x22 */ SDL_SCANCODE_G,
    /* 0x23 */ SDL_SCANCODE_H,
    /* 0x24 */ SDL_SCANCODE_J,
    /* 0x25 */ SDL_SCANCODE_K,
    /* 0x26 */ SDL_SCANCODE_L,
    /* 0x27 */ SDL_SCANCODE_SEMICOLON,
    /* 0x28 */ SDL_SCANCODE_APOSTROPHE,
    /* 0x29 */ SDL_SCANCODE_GRAVE,
    /* 0x2A */ SDL_SCANCODE_LSHIFT,
    /* 0x2B */ SDL_SCANCODE_BACKSLASH,
    /* 0x2C */ SDL_SCANCODE_Z,
    /* 0x2D */ SDL_SCANCODE_X,
    /* 0x2E */ SDL_SCANCODE_C,
    /* 0x2F */ SDL_SCANCODE_V,

    /* 0x30 */ SDL_SCANCODE_B,
    /* 0x31 */ SDL_SCANCODE_N,
    /* 0x32 */ SDL_SCANCODE_M,
    /* 0x33 */ SDL_SCANCODE_COMMA,
    /* 0x34 */ SDL_SCANCODE_PERIOD,
    /* 0x35 */ SDL_SCANCODE_SLASH,
    /* 0x36 */ SDL_SCANCODE_RSHIFT,
    /* 0x37 */ SDL_SCANCODE_PRINTSCREEN,
    /* 0x38 */ SDL_SCANCODE_LALT,
    /* 0x39 */ SDL_SCANCODE_SPACE,
    /* 0x3A */ SDL_SCANCODE_CAPSLOCK,
    /* 0x3B */ SDL_SCANCODE_F1,
    /* 0x3C */ SDL_SCANCODE_F2,
    /* 0x3D */ SDL_SCANCODE_F3,
    /* 0x3E */ SDL_SCANCODE_F4,
    /* 0x3F */ SDL_SCANCODE_F5,

    /* 0x040 */ SDL_SCANCODE_F6,
    /* 0x041 */ SDL_SCANCODE_F7,
    /* 0x042 */ SDL_SCANCODE_F8,
    /* 0x043 */ SDL_SCANCODE_F9,
    /* 0x044 */ SDL_SCANCODE_F10,
    /* 0x045 */ SDL_SCANCODE_NUMLOCKCLEAR,
    /* 0x046 */ SDL_SCANCODE_SCROLLLOCK,
    /* 0x047 */ SDL_SCANCODE_KP_7,
    /* 0x048 */ SDL_SCANCODE_KP_8,
    /* 0x049 */ SDL_SCANCODE_KP_9,
    /* 0x04A */ SDL_SCANCODE_KP_MINUS,
    /* 0x04B */ SDL_SCANCODE_KP_4,
    /* 0x04C */ SDL_SCANCODE_KP_5,
    /* 0x04D */ SDL_SCANCODE_KP_6,
    /* 0x04E */ SDL_SCANCODE_KP_PLUS,
    /* 0x04F */ SDL_SCANCODE_KP_1,

    /* 0x050 */ SDL_SCANCODE_KP_2,
    /* 0x051 */ SDL_SCANCODE_KP_3,
    /* 0x052 */ SDL_SCANCODE_KP_0,
    /* 0x053 */ SDL_SCANCODE_KP_PERIOD,
    /* 0x054 */ SDL_SCANCODE_UNKNOWN,
    /* 0x055 */ SDL_SCANCODE_UNKNOWN,
    /* 0x056 */ SDL_SCANCODE_UNKNOWN,
    /* 0x057 */ SDL_SCANCODE_F11,
    /* 0x058 */ SDL_SCANCODE_F12
};


typedef struct DOSVESA_KeyEventBuffer
{
    Uint8 keyevents[256];
    int num_keyevents;
} DOSVESA_KeyEventBuffer;

static DOSVESA_KeyEventBuffer keyevent_buffers[2];
static volatile int current_keyevent_buffer = 0;  // interrupt handler writes to this buffer.

void DOSVESA_PumpEvents(SDL_VideoDevice *device)
{
    DOS_DisableInterrupts();
    const int keyevent_buffer_to_process = current_keyevent_buffer;
    current_keyevent_buffer = (current_keyevent_buffer + 1) & 1;
    DOS_EnableInterrupts();

    DOSVESA_KeyEventBuffer *evbuf = &keyevent_buffers[keyevent_buffer_to_process];
    const Uint8 *events = evbuf->keyevents;
    const int total = SDL_min(evbuf->num_keyevents, SDL_arraysize(evbuf->keyevents));
    evbuf->num_keyevents = 0;

    for (int i = 0; i < total; i++) {
        const Uint8 event = events[i];
        const int scancode = (int) (event & 0x7F);
        const bool pressed = ((event & 0x80) == 0);

        // !!! FIXME: 0xE0 is an extended key signifier. This gives you things like Right Ctrl and the dedicated arrow keys...they
        // !!! FIXME:  just happen to map to the non-extended versions (left ctrl, keypad arrows) if you ignore the extended byte.
        // !!! FIXME: But we should handle this appropriately.
        if (scancode < SDL_arraysize(DOSVESA_ScancodeMapping)) {
            SDL_SendKeyboardKey(0, SDL_GLOBAL_KEYBOARD_ID, 0, DOSVESA_ScancodeMapping[scancode], pressed);
        }
    }

    SDL_Mouse *mouse = SDL_GetMouse();
    if (mouse->internal) {  // if non-NULL, there's a mouse detected on the system.
        __dpmi_regs regs;

        regs.x.ax = 0x3;   // read mouse buttons and position.
        __dpmi_int(0x33, &regs);
        const Uint16 buttons = (int) (Sint16) regs.x.bx;

        SDL_SendMouseButton(0, mouse->focus, SDL_DEFAULT_MOUSE_ID, SDL_BUTTON_LEFT, (buttons & (1 << 0)) != 0);
        SDL_SendMouseButton(0, mouse->focus, SDL_DEFAULT_MOUSE_ID, SDL_BUTTON_RIGHT, (buttons & (1 << 1)) != 0);
        SDL_SendMouseButton(0, mouse->focus, SDL_DEFAULT_MOUSE_ID, SDL_BUTTON_MIDDLE, (buttons & (1 << 2)) != 0);

        if (!mouse->relative_mode) {
            const int x = (int) (Sint16) regs.x.cx;  // part of function 0x3's return value.
            const int y = (int) (Sint16) regs.x.dx;
            SDL_SendMouseMotion(0, mouse->focus, SDL_DEFAULT_MOUSE_ID, false, x, y);
        } else {
            regs.x.ax = 0xB;   // read motion counters
            __dpmi_int(0x33, &regs);
            // values returned here are -32768 to 32767
            const float MICKEYS_PER_HPIXEL = 4.0f;  // !!! FIXME: what should this be?
            const float MICKEYS_PER_VPIXEL = 4.0f;
            const int mickeys_x = (int) (Sint16) regs.x.cx;
            const int mickeys_y = (int) (Sint16) regs.x.dx;
            SDL_SendMouseMotion(0, mouse->focus, SDL_DEFAULT_MOUSE_ID, true, mickeys_x / MICKEYS_PER_HPIXEL, mickeys_y / MICKEYS_PER_VPIXEL);
        }
    }
}

static void KeyboardIRQHandler(void)  // this is wrapped in a thing that handles IRET, etc.
{
    const Uint8 key = inportb(0x60);
    DOSVESA_KeyEventBuffer *evbuf = &keyevent_buffers[current_keyevent_buffer];
    if (evbuf->num_keyevents < SDL_arraysize(evbuf->keyevents)) {
	    evbuf->keyevents[evbuf->num_keyevents++] = key;
    }
    DOS_EndOfInterrupt();
}

void DOSVESA_InitKeyboard(SDL_VideoDevice *device)
{
    SDL_VideoData *data = device->internal;
    DOS_HookInterrupt(1, KeyboardIRQHandler, &data->keyboard_interrupt_hook);
}

void DOSVESA_QuitKeyboard(SDL_VideoDevice *device)
{
    SDL_VideoData *data = device->internal;
    DOS_UnhookInterrupt(&data->keyboard_interrupt_hook, false);
}

#endif // SDL_VIDEO_DRIVER_DOSVESA
