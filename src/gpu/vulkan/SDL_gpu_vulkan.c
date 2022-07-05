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

#ifdef SDL_GPU_VULKAN

/* The gpu subsystem dummy driver */

#include "SDL.h"
#include "../SDL_sysgpu.h"

typedef struct VULKAN_GpuDeviceData
{

} VULKAN_GpuDeviceData;

static void VULKAN_GpuDestroyDevice(SDL_GpuDevice *device) { /* no-op */ }

static int VULKAN_GpuClaimWindow(SDL_GpuDevice *device, SDL_Window *window) { return 0; }

static int VULKAN_GpuCreateCpuBuffer(SDL_GpuCpuBuffer *buffer, const void *data)
{
    /* have to save off buffer data so we can provide it for locking, etc. */
    buffer->driverdata = SDL_calloc(1, buffer->buflen);
    if (!buffer->driverdata) {
        return SDL_OutOfMemory();
    }
    if (data) {
        SDL_memcpy(buffer->driverdata, data, buffer->buflen);
    }
    return 0;
}

static void VULKAN_GpuDestroyCpuBuffer(SDL_GpuCpuBuffer *buffer)
{
    SDL_free(buffer->driverdata);
}

static void *VULKAN_GpuLockCpuBuffer(SDL_GpuCpuBuffer *buffer)
{
    return buffer->driverdata;
}

/* we could get fancier and manage imaginary GPU buffers and textures, but I don't think it's worth it atm. */

static int VULKAN_GpuUnlockCpuBuffer(SDL_GpuCpuBuffer *buffer) { return 0; }
static int VULKAN_GpuCreateBuffer(SDL_GpuBuffer *buffer) { return 0; }
static void VULKAN_GpuDestroyBuffer(SDL_GpuBuffer *buffer) {}
static int VULKAN_GpuCreateTexture(SDL_GpuTexture *texture) { return 0; }
static void VULKAN_GpuDestroyTexture(SDL_GpuTexture *texture) {}
static int VULKAN_GpuCreateShader(SDL_GpuShader *shader, const Uint8 *bytecode, const Uint32 bytecodelen) { return 0; }
static void VULKAN_GpuDestroyShader(SDL_GpuShader *shader) {}
static int VULKAN_GpuCreatePipeline(SDL_GpuPipeline *pipeline) { return 0; }
static void VULKAN_GpuDestroyPipeline(SDL_GpuPipeline *pipeline) {}
static int VULKAN_GpuCreateSampler(SDL_GpuSampler *sampler) { return 0; }
static void VULKAN_GpuDestroySampler(SDL_GpuSampler *sampler) {}
static int VULKAN_GpuCreateCommandBuffer(SDL_GpuCommandBuffer *cmdbuf) { return 0; }
static int VULKAN_GpuStartRenderPass(SDL_GpuRenderPass *pass, Uint32 num_color_attachments, const SDL_GpuColorAttachmentDescription *color_attachments, const SDL_GpuDepthAttachmentDescription *depth_attachment, const SDL_GpuStencilAttachmentDescription *stencil_attachment) { return 0; }
static int VULKAN_GpuSetRenderPassPipeline(SDL_GpuRenderPass *pass, SDL_GpuPipeline *pipeline) { return 0; }
static int VULKAN_GpuSetRenderPassViewport(SDL_GpuRenderPass *pass, double x, double y, double width, double height, double znear, double zfar) { return 0; }
static int VULKAN_GpuSetRenderPassScissor(SDL_GpuRenderPass *pass, Uint32 x, Uint32 y, Uint32 width, Uint32 height) { return 0; }
static int VULKAN_GpuSetRenderPassBlendConstant(SDL_GpuRenderPass *pass, double red, double green, double blue, double alpha) { return 0; }
static int VULKAN_GpuSetRenderPassVertexBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 index) { return 0; }
static int VULKAN_GpuSetRenderPassVertexSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, Uint32 index) { return 0; }
static int VULKAN_GpuSetRenderPassVertexTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, Uint32 index) { return 0; }
static int VULKAN_GpuSetRenderPassFragmentBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 index) { return 0; }
static int VULKAN_GpuSetRenderPassFragmentSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, Uint32 index) { return 0; }
static int VULKAN_GpuSetRenderPassFragmentTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, Uint32 index) { return 0; }
static int VULKAN_GpuDraw(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count) { return 0; }
static int VULKAN_GpuDrawIndexed(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset) { return 0; }
static int VULKAN_GpuDrawInstanced(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count, Uint32 instance_count, Uint32 base_instance) { return 0; }
static int VULKAN_GpuDrawInstancedIndexed(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset, Uint32 instance_count, Uint32 base_vertex, Uint32 base_instance) { return 0; }
static int VULKAN_GpuEndRenderPass(SDL_GpuRenderPass *pass) { return 0; }
static int VULKAN_GpuStartBlitPass(SDL_GpuBlitPass *pass) { return 0; }
static int VULKAN_GpuCopyBetweenTextures(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel, Uint32 srcx, Uint32 srcy, Uint32 srcz, Uint32 srcw, Uint32 srch, Uint32 srcdepth, SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel, Uint32 dstx, Uint32 dsty, Uint32 dstz) { return 0; }
static int VULKAN_GpuFillBuffer(SDL_GpuBlitPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 length, Uint8 value) { return 0; }
static int VULKAN_GpuGenerateMipmaps(SDL_GpuBlitPass *pass, SDL_GpuTexture *texture) { return 0; }
static int VULKAN_GpuCopyBufferCpuToGpu(SDL_GpuBlitPass *pass, SDL_GpuCpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length) { return 0; }
static int VULKAN_GpuCopyBufferGpuToCpu(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuCpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length) { return 0; }
static int VULKAN_GpuCopyBufferGpuToGpu(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length) { return 0; }
static int VULKAN_GpuCopyFromBufferToTexture(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, Uint32 srcpitch, Uint32 srcimgpitch, Uint32 srcw, Uint32 srch, Uint32 srcdepth, SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel, Uint32 dstx, Uint32 dsty, Uint32 dstz) { return 0; }
static int VULKAN_GpuCopyFromTextureToBuffer(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel, Uint32 srcx, Uint32 srcy, Uint32 srcz, Uint32 srcw, Uint32 srch, Uint32 srcdepth, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 dstpitch, Uint32 dstimgpitch) { return 0; }
static int VULKAN_GpuEndBlitPass(SDL_GpuBlitPass *pass) { return 0; }
static int VULKAN_GpuSubmitCommandBuffer(SDL_GpuCommandBuffer *cmdbuf, SDL_GpuFence *fence) { return 0; }
static void VULKAN_GpuAbandonCommandBuffer(SDL_GpuCommandBuffer *buffer) {}
static int VULKAN_GpuGetBackbuffer(SDL_GpuDevice *device, SDL_Window *window, SDL_GpuTexture *texture) { return 0; }
static int VULKAN_GpuPresent(SDL_GpuDevice *device, SDL_Window *window, SDL_GpuTexture *backbuffer, int swapinterval) { return 0; }
static int VULKAN_GpuCreateFence(SDL_GpuFence *fence) { return 0; }
static void VULKAN_GpuDestroyFence(SDL_GpuFence *fence) {}
static int VULKAN_GpuQueryFence(SDL_GpuFence *fence) { return 1; }
static int VULKAN_GpuResetFence(SDL_GpuFence *fence) { return 0; }
static int VULKAN_GpuWaitFence(SDL_GpuFence *fence) { return 0; }

static int
VULKAN_GpuCreateDevice(SDL_GpuDevice *device)
{

	VULKAN_GpuDeviceData *devdata;

	devdata = SDL_malloc(sizeof(VULKAN_GpuDeviceData));
	if (!devdata) {
		return SDL_OutOfMemory();
	}

	/* Safety zero */
	SDL_memset(devdata, '\0', sizeof(VULKAN_GpuDeviceData));

	device->driverdata = (void *) devdata;
    device->DestroyDevice = VULKAN_GpuDestroyDevice;
    device->ClaimWindow = VULKAN_GpuClaimWindow;
    device->CreateCpuBuffer = VULKAN_GpuCreateCpuBuffer;
    device->DestroyCpuBuffer = VULKAN_GpuDestroyCpuBuffer;
    device->LockCpuBuffer = VULKAN_GpuLockCpuBuffer;
    device->UnlockCpuBuffer = VULKAN_GpuUnlockCpuBuffer;
    device->CreateBuffer = VULKAN_GpuCreateBuffer;
    device->DestroyBuffer = VULKAN_GpuDestroyBuffer;
    device->CreateTexture = VULKAN_GpuCreateTexture;
    device->DestroyTexture = VULKAN_GpuDestroyTexture;
    device->CreateShader = VULKAN_GpuCreateShader;
    device->DestroyShader = VULKAN_GpuDestroyShader;
    device->CreatePipeline = VULKAN_GpuCreatePipeline;
    device->DestroyPipeline = VULKAN_GpuDestroyPipeline;
    device->CreateSampler = VULKAN_GpuCreateSampler;
    device->DestroySampler = VULKAN_GpuDestroySampler;
    device->CreateCommandBuffer = VULKAN_GpuCreateCommandBuffer;
    device->StartRenderPass = VULKAN_GpuStartRenderPass;
    device->SetRenderPassPipeline = VULKAN_GpuSetRenderPassPipeline;
    device->SetRenderPassViewport = VULKAN_GpuSetRenderPassViewport;
    device->SetRenderPassScissor = VULKAN_GpuSetRenderPassScissor;
    device->SetRenderPassBlendConstant = VULKAN_GpuSetRenderPassBlendConstant;
    device->SetRenderPassVertexBuffer = VULKAN_GpuSetRenderPassVertexBuffer;
    device->SetRenderPassVertexSampler = VULKAN_GpuSetRenderPassVertexSampler;
    device->SetRenderPassVertexTexture = VULKAN_GpuSetRenderPassVertexTexture;
    device->SetRenderPassFragmentBuffer = VULKAN_GpuSetRenderPassFragmentBuffer;
    device->SetRenderPassFragmentSampler = VULKAN_GpuSetRenderPassFragmentSampler;
    device->SetRenderPassFragmentTexture = VULKAN_GpuSetRenderPassFragmentTexture;
    device->Draw = VULKAN_GpuDraw;
    device->DrawIndexed = VULKAN_GpuDrawIndexed;
    device->DrawInstanced = VULKAN_GpuDrawInstanced;
    device->DrawInstancedIndexed = VULKAN_GpuDrawInstancedIndexed;
    device->EndRenderPass = VULKAN_GpuEndRenderPass;
    device->StartBlitPass = VULKAN_GpuStartBlitPass;
    device->CopyBetweenTextures = VULKAN_GpuCopyBetweenTextures;
    device->FillBuffer = VULKAN_GpuFillBuffer;
    device->GenerateMipmaps = VULKAN_GpuGenerateMipmaps;
    device->CopyBufferCpuToGpu = VULKAN_GpuCopyBufferCpuToGpu;
    device->CopyBufferGpuToCpu = VULKAN_GpuCopyBufferGpuToCpu;
    device->CopyBufferGpuToGpu = VULKAN_GpuCopyBufferGpuToGpu;
    device->CopyFromBufferToTexture = VULKAN_GpuCopyFromBufferToTexture;
    device->CopyFromTextureToBuffer = VULKAN_GpuCopyFromTextureToBuffer;
    device->EndBlitPass = VULKAN_GpuEndBlitPass;
    device->SubmitCommandBuffer = VULKAN_GpuSubmitCommandBuffer;
    device->AbandonCommandBuffer = VULKAN_GpuAbandonCommandBuffer;
    device->GetBackbuffer = VULKAN_GpuGetBackbuffer;
    device->Present = VULKAN_GpuPresent;
    device->CreateFence = VULKAN_GpuCreateFence;
    device->DestroyFence = VULKAN_GpuDestroyFence;
    device->QueryFence = VULKAN_GpuQueryFence;
    device->ResetFence = VULKAN_GpuResetFence;
    device->WaitFence = VULKAN_GpuWaitFence;

    return 0;  /* okay, always succeeds. */
}

const SDL_GpuDriver VULKAN_GpuDriver = {
    "Vulkan", VULKAN_GpuCreateDevice
};

#endif /* SDL_GPU_VULKAN */

/* vi: set ts=4 sw=4 expandtab: */
