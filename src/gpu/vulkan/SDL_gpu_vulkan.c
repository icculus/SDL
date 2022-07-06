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

/* The Vulkan 1.0 driver for the GPU subsystem. */

#include "SDL.h"
#include "../SDL_sysgpu.h"
#include "SDL_version.h"
#include "SDL_vulkan.h"
#include "SDL_error.h"

/* Needed for VK_KHR_portability_subset */
#define VK_ENABLE_BETA_EXTENSIONS

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

/* Constants/Limits */

static const uint8_t DEVICE_PRIORITY[] =
{
    0,	/* VK_PHYSICAL_DEVICE_TYPE_OTHER */
    3,	/* VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU */
    4,	/* VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU */
    2,	/* VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU */
    1	/* VK_PHYSICAL_DEVICE_TYPE_CPU */
};

/* Vulkan Extensions */

typedef struct VulkanExtensions
{
    /* These extensions are required! */

    /* Globally supported */
    uint8_t KHR_swapchain;
    /* Core since 1.1 */
    uint8_t KHR_maintenance1;
    uint8_t KHR_dedicated_allocation;
    uint8_t KHR_get_memory_requirements2;

    /* These extensions are optional! */

    /* Core since 1.2, but requires annoying paperwork to implement */
    uint8_t KHR_driver_properties;
    /* Only required for special implementations (i.e. MoltenVK) */
    uint8_t KHR_portability_subset;
    /* Vendor-specific extensions */
    uint8_t GGP_frame_token;
} VulkanExtensions;

static inline uint8_t CheckDeviceExtensions(
    VkExtensionProperties *extensions,
    uint32_t numExtensions,
    VulkanExtensions *supports
) {
    uint32_t i;

    SDL_memset(supports, '\0', sizeof(VulkanExtensions));
    for (i = 0; i < numExtensions; i += 1)
    {
        const char *name = extensions[i].extensionName;
        #define CHECK(ext) \
            if (SDL_strcmp(name, "VK_" #ext) == 0) \
            { \
                supports->ext = 1; \
            }
        CHECK(KHR_swapchain)
        else CHECK(KHR_maintenance1)
        else CHECK(KHR_dedicated_allocation)
        else CHECK(KHR_get_memory_requirements2)
        else CHECK(KHR_driver_properties)
        else CHECK(KHR_portability_subset)
        else CHECK(GGP_frame_token)
        #undef CHECK
    }

    return (	supports->KHR_swapchain &&
            supports->KHR_maintenance1 &&
            supports->KHR_dedicated_allocation &&
            supports->KHR_get_memory_requirements2	);
}

static inline uint32_t GetDeviceExtensionCount(VulkanExtensions *supports)
{
    return (
        supports->KHR_swapchain +
        supports->KHR_maintenance1 +
        supports->KHR_dedicated_allocation +
        supports->KHR_get_memory_requirements2 +
        supports->KHR_driver_properties +
        supports->KHR_portability_subset +
        supports->GGP_frame_token
    );
}

static inline void CreateDeviceExtensionArray(
    VulkanExtensions *supports,
    const char **extensions
) {
    uint8_t cur = 0;
    #define CHECK(ext) \
        if (supports->ext) \
        { \
            extensions[cur++] = "VK_" #ext; \
        }
    CHECK(KHR_swapchain)
    CHECK(KHR_maintenance1)
    CHECK(KHR_dedicated_allocation)
    CHECK(KHR_get_memory_requirements2)
    CHECK(KHR_driver_properties)
    CHECK(KHR_portability_subset)
    CHECK(GGP_frame_token)
    #undef CHECK
}

/* Logging */

typedef void (*VULKAN_LogFunc)(const char *msg);

/* FIXME: Request that this become an actual log category */
#define SDL_LOG_CATEGORY_GPU SDL_LOG_CATEGORY_APPLICATION

static void VULKAN_Default_LogInfo(const char *msg)
{
    SDL_LogInfo(
        SDL_LOG_CATEGORY_GPU,
        "%s",
        msg
    );
}

static void VULKAN_Default_LogWarn(const char *msg)
{
    SDL_LogWarn(
        SDL_LOG_CATEGORY_GPU,
        "%s",
        msg
    );
}

static void VULKAN_Default_LogError(const char *msg)
{
    SDL_LogError(
        SDL_LOG_CATEGORY_GPU,
        "%s",
        msg
    );
}

static VULKAN_LogFunc VULKAN_LogInfoFunc = VULKAN_Default_LogInfo;
static VULKAN_LogFunc VULKAN_LogWarnFunc = VULKAN_Default_LogWarn;
static VULKAN_LogFunc VULKAN_LogErrorFunc = VULKAN_Default_LogError;

#define MAX_MESSAGE_SIZE 1024

void VULKAN_LogInfo(const char *fmt, ...)
{
    char msg[MAX_MESSAGE_SIZE];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    VULKAN_LogInfoFunc(msg);
}

void VULKAN_LogWarn(const char *fmt, ...)
{
    char msg[MAX_MESSAGE_SIZE];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    VULKAN_LogWarnFunc(msg);
}

void VULKAN_LogError(const char *fmt, ...)
{
    char msg[MAX_MESSAGE_SIZE];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    VULKAN_LogErrorFunc(msg);
}

#undef MAX_MESSAGE_SIZE

/* Error Handling */

static inline const char* VkErrorMessages(VkResult code)
{
    #define ERR_TO_STR(e) \
        case e: return #e;
    switch (code)
    {
        ERR_TO_STR(VK_ERROR_OUT_OF_HOST_MEMORY)
        ERR_TO_STR(VK_ERROR_OUT_OF_DEVICE_MEMORY)
        ERR_TO_STR(VK_ERROR_FRAGMENTED_POOL)
        ERR_TO_STR(VK_ERROR_OUT_OF_POOL_MEMORY)
        ERR_TO_STR(VK_ERROR_INITIALIZATION_FAILED)
        ERR_TO_STR(VK_ERROR_LAYER_NOT_PRESENT)
        ERR_TO_STR(VK_ERROR_EXTENSION_NOT_PRESENT)
        ERR_TO_STR(VK_ERROR_FEATURE_NOT_PRESENT)
        ERR_TO_STR(VK_ERROR_TOO_MANY_OBJECTS)
        ERR_TO_STR(VK_ERROR_DEVICE_LOST)
        ERR_TO_STR(VK_ERROR_INCOMPATIBLE_DRIVER)
        ERR_TO_STR(VK_ERROR_OUT_OF_DATE_KHR)
        ERR_TO_STR(VK_ERROR_SURFACE_LOST_KHR)
        ERR_TO_STR(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
        ERR_TO_STR(VK_SUBOPTIMAL_KHR)
        default: return "Unhandled VkResult!";
    }
    #undef ERR_TO_STR
}

#define VULKAN_SET_ERROR(res, fn) \
    if (res != VK_SUCCESS) \
    { \
        return SDL_SetError("%s %s", #fn, VkErrorMessages(res)); \
    }

#define VULKAN_ERROR_CHECK(res, fn, ret) \
    if (res != VK_SUCCESS) \
    { \
        VULKAN_LogError("%s %s", #fn, VkErrorMessages(res)); \
        return ret; \
    }

/* Vulkan global function pointers */
static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = NULL;
static PFN_vkCreateInstance vkCreateInstance = NULL;
static PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties = NULL;
static PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties = NULL;

/* Internal structures */

typedef struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    uint32_t formatsLength;
    VkPresentModeKHR *presentModes;
    uint32_t presentModesLength;
} SwapChainSupportDetails;

typedef struct VULKAN_GpuDeviceData
{
    VkInstance instance;

    VkPhysicalDevice physicalDevice;
    uint32_t queueFamilyIndex;
    VkPhysicalDeviceProperties2 physicalDeviceProperties;
    VkPhysicalDeviceDriverPropertiesKHR physicalDeviceDriverProperties;
    VkPhysicalDeviceMemoryProperties memoryProperties;

    VkDevice logicalDevice;
    VkQueue unifiedQueue;

    SDL_HashTable *commandPoolHashTable;

    /* Capabilities */
    uint8_t debugMode;
    uint8_t supportsDebugUtils;
    VulkanExtensions supportedExtensions;

    /* Sync primitives */
    SDL_mutex *acquireCommandBufferLock;

    #define VULKAN_INSTANCE_FUNCTION(name) \
        PFN_##name name;
    #define VULKAN_DEVICE_FUNCTION(name) \
        PFN_##name name;
    #include "SDL_gpu_vulkan_vkfuncs.h"
} VULKAN_GpuDeviceData;

/* Command buffer management */

typedef struct VULKAN_CommandBufferData VULKAN_CommandBufferData;

typedef struct VULKAN_CommandPool
{
    SDL_threadID threadID;
    VkCommandPool commandPool;

    VULKAN_CommandBufferData **inactiveCommandBuffers;
    uint32_t inactiveCommandBufferCapacity;
    uint32_t inactiveCommandBufferCount;
} VULKAN_CommandPool;

struct VULKAN_CommandBufferData
{
    VkCommandBuffer commandBuffer;
    VULKAN_CommandPool *commandPool;
    uint8_t submitted;
};

static uint32_t HashCommandPool(const void *key, void *data)
{
    /* FIXME: this is weird */
    return (uint32_t) key;
}

static SDL_bool KeyMatchCommandPool(const void *a, const void *b, void *data)
{
    const VULKAN_CommandPool *poolA = (const VULKAN_CommandPool *) a;
    const VULKAN_CommandPool *poolB = (const VULKAN_CommandPool *) b;

    return poolA->threadID == poolB->threadID;
}

/* We'll never need to remove command pools so just return */
static void NukeCommandPool(const void *key, const void *value, void *data) { /* no-op */ }

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

/* Command buffer management */

static void VULKAN_INTERNAL_AllocateCommandBuffers(
	VULKAN_GpuDeviceData *deviceData,
	VULKAN_CommandPool *vulkanCommandPool,
	uint32_t allocateCount
) {
	VkCommandBufferAllocateInfo allocateInfo;
	VkResult vulkanResult;
	uint32_t i;
	VkCommandBuffer *commandBuffers = SDL_stack_alloc(VkCommandBuffer, allocateCount);
	VULKAN_CommandBufferData *commandBufferData;

	vulkanCommandPool->inactiveCommandBufferCapacity += allocateCount;

	vulkanCommandPool->inactiveCommandBuffers = SDL_realloc(
		vulkanCommandPool->inactiveCommandBuffers,
		sizeof(VULKAN_CommandBufferData*) *
		vulkanCommandPool->inactiveCommandBufferCapacity
	);

	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.pNext = NULL;
	allocateInfo.commandPool = vulkanCommandPool->commandPool;
	allocateInfo.commandBufferCount = allocateCount;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	vulkanResult = deviceData->vkAllocateCommandBuffers(
		deviceData->logicalDevice,
		&allocateInfo,
		commandBuffers
	);
    VULKAN_ERROR_CHECK(vulkanResult, vkAllocateCommandBuffers, )

    for (i = 0; i < allocateCount; i += 1)
	{
		commandBufferData = SDL_malloc(sizeof(VULKAN_CommandBufferData));
		commandBufferData->commandPool = vulkanCommandPool;
		commandBufferData->commandBuffer = commandBuffers[i];
        commandBufferData->submitted = 0;

		vulkanCommandPool->inactiveCommandBuffers[
			vulkanCommandPool->inactiveCommandBufferCount
		] = commandBufferData;
		vulkanCommandPool->inactiveCommandBufferCount += 1;
	}

	SDL_stack_free(commandBuffers);
}

static VULKAN_CommandPool* VULKAN_INTERNAL_FetchCommandPool(
	VULKAN_GpuDeviceData *deviceData,
	SDL_threadID threadID
) {
	VULKAN_CommandPool *vulkanCommandPool;
    SDL_bool foundInHash;
	VkCommandPoolCreateInfo commandPoolCreateInfo;
	VkResult vulkanResult;

	foundInHash = SDL_FindInHashTable(
		deviceData->commandPoolHashTable,
		(void *) threadID,
        (const void **) &vulkanCommandPool
	);

	if (foundInHash)
	{
		return vulkanCommandPool;
	}

	vulkanCommandPool = (VULKAN_CommandPool*) SDL_malloc(sizeof(VULKAN_CommandPool));

	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = NULL;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = deviceData->queueFamilyIndex;

	vulkanResult = deviceData->vkCreateCommandPool(
		deviceData->logicalDevice,
		&commandPoolCreateInfo,
		NULL,
		&vulkanCommandPool->commandPool
	);
    VULKAN_ERROR_CHECK(vulkanResult, vkCreateCommandPool, NULL)

	vulkanCommandPool->threadID = threadID;

	vulkanCommandPool->inactiveCommandBufferCapacity = 0;
	vulkanCommandPool->inactiveCommandBufferCount = 0;
	vulkanCommandPool->inactiveCommandBuffers = NULL;

	VULKAN_INTERNAL_AllocateCommandBuffers(
		deviceData,
		vulkanCommandPool,
		2
	);

    SDL_InsertIntoHashTable(
        deviceData->commandPoolHashTable,
        (void *) threadID,
        vulkanCommandPool
    );

	return vulkanCommandPool;
}

static VULKAN_CommandBufferData* VULKAN_INTERNAL_GetInactiveCommandBufferFromPool(
	VULKAN_GpuDeviceData *deviceData,
	SDL_threadID threadID
) {
	VULKAN_CommandPool *commandPool =
		VULKAN_INTERNAL_FetchCommandPool(deviceData, threadID);
	VULKAN_CommandBufferData *commandBuffer;

	if (commandPool->inactiveCommandBufferCount == 0)
	{
		VULKAN_INTERNAL_AllocateCommandBuffers(
			deviceData,
			commandPool,
			commandPool->inactiveCommandBufferCapacity
		);
	}

	commandBuffer = commandPool->inactiveCommandBuffers[commandPool->inactiveCommandBufferCount - 1];
	commandPool->inactiveCommandBufferCount -= 1;

	return commandBuffer;
}

/* It's much more efficient to pool command buffers and reuse them in Vulkan. */
static int VULKAN_GpuCreateCommandBuffer(SDL_GpuCommandBuffer *cmdbuf)
{
    VULKAN_GpuDeviceData *deviceData = (VULKAN_GpuDeviceData*) cmdbuf->device->driverdata;
    SDL_threadID threadID = SDL_ThreadID();
    VULKAN_CommandBufferData *commandBufferData;
    VkResult vulkanResult;
    VkCommandBufferBeginInfo beginInfo;

    SDL_LockMutex(deviceData->acquireCommandBufferLock);

    commandBufferData = VULKAN_INTERNAL_GetInactiveCommandBufferFromPool(
        deviceData,
        threadID
    );

    SDL_UnlockMutex(deviceData->acquireCommandBufferLock);

    commandBufferData->submitted = 0;

    vulkanResult = deviceData->vkResetCommandBuffer(
        commandBufferData->commandBuffer,
        VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT
    );
    VULKAN_SET_ERROR(vulkanResult, vkResetCommandBuffer)

    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = NULL;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = NULL;

    vulkanResult = deviceData->vkBeginCommandBuffer(
        commandBufferData->commandBuffer,
        &beginInfo
    );
    VULKAN_SET_ERROR(vulkanResult, vkBeginCommandBuffer)

    cmdbuf->driverdata = commandBufferData;

    return 0;
}

static int VULKAN_GpuStartRenderPass(SDL_GpuRenderPass *pass, Uint32 num_color_attachments, const SDL_GpuColorAttachmentDescription *color_attachments, const SDL_GpuDepthAttachmentDescription *depth_attachment, const SDL_GpuStencilAttachmentDescription *stencil_attachment)
{
    return 0;
}

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

/* VkInstance Creation */

static VkBool32 VKAPI_PTR VULKAN_INTERNAL_DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void* pUserData
) {
    void (*logFunc)(const char *fmt, ...);
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        logFunc = VULKAN_LogError;
    }
    else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        logFunc = VULKAN_LogWarn;
    }
    else {
        logFunc = VULKAN_LogInfo;
    }
    logFunc("VULKAN DEBUG: %s", pCallbackData->pMessage);
    return VK_FALSE;
}

static inline uint8_t SupportsInstanceExtension(
	const char *ext,
	VkExtensionProperties *availableExtensions,
	uint32_t numAvailableExtensions
) {
	uint32_t i;
	for (i = 0; i < numAvailableExtensions; i += 1)
	{
		if (SDL_strcmp(ext, availableExtensions[i].extensionName) == 0)
		{
			return 1;
		}
	}
	return 0;
}

static uint8_t VULKAN_INTERNAL_CheckInstanceExtensions(
    const char **requiredExtensions,
    uint32_t requiredExtensionsLength,
    uint8_t *supportsDebugUtils
) {
    uint32_t extensionCount, i;
    VkExtensionProperties *availableExtensions;
    uint8_t allExtensionsSupported = 1;

    vkEnumerateInstanceExtensionProperties(
        NULL,
        &extensionCount,
        NULL
    );
    availableExtensions = SDL_malloc(
        extensionCount * sizeof(VkExtensionProperties)
    );
    vkEnumerateInstanceExtensionProperties(
        NULL,
        &extensionCount,
        availableExtensions
    );

    for (i = 0; i < requiredExtensionsLength; i += 1) {
        if (!SupportsInstanceExtension(
            requiredExtensions[i],
            availableExtensions,
            extensionCount
        )) {
            allExtensionsSupported = 0;
            break;
        }
    }

    /* This is optional, but nice to have! */
    *supportsDebugUtils = SupportsInstanceExtension(
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        availableExtensions,
        extensionCount
    );

    SDL_free(availableExtensions);
    return allExtensionsSupported;
}

static uint8_t VULKAN_INTERNAL_CheckValidationLayers(
	const char** validationLayers,
	uint32_t validationLayersLength
) {
	uint32_t layerCount;
	VkLayerProperties *availableLayers;
	uint32_t i, j;
	uint8_t layerFound;

	vkEnumerateInstanceLayerProperties(&layerCount, NULL);
	availableLayers = (VkLayerProperties*) SDL_malloc(
		layerCount * sizeof(VkLayerProperties)
	);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

	for (i = 0; i < validationLayersLength; i += 1)
	{
		layerFound = 0;

		for (j = 0; j < layerCount; j += 1)
		{
			if (SDL_strcmp(validationLayers[i], availableLayers[j].layerName) == 0)
			{
				layerFound = 1;
				break;
			}
		}

		if (!layerFound)
		{
			break;
		}
	}

	SDL_free(availableLayers);
	return layerFound;
}

static uint8_t VULKAN_INTERNAL_CreateInstance(
    VULKAN_GpuDeviceData *deviceData,
    SDL_Window *windowHandle
) {
    VkApplicationInfo appInfo;
    uint32_t instanceExtensionCount;
    const char **instanceExtensionNames;
    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo;
    VkInstanceCreateInfo instanceCreateInfo;
    static const char *layerNames[] = { "VK_LAYER_KHRONOS_validation" };
    VkResult vulkanResult;

    if (SDL_Vulkan_LoadLibrary(NULL) < 0) {
        VULKAN_LogWarn("SDL_Vulkan_LoadLibrary failed!");
        return 0;
    }

    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) SDL_Vulkan_GetVkGetInstanceProcAddr();

    if (vkGetInstanceProcAddr == NULL) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_GPU,
            "SDL_Vulkan_GetVkGetInstanceProcAddr(): %s",
            SDL_GetError()
        );
        return 0;
    }

    vkCreateInstance = (PFN_vkCreateInstance) vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
    vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties) vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties");
    vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties) vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");

    if (!SDL_Vulkan_GetInstanceExtensions(
        windowHandle,
        &instanceExtensionCount,
        NULL
    )) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_GPU,
            "SDL_Vulkan_GetInstanceExtensions(): getExtensionCount: %s",
            SDL_GetError()
        );
        return 0;
    }

    /* Extra space for the following extensions:
     * VK_KHR_get_physical_device_properties2
     * VK_EXT_debug_utils
     */
    instanceExtensionNames = SDL_stack_alloc(
        const char*,
        instanceExtensionCount + 2
    );

    if (!SDL_Vulkan_GetInstanceExtensions(
        windowHandle,
        &instanceExtensionCount,
        instanceExtensionNames
    )) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_GPU,
            "SDL_Vulkan_GetInstanceExtensions(): %s",
            SDL_GetError()
        );
        goto create_instance_fail;
    }

    instanceExtensionNames[instanceExtensionCount++] =
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;

    if (!VULKAN_INTERNAL_CheckInstanceExtensions(
        instanceExtensionNames,
        instanceExtensionCount,
        &deviceData->supportsDebugUtils
    )) {
        VULKAN_LogWarn(
            "Required Vulkan instance extensions not supported"
        );
        goto create_instance_fail;
    }

    if (deviceData->debugMode) {
        if (deviceData->supportsDebugUtils) {
            /* Append the debug extension to the end */
            instanceExtensionNames[instanceExtensionCount++] =
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

            debugMessengerCreateInfo.sType =
                VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugMessengerCreateInfo.pNext = NULL;
            debugMessengerCreateInfo.flags = 0;
            debugMessengerCreateInfo.messageSeverity = (
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
            );
            debugMessengerCreateInfo.messageType = (
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
            );
            debugMessengerCreateInfo.pfnUserCallback = VULKAN_INTERNAL_DebugCallback;
            debugMessengerCreateInfo.pUserData = NULL;
        }
        else
        {
            VULKAN_LogWarn(
                "%s is not supported!",
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME
            );
        }
    }

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = NULL;
    appInfo.pApplicationName = NULL;
    appInfo.applicationVersion = 0;
    appInfo.pEngineName = "SDL_GPU";
    appInfo.engineVersion = SDL_COMPILEDVERSION;
    appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    instanceCreateInfo.pNext = NULL;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.ppEnabledLayerNames = layerNames;
    instanceCreateInfo.enabledExtensionCount = instanceExtensionCount;
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensionNames;

    if (deviceData->debugMode) {
        instanceCreateInfo.enabledLayerCount = SDL_arraysize(layerNames);
        if (VULKAN_INTERNAL_CheckValidationLayers(
            layerNames,
            instanceCreateInfo.enabledLayerCount
        )) {
            VULKAN_LogInfo("Vulkan validation enabled! Expect debug-level performance!");
        }
        else {
            VULKAN_LogWarn("Validation layers not found, continuing without validation");
            instanceCreateInfo.enabledLayerCount = 0;
        }

        if (deviceData->supportsDebugUtils) {
            instanceCreateInfo.pNext = &debugMessengerCreateInfo;
        }
    }
    else {
        instanceCreateInfo.enabledLayerCount = 0;
    }

    vulkanResult = vkCreateInstance(&instanceCreateInfo, NULL, &deviceData->instance);
    if (vulkanResult != VK_SUCCESS) {
        VULKAN_LogWarn(
            "vkCreateInstance failed: %s",
            VkErrorMessages(vulkanResult)
        );
        goto create_instance_fail;
    }

    SDL_stack_free(instanceExtensionNames);
    return 1;

create_instance_fail:
    SDL_stack_free(instanceExtensionNames);
    return 0;
}

/* Physical device selection */

static uint8_t VULKAN_INTERNAL_CheckDeviceExtensions(
    VULKAN_GpuDeviceData *deviceData,
    VkPhysicalDevice physicalDevice,
    VulkanExtensions *physicalDeviceExtensions
) {
    uint32_t extensionCount;
    VkExtensionProperties *availableExtensions;
    uint8_t allExtensionsSupported;

    deviceData->vkEnumerateDeviceExtensionProperties(
        physicalDevice,
        NULL,
        &extensionCount,
        NULL
    );
    availableExtensions = (VkExtensionProperties*) SDL_malloc(
        extensionCount * sizeof(VkExtensionProperties)
    );
    deviceData->vkEnumerateDeviceExtensionProperties(
        physicalDevice,
        NULL,
        &extensionCount,
        availableExtensions
    );

    allExtensionsSupported = CheckDeviceExtensions(
        availableExtensions,
        extensionCount,
        physicalDeviceExtensions
    );

    SDL_free(availableExtensions);
    return allExtensionsSupported;
}

static uint8_t VULKAN_INTERNAL_QuerySwapChainSupport(
	VULKAN_GpuDeviceData *deviceData,
	VkPhysicalDevice physicalDevice,
	VkSurfaceKHR surface,
	SwapChainSupportDetails *outputDetails
) {
	VkResult result;
	VkBool32 supportsPresent;

	deviceData->vkGetPhysicalDeviceSurfaceSupportKHR(
		physicalDevice,
		deviceData->queueFamilyIndex,
		surface,
		&supportsPresent
	);

	if (!supportsPresent)
	{
		VULKAN_LogWarn("This surface does not support presenting!");
		return 0;
	}

	/* Initialize these in case anything fails */
	outputDetails->formatsLength = 0;
	outputDetails->presentModesLength = 0;

	/* Run the device surface queries */
	result = deviceData->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		physicalDevice,
		surface,
		&outputDetails->capabilities
	);
	VULKAN_SET_ERROR(result, vkGetPhysicalDeviceSurfaceCapabilitiesKHR)

	if (!(outputDetails->capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
	{
		VULKAN_LogWarn("Opaque presentation unsupported! Expect weird transparency bugs!");
	}

	result = deviceData->vkGetPhysicalDeviceSurfaceFormatsKHR(
		physicalDevice,
		surface,
		&outputDetails->formatsLength,
		NULL
	);
	VULKAN_SET_ERROR(result, vkGetPhysicalDeviceSurfaceFormatsKHR)
	result = deviceData->vkGetPhysicalDeviceSurfacePresentModesKHR(
		physicalDevice,
		surface,
		&outputDetails->presentModesLength,
		NULL
	);
	VULKAN_SET_ERROR(result, vkGetPhysicalDeviceSurfacePresentModesKHR)

	/* Generate the arrays, if applicable */
	if (outputDetails->formatsLength != 0)
	{
		outputDetails->formats = (VkSurfaceFormatKHR*) SDL_malloc(
			sizeof(VkSurfaceFormatKHR) * outputDetails->formatsLength
		);

		if (!outputDetails->formats)
		{
			SDL_OutOfMemory();
			return 0;
		}

		result = deviceData->vkGetPhysicalDeviceSurfaceFormatsKHR(
			physicalDevice,
			surface,
			&outputDetails->formatsLength,
			outputDetails->formats
		);
		if (result != VK_SUCCESS)
		{
			VULKAN_LogError(
				"vkGetPhysicalDeviceSurfaceFormatsKHR: %s",
				VkErrorMessages(result)
			);

			SDL_free(outputDetails->formats);
			return 0;
		}
	}
	if (outputDetails->presentModesLength != 0)
	{
		outputDetails->presentModes = (VkPresentModeKHR*) SDL_malloc(
			sizeof(VkPresentModeKHR) * outputDetails->presentModesLength
		);

		if (!outputDetails->presentModes)
		{
			SDL_OutOfMemory();
			return 0;
		}

		result = deviceData->vkGetPhysicalDeviceSurfacePresentModesKHR(
			physicalDevice,
			surface,
			&outputDetails->presentModesLength,
			outputDetails->presentModes
		);
		if (result != VK_SUCCESS)
		{
			VULKAN_LogError(
				"vkGetPhysicalDeviceSurfacePresentModesKHR: %s",
				VkErrorMessages(result)
			);

			SDL_free(outputDetails->formats);
			SDL_free(outputDetails->presentModes);
			return 0;
		}
	}

	/* If we made it here, all the queries were successfull. This does NOT
	 * necessarily mean there are any supported formats or present modes!
	 */
	return 1;
}

static uint8_t VULKAN_INTERNAL_IsDeviceSuitable(
    VULKAN_GpuDeviceData *deviceData,
    VkPhysicalDevice physicalDevice,
    VulkanExtensions *physicalDeviceExtensions,
    VkSurfaceKHR surface,
    int32_t *queueFamilyIndex,
    uint8_t *deviceRank
) {
    uint32_t queueFamilyCount, queueFamilyRank, queueFamilyBest;
    SwapChainSupportDetails swapchainSupportDetails;
    VkQueueFamilyProperties *queueProps;
    VkBool32 supportsPresent;
    uint8_t querySuccess;
    VkPhysicalDeviceProperties deviceProperties;
    uint32_t i;

    /* Get the device rank before doing any checks, in case one fails.
     * Note: If no dedicated device exists, one that supports our features
     * would be fine
     */
    deviceData->vkGetPhysicalDeviceProperties(
        physicalDevice,
        &deviceProperties
    );

    if (*deviceRank < DEVICE_PRIORITY[deviceProperties.deviceType]) {
        *deviceRank = DEVICE_PRIORITY[deviceProperties.deviceType];
    }
    else if (*deviceRank > DEVICE_PRIORITY[deviceProperties.deviceType]) {
        *deviceRank = 0;
        return 0;
    }

    if (!VULKAN_INTERNAL_CheckDeviceExtensions(
        deviceData,
        physicalDevice,
        physicalDeviceExtensions
    )) {
        return 0;
    }

    deviceData->vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        NULL
    );

    queueProps = (VkQueueFamilyProperties*) SDL_stack_alloc(
        VkQueueFamilyProperties,
        queueFamilyCount
    );
    deviceData->vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        queueProps
    );

    queueFamilyBest = 0;
    *queueFamilyIndex = INT32_MAX;
    for (i = 0; i < queueFamilyCount; i += 1)
    {
        deviceData->vkGetPhysicalDeviceSurfaceSupportKHR(
            physicalDevice,
            i,
            surface,
            &supportsPresent
        );
        if (	!supportsPresent ||
            !(queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)	)
        {
            /* Not a graphics family, ignore. */
            continue;
        }

        /* The queue family bitflags are kind of annoying.
         *
         * We of course need a graphics family, but we ideally want the
         * _primary_ graphics family. The spec states that at least one
         * graphics family must also be a compute family, so generally
         * drivers make that the first one. But hey, maybe something
         * genuinely can't do compute or something, and FNA doesn't
         * need it, so we'll be open to a non-compute queue family.
         *
         * Additionally, it's common to see the primary queue family
         * have the transfer bit set, which is great! But this is
         * actually optional; it's impossible to NOT have transfers in
         * graphics/compute but it _is_ possible for a graphics/compute
         * family, even the primary one, to just decide not to set the
         * bitflag. Admittedly, a driver may want to isolate transfer
         * queues to a dedicated family so that queues made solely for
         * transfers can have an optimized DMA queue.
         *
         * That, or the driver author got lazy and decided not to set
         * the bit. Looking at you, Android.
         *
         * -flibit
         */
        if (queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            if (queueProps[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                /* Has all attribs! */
                queueFamilyRank = 3;
            }
            else
            {
                /* Probably has a DMA transfer queue family */
                queueFamilyRank = 2;
            }
        }
        else
        {
            /* Just a graphics family, probably has something better */
            queueFamilyRank = 1;
        }
        if (queueFamilyRank > queueFamilyBest)
        {
            *queueFamilyIndex = i;
            queueFamilyBest = queueFamilyRank;
        }
    }

    SDL_stack_free(queueProps);

    if (*queueFamilyIndex == INT32_MAX) {
        /* Somehow no graphics queues existed. Compute-only device? */
        return 0;
    }

    	/* FIXME: Need better structure for checking vs storing support details */
	querySuccess = VULKAN_INTERNAL_QuerySwapChainSupport(
		deviceData,
		physicalDevice,
		surface,
		&swapchainSupportDetails
	);
	if (swapchainSupportDetails.formatsLength > 0) {
		SDL_free(swapchainSupportDetails.formats);
	}
	if (swapchainSupportDetails.presentModesLength > 0) {
		SDL_free(swapchainSupportDetails.presentModes);
	}

	return (	querySuccess &&
			swapchainSupportDetails.formatsLength > 0 &&
			swapchainSupportDetails.presentModesLength > 0	);
}

static uint8_t VULKAN_INTERNAL_DeterminePhysicalDevice(
    VULKAN_GpuDeviceData *deviceData,
    VkSurfaceKHR surface
) {
    VkResult vulkanResult;
    VkPhysicalDevice *physicalDevices;
    uint32_t physicalDeviceCount;
    VulkanExtensions *physicalDeviceExtensions;
    int32_t suitableIndex, highestRank, i;
    int32_t queueFamilyIndex, suitableQueueFamilyIndex;
    uint8_t deviceRank;

    vulkanResult = deviceData->vkEnumeratePhysicalDevices(
        deviceData->instance,
        &physicalDeviceCount,
        NULL
    );
    VULKAN_SET_ERROR(vulkanResult, vkEnumeratePhysicalDevices)

    if (physicalDeviceCount == 0) {
        VULKAN_LogWarn("Failed to find any GPUs with Vulkan support!");
        return 0;
    }

    physicalDevices = SDL_stack_alloc(VkPhysicalDevice, physicalDeviceCount);
    physicalDeviceExtensions = SDL_stack_alloc(VulkanExtensions, physicalDeviceCount);

    vulkanResult = deviceData->vkEnumeratePhysicalDevices(
        deviceData->instance,
        &physicalDeviceCount,
        physicalDevices
    );

    if (vulkanResult == VK_INCOMPLETE) {
        VULKAN_LogWarn("vkEnumeratePhysicalDevices returned VK_INCOMPLETE, will keep trying anyway...");
        vulkanResult = VK_SUCCESS;
    }

    if (vulkanResult != VK_SUCCESS) {
        VULKAN_LogWarn(
            "vkEnumeratePhysicalDevices failed: %s",
            VkErrorMessages(vulkanResult)
        );
        SDL_stack_free(physicalDevices);
        SDL_stack_free(physicalDeviceExtensions);
        return 0;
    }

    /* Any suitable device will do, but we'd like the best */
    suitableIndex = -1;
    highestRank = 0;

    for (i = 0; i < physicalDeviceCount; i += 1) {
        deviceRank = highestRank;
        if (VULKAN_INTERNAL_IsDeviceSuitable(
            deviceData,
            physicalDevices[i],
            &physicalDeviceExtensions[i],
            surface,
            &queueFamilyIndex,
            &deviceRank
        )) {
            /* Use this for rendering.
             * Note that this may override a previous device that
             * supports rendering, but shares the same device rank.
             */
            suitableIndex = i;
            suitableQueueFamilyIndex = queueFamilyIndex;
            highestRank = deviceRank;
        }
        else if (deviceRank > highestRank) {
            /* In this case, we found a... "realer?" GPU,
             * but it doesn't actually support our Vulkan.
             * We should disqualify all devices below as a
             * result, because if we don't we end up
             * ignoring real hardware and risk using
             * something like LLVMpipe instead!
             * -flibit
             */
            suitableIndex = -1;
            highestRank = deviceRank;
        }
    }

    if (suitableIndex != -1) {
        deviceData->supportedExtensions = physicalDeviceExtensions[suitableIndex];
        deviceData->physicalDevice = physicalDevices[suitableIndex];
        deviceData->queueFamilyIndex = suitableQueueFamilyIndex;
    }
    else {
        SDL_stack_free(physicalDevices);
        SDL_stack_free(physicalDeviceExtensions);
        return 0;
    }

    deviceData->physicalDeviceProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    if (deviceData->supportedExtensions.KHR_driver_properties) {
        deviceData->physicalDeviceDriverProperties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR;
        deviceData->physicalDeviceDriverProperties.pNext = NULL;

        deviceData->physicalDeviceProperties.pNext =
            &deviceData->physicalDeviceDriverProperties;
    }
    else {
        deviceData->physicalDeviceProperties.pNext = NULL;
    }

    deviceData->vkGetPhysicalDeviceProperties2KHR(
        deviceData->physicalDevice,
        &deviceData->physicalDeviceProperties
    );

    deviceData->vkGetPhysicalDeviceMemoryProperties(
        deviceData->physicalDevice,
        &deviceData->memoryProperties
    );

    SDL_stack_free(physicalDevices);
    SDL_stack_free(physicalDeviceExtensions);
    return 1;
}

/* Create VkDevice */

static uint8_t VULKAN_INTERNAL_CreateLogicalDevice(VULKAN_GpuDeviceData *deviceData)
{
    VkResult vulkanResult;
    VkDeviceQueueCreateInfo queueCreateInfo;
    float queuePriority = 1.0f;
    VkPhysicalDeviceFeatures deviceFeatures;
    VkDeviceCreateInfo deviceCreateInfo;
	VkPhysicalDevicePortabilitySubsetFeaturesKHR portabilityFeatures;
	const char **deviceExtensions;

    queueCreateInfo.sType =
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = NULL;
    queueCreateInfo.flags = 0;
    queueCreateInfo.queueFamilyIndex = deviceData->queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    /* Specify used device features */

    SDL_zero(deviceFeatures);
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    /* Create logical device */
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    if (deviceData->supportedExtensions.KHR_portability_subset) {
		portabilityFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
		portabilityFeatures.pNext = NULL;
		portabilityFeatures.constantAlphaColorBlendFactors = VK_FALSE;
		portabilityFeatures.events = VK_FALSE;
		portabilityFeatures.imageViewFormatReinterpretation = VK_FALSE;
		portabilityFeatures.imageViewFormatSwizzle = VK_TRUE;
		portabilityFeatures.imageView2DOn3DImage = VK_FALSE;
		portabilityFeatures.multisampleArrayImage = VK_FALSE;
		portabilityFeatures.mutableComparisonSamplers = VK_FALSE;
		portabilityFeatures.pointPolygons = VK_FALSE;
		portabilityFeatures.samplerMipLodBias = VK_FALSE; /* Technically should be true, but eh */
		portabilityFeatures.separateStencilMaskRef = VK_FALSE;
		portabilityFeatures.shaderSampleRateInterpolationFunctions = VK_FALSE;
		portabilityFeatures.tessellationIsolines = VK_FALSE;
		portabilityFeatures.tessellationPointMode = VK_FALSE;
		portabilityFeatures.triangleFans = VK_FALSE;
		portabilityFeatures.vertexAttributeAccessBeyondStride = VK_FALSE;
		deviceCreateInfo.pNext = &portabilityFeatures;
    }
    else
	{
		deviceCreateInfo.pNext = NULL;
	}

    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = NULL;
    deviceCreateInfo.enabledExtensionCount = GetDeviceExtensionCount(
        &deviceData->supportedExtensions
    );

	deviceExtensions = SDL_stack_alloc(
		const char*,
		deviceCreateInfo.enabledExtensionCount
	);

    CreateDeviceExtensionArray(&deviceData->supportedExtensions, deviceExtensions);
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    vulkanResult = deviceData->vkCreateDevice(
        deviceData->physicalDevice,
        &deviceCreateInfo,
        NULL,
        &deviceData->logicalDevice
    );
    SDL_stack_free(deviceExtensions);
    VULKAN_SET_ERROR(vulkanResult, vkCreateDevice)

	/* Load vkDevice entry points */

	#define VULKAN_DEVICE_FUNCTION(name) \
		deviceData->name = (PFN_##name) \
			deviceData->vkGetDeviceProcAddr( \
				deviceData->logicalDevice, \
				#name \
			);
	#include "SDL_gpu_vulkan_vkfuncs.h"

    deviceData->vkGetDeviceQueue(
        deviceData->logicalDevice,
        deviceData->queueFamilyIndex,
        0,
        &deviceData->unifiedQueue
    );

    return 1;
}

static int
VULKAN_GpuCreateDevice(SDL_GpuDevice *device, uint8_t debugMode)
{
    VULKAN_GpuDeviceData *deviceData;
    /* Need a dummy window to query extensions and swapchain support. */
    SDL_Window *dummyWindowHandle;
    /* Need a dummy surface to initialize physical device. */
    VkSurfaceKHR dummySurface;

    dummyWindowHandle = SDL_CreateWindow(
        "SDL_GPU Vulkan",
        0, 0,
        128, 128,
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN
    );

    if (dummyWindowHandle == NULL) {
        VULKAN_LogWarn("Could not create dummy window!");
        return -1;
    }

    deviceData = SDL_malloc(sizeof(VULKAN_GpuDeviceData));
    if (!deviceData) {
        return SDL_OutOfMemory();
    }

    /* Safety zero */
    SDL_memset(deviceData, '\0', sizeof(VULKAN_GpuDeviceData));

    /* Create the vkInstance */

    if (!VULKAN_INTERNAL_CreateInstance(deviceData, dummyWindowHandle))
    {
        VULKAN_LogError("Error creating Vulkan instance!");
        SDL_DestroyWindow(dummyWindowHandle);
        return -1;
    }

    /* Create the dummy surface */

    if (!SDL_Vulkan_CreateSurface(
        dummyWindowHandle,
        deviceData->instance,
        &dummySurface
    )) {
        VULKAN_LogError(
            "SDL_Vulkan_CreateSurface failed: %s",
            SDL_GetError()
        );
        return -1;
    }

    /* Get VkInstance entry points */

    #define VULKAN_INSTANCE_FUNCTION(name) \
        deviceData->name = (PFN_##name) vkGetInstanceProcAddr(deviceData->instance, #name);
    #include "SDL_gpu_vulkan_vkfuncs.h"

    /* Choose physical device */

    if (!VULKAN_INTERNAL_DeterminePhysicalDevice(deviceData, dummySurface)) {
        VULKAN_LogError("Failed to determine a suitable physical device!");
        return -1;
    }

    deviceData->vkDestroySurfaceKHR(
        deviceData->instance,
        dummySurface,
        NULL
    );

	VULKAN_LogInfo("SDL GPU Driver: Vulkan");
	VULKAN_LogInfo(
		"Vulkan Device: %s",
		deviceData->physicalDeviceProperties.properties.deviceName
	);
	if (deviceData->supportedExtensions.KHR_driver_properties) {
		VULKAN_LogInfo(
			"Vulkan Driver: %s %s",
			deviceData->physicalDeviceDriverProperties.driverName,
			deviceData->physicalDeviceDriverProperties.driverInfo
		);
		VULKAN_LogInfo(
			"Vulkan Conformance: %u.%u.%u",
			deviceData->physicalDeviceDriverProperties.conformanceVersion.major,
			deviceData->physicalDeviceDriverProperties.conformanceVersion.minor,
			deviceData->physicalDeviceDriverProperties.conformanceVersion.patch
		);
	}
	else {
		VULKAN_LogInfo("KHR_driver_properties unsupported! Bother your vendor about this!");
	}

    if (!VULKAN_INTERNAL_CreateLogicalDevice(deviceData)) {
        VULKAN_LogError("Failed to create logical device!");
        return -1;
    }

    deviceData->acquireCommandBufferLock = SDL_CreateMutex();

    /* Create command pool hash table */
    /* Vulkan only wants you to use command pools and buffers on threads they were created on */
    /* FIXME: is there some way we can detect when a thread is no longer in play? */

    deviceData->commandPoolHashTable = SDL_NewHashTable(
        NULL,
        512,
        HashCommandPool,
        KeyMatchCommandPool,
        NukeCommandPool,
        SDL_FALSE
    );

    if (deviceData->commandPoolHashTable == NULL)
    {
        VULKAN_LogError("Failed to create command pool hash table!");
        return -1;
    }

    device->driverdata = (void *) deviceData;
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

    return 0;
}

const SDL_GpuDriver VULKAN_GpuDriver = {
    "Vulkan", VULKAN_GpuCreateDevice
};

#endif /* SDL_GPU_VULKAN */

/* vi: set ts=4 sw=4 expandtab: */
