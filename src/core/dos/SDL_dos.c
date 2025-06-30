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

#if defined(SDL_PLATFORM_DOS)

#include "SDL_dos.h"

void *DOS_AllocateConventionalMemory(const int len, _go32_dpmi_seginfo *seginfo)
{
    seginfo->size = (len + 15) / 16;  // this is in "paragraphs"
	if (_go32_dpmi_allocate_dos_memory(seginfo) != 0) {
        SDL_OutOfMemory();
        return NULL;
    }
    // !!! FIXME: most (all?) DPMI hosts lock all conventional memory, right? If not, we should lock it here.
    return DOS_PhysicalToLinear(seginfo->rm_segment * 16);
}

void *DOS_AllocateDMAMemory(const int len, _go32_dpmi_seginfo *seginfo)
{
    // !!! FIXME: I stole this RAM doubling idea from Allegro, but see if we
    // !!! FIXME:  can get away with not doubling the RAM first, and fall back
    // !!! FIXME:  to this if not? Maybe there's a better way to do this in general?

    // double our requested size, so we can deal with crossing a page boundary.
    uint8_t *ptr = (uint8_t *) DOS_AllocateConventionalMemory(len * 2, seginfo);
    if (!ptr) {
        return NULL;
    }

    // if we're past the end of a page, use the second half of the block.
    const uint32_t physical = (seginfo->rm_segment * 16);
    if ((physical >> 16) != ((physical+len) >> 16)) {
        ptr += len;
    }
    return ptr;
}

void DOS_FreeConventionalMemory(_go32_dpmi_seginfo *seginfo)
{
    _go32_dpmi_free_dos_memory(seginfo);
}

char *DOS_GetFarPtrCString(const Uint32 segoffset)
{
    if (!segoffset) {  // let's just treat this as a NULL pointer.
        return NULL;
    }

    const unsigned long ofs = (unsigned long) (((segoffset & 0xFFFF0000) >> 12) + (segoffset & 0xFFFF));
    size_t len;

    for (len = 0; _farpeekb(_dos_ds, ofs+len) != '\0'; len++) {}

    len++;  // null terminator.
    char *retval = SDL_malloc(len);
    if (!retval) {
        return NULL;
    }

    for (size_t i = 0; i < len; i++) {
        retval[i] = (char) _farpeekb(_dos_ds, ofs+i);
    }
    return retval;
}

void DOS_HookInterrupt(int irq, DOS_InterruptHookFn fn, DOS_InterruptHook *hook)
{
    SDL_assert(irq > 0);
    SDL_assert(fn != NULL);
    SDL_assert(hook != NULL);
    hook->fn = fn;
    hook->irq = irq;
    hook->interrupt_vector = DOS_IRQToVector(irq);
    hook->irq_handler_seginfo.pm_selector = _go32_my_cs();
    hook->irq_handler_seginfo.pm_offset = (uint32_t) fn;
    _go32_dpmi_get_protected_mode_interrupt_vector(hook->interrupt_vector, &hook->original_irq_handler_seginfo);
    _go32_dpmi_chain_protected_mode_interrupt_vector(hook->interrupt_vector, &hook->irq_handler_seginfo);
    outportb(0x21, inportb(0x21) & (~(1<<irq)));  // enable interrupt
}

void DOS_UnhookInterrupt(DOS_InterruptHook *hook, bool disable_interrupt)
{
    if (!hook || !hook->fn) {
        return;
    }

    SDL_assert(hook->interrupt_vector > 0);
    SDL_assert(hook->irq > 0);

    if (disable_interrupt) {
        outportb( 0x21, inportb(0x21) | (1 << hook->irq) );
    }

    // !!! FIXME: do we not have to free something from _go32_dpmi_chain_protected_mode_interrupt_vector?
    _go32_dpmi_set_protected_mode_interrupt_vector(hook->interrupt_vector, &hook->original_irq_handler_seginfo);

    SDL_zerop(hook);
}


#endif // defined(SDL_PLATFORM_DOS)

