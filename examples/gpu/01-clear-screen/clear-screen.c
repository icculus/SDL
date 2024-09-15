/*
 * This example code sets up a GPU and clears the window to a different color
 * every frame, so you'll effectively get a window that's smoothly fading
 * between colors.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* we don't actually use any shaders in this one, so just give us lots of options for backends. */
#define TESTGPU_SUPPORTED_FORMATS (SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB)

/* We will use this gpu device to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_GPUDevice *gpu_device = NULL;

/* the current red color we're clearing to. */
static Uint8 red = 0;

/* When fading up, this is 1, when fading down, it's -1. */
static int fade_direction = 1;

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't initialize SDL!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    window = SDL_CreateWindow("examples/gpu/clear-screen", 640, 480, 0);
    if (!window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create window/renderer!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    gpu_device = SDL_CreateGPUDevice(TESTGPU_SUPPORTED_FORMATS, SDL_TRUE, NULL);
    if (!gpu_device) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL_CreateGPUDevice failed!", SDL_GetError(), window);
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(gpu_device, window)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL_ClaimWindowForGPUDevice failed!", SDL_GetError(), window);
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    Uint32 w, h;
    SDL_GPUCommandBuffer *cmdbuf;
    SDL_GPUTexture *swapchain_texture;

    /* we send instructions to the GPU through command buffers, so get one. */
    cmdbuf = SDL_AcquireGPUCommandBuffer(gpu_device);
    if (cmdbuf == NULL) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL_GPUAcquireCommandBuffer failed!", SDL_GetError(), window);
        return SDL_APP_FAILURE;
    }

    /* Get a thing to draw to. A swapchain texture is what will go to the screen next. */
    swapchain_texture = SDL_AcquireGPUSwapchainTexture(cmdbuf, window, &w, &h);
    if (swapchain_texture != NULL) {
        SDL_GPURenderPass *render_pass;
        SDL_GPUColorTargetInfo color_target_info;

        SDL_zero(color_target_info);
        color_target_info.texture = swapchain_texture;
        color_target_info.clear_color.r = ((float) red) / 255.0f;
        color_target_info.clear_color.a = 1.0f;  /* blue and green were zero'd already. */
        color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
        color_target_info.store_op = SDL_GPU_STOREOP_STORE;

        /* A render pass goes into the command buffer and tells the GPU the draw
           things. Since we just need to clear the screen, which is specified
           when starting the render pass, we can end it right away! */
        render_pass = SDL_BeginGPURenderPass(cmdbuf, &color_target_info, 1, NULL);
        SDL_EndGPURenderPass(render_pass);

        /* update the color for the next frame we will draw. */
        if (fade_direction > 0) {
            if (red == 255) {
                fade_direction = -1;
            } else {
                red++;
            }
        } else if (fade_direction < 0) {
            if (red == 0) {
                fade_direction = 1;
            } else {
                red--;
            }
        }
    }

    /* send the drawing work to the GPU. */
    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate)
{
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    /* SDL will clean up the window for us. */
}

