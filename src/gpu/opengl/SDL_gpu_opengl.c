/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_GPU_OPENGL

/* The gpu subsystem OpenGL driver */

#include "SDL3/SDL_gpu.h"
#include "SDL_gpu_opengl.h"
#include "SDL_gpu_glcommand.h"
#include "../../video/SDL_sysvideo.h"

#ifdef NDEBUG
#define CHECK_GL_ERROR
#else
#define CHECK_GL_ERROR \
    do { \
        GLenum gl__error = gl_data->glGetError(); \
        if (gl__error != GL_NO_ERROR) { \
            SDL_LogError(SDL_LOG_CATEGORY_RESERVED1, "openGL error: (%s, %d): %x", __FILE__, __LINE__, gl__error); \
        } \
    } while (0);
#endif

#define SET_GL_ERROR(msg) SDL_SetError("%s: OpenGL error: %d", msg, gl_data->glGetError())

static void PushDebugGroup(OGL_GpuDevice *gl_data, const char *msg, const char *label)
{
    char debug_msg[128];
    size_t len_msg = SDL_strlen(msg);
    SDL_assert(len_msg < sizeof(debug_msg));
    SDL_memcpy(debug_msg, msg, len_msg);
    SDL_strlcpy(&debug_msg[len_msg], label, sizeof(debug_msg) - len_msg);
    gl_data->glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, debug_msg);
}

static void APIENTRY DebugOutputCallBack(GLenum source, GLenum type, GLuint id, GLenum severity,
                                         GLsizei length, const GLchar* glmsg, const void* data)
{
    if (!glmsg) {return;}
    if (source == GL_DEBUG_SOURCE_APPLICATION) {return;} // our own messages
    const char* str_source;
    switch (source) {
    case GL_DEBUG_SOURCE_API            : str_source = "GL"             ; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM  : str_source = "window system"  ; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: str_source = "shader compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY    : str_source = "third party"    ; break;
    case GL_DEBUG_SOURCE_APPLICATION    : str_source = "application"    ; break;
    case GL_DEBUG_SOURCE_OTHER          : str_source = "other"          ; break;
    default                             : str_source = "unknown"        ; break;
    }
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        SDL_LogError(SDL_LOG_CATEGORY_RESERVED1, "OpenGL (%s): %s", str_source, glmsg);
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        SDL_LogError(SDL_LOG_CATEGORY_RESERVED1, "OpenGL (%s): Deprecated behavior: %s", str_source, glmsg);
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        SDL_LogError(SDL_LOG_CATEGORY_RESERVED1, "OpenGL (%s): Undefined behavior: %s", str_source, glmsg);
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        SDL_LogError(SDL_LOG_CATEGORY_RESERVED1, "OpenGL (%s): Portability: %s", str_source, glmsg);
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        SDL_LogWarn(SDL_LOG_CATEGORY_RESERVED1, "OpenGL (%s): Performance: %s", str_source, glmsg);
        break;
    case GL_DEBUG_TYPE_OTHER:
        SDL_LogInfo(SDL_LOG_CATEGORY_RESERVED1, "OpenGL (%s): %s", str_source, glmsg);
        break;
    case GL_DEBUG_TYPE_MARKER:
        SDL_LogInfo(SDL_LOG_CATEGORY_RESERVED1, "OpenGL (%s): %s", str_source, glmsg);
        break;
    default:
        SDL_LogInfo(SDL_LOG_CATEGORY_RESERVED1, "OpenGL (%s): %s", str_source, glmsg);
        break;
    }
}

static void OPENGL_GpuDestroyDevice(SDL_GpuDevice *device)
{
    OGL_GpuDevice *gl_data = device->driverdata;
    if (gl_data) {
        /* FIXME: UnClaimWindow() required: if window is destroyed before gpu objects,
         * GL context is not current anymore and we cannot delete these objects.
         * NOTE: testgpu_spinning_cube destroys the window first (when calling
         * SDLTest_CommonEvent() with SDL_EVENT_WINDOW_CLOSE_REQUESTED)
         */
        if (gl_data->fbo_render != 0) {
            gl_data->glDeleteFramebuffers(1, &gl_data->fbo_render);
        }
        if (gl_data->fbo_backbuffer != 0) {
            gl_data->glDeleteFramebuffers(1, &gl_data->fbo_backbuffer);
        }
        if (gl_data->texture_backbuffer != 0) {
            gl_data->glDeleteTextures(1, &gl_data->texture_backbuffer);
        }
        if (gl_data->context) {
            SDL_GL_DeleteContext(gl_data->context);
        }
        if (gl_data->dummy_window && gl_data->window) {
            SDL_DestroyWindow(gl_data->window);
        }
        SDL_free(gl_data);
        device->driverdata = NULL;
    }
}

static SDL_GpuPixelFormat PixelFormatFromGL(GLuint interal_pixel_format)
{
    switch (interal_pixel_format) {
    // case GL_R3_G3_B2: return ;
    // case GL_RGB4    : return ;
    // case GL_RGB5    : return ;
    // case GL_RGBA4   : return ;
    case GL_RGB5_A1 : return SDL_GPUPIXELFMT_BGR5A1;
    case GL_RGB565  : return SDL_GPUPIXELFMT_B5G6R5;
    // case GL_RGB8    : return ;
    case GL_RGBA8   : return SDL_GPUPIXELFMT_RGBA8;
    // case GL_RGB10_A2: return ;
    }
    return SDL_GPUPIXELFMT_INVALID;
}

static SDL_bool CheckFrameBuffer(OGL_GpuDevice *gl_data, GLuint fbo, SDL_bool draw_fb)
{
    CHECK_GL_ERROR;
    GLenum fb_status = gl_data->glCheckNamedFramebufferStatus(fbo, draw_fb ? GL_DRAW_FRAMEBUFFER : GL_READ_FRAMEBUFFER);
    const char* error_str = NULL;
    switch (fb_status) {
    case GL_FRAMEBUFFER_COMPLETE                     : break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT        : error_str = "incomplete attachement" ; break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: error_str = "missing attachement"    ; break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER       : error_str = "incomplete draw buffer" ; break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER       : error_str = "incomplete read buffer" ; break;
    case GL_FRAMEBUFFER_UNSUPPORTED                  : error_str = "unsuported"             ; break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE       : error_str = "incomplte multisample"  ; break;
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS     : error_str = "incomplete layer target"; break;
    default                                          : error_str = "other error"            ; break;
    }
    CHECK_GL_ERROR;
    if (error_str) {
        SDL_SetError("Framebuffer error: %s", error_str);
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

// recreate the back texture to match size and format of the window
static SDL_bool RecreateBackBufferTexture(SDL_GpuDevice *device)
{
    OGL_GpuDevice *gl_data = device->driverdata;
    int w, h;
    SDL_GetWindowSizeInPixels(gl_data->window, &w, &h);
    // SDL_GetWindowSize(window, &w, &h);

    Uint32 window_pixel_format = SDL_GetWindowPixelFormat(gl_data->window);
    GLuint gl_internal_format = 0;
    // TODO: select the internal format that matches best once more format are added to GPU texture
    switch (window_pixel_format) {
    case SDL_PIXELFORMAT_UNKNOWN     : gl_internal_format = 0; break;
    // case SDL_PIXELFORMAT_INDEX1LSB   : gl_internal_format = ; break;
    // case SDL_PIXELFORMAT_INDEX1MSB   : gl_internal_format = ; break;
    // case SDL_PIXELFORMAT_INDEX4LSB   : gl_internal_format = ; break;
    // case SDL_PIXELFORMAT_INDEX4MSB   : gl_internal_format = ; break;
    // case SDL_PIXELFORMAT_INDEX8      : gl_internal_format = ; break;
    case SDL_PIXELFORMAT_RGB332      : //gl_internal_format = GL_R3_G3_B2; break;
    // case SDL_PIXELFORMAT_XRGB4444    :
    case SDL_PIXELFORMAT_RGB444      :
    // case SDL_PIXELFORMAT_XBGR4444    :
    case SDL_PIXELFORMAT_BGR444      : //gl_internal_format = GL_RGB4; break;
    // case SDL_PIXELFORMAT_XRGB1555    :
    case SDL_PIXELFORMAT_RGB555      :
    // case SDL_PIXELFORMAT_XBGR1555    :
    case SDL_PIXELFORMAT_BGR555      : //gl_internal_format = GL_RGB5; break;
    case SDL_PIXELFORMAT_ARGB4444    :
    case SDL_PIXELFORMAT_RGBA4444    :
    case SDL_PIXELFORMAT_ABGR4444    :
    case SDL_PIXELFORMAT_BGRA4444    : //gl_internal_format = GL_RGBA4; break;
    case SDL_PIXELFORMAT_ARGB1555    :
    case SDL_PIXELFORMAT_RGBA5551    :
    case SDL_PIXELFORMAT_ABGR1555    :
    case SDL_PIXELFORMAT_BGRA5551    : //gl_internal_format = GL_RGB5_A1; break;
    case SDL_PIXELFORMAT_RGB565      :
    case SDL_PIXELFORMAT_BGR565      : //gl_internal_format = GL_RGB565; break;
    case SDL_PIXELFORMAT_RGB24       :
    case SDL_PIXELFORMAT_BGR24       :
    case SDL_PIXELFORMAT_XRGB8888    :
    case SDL_PIXELFORMAT_RGBX8888    :
    case SDL_PIXELFORMAT_XBGR8888    :
    case SDL_PIXELFORMAT_BGRX8888    : //gl_internal_format = GL_RGB8; break;
    case SDL_PIXELFORMAT_ARGB8888    :
    case SDL_PIXELFORMAT_RGBA8888    :
    case SDL_PIXELFORMAT_ABGR8888    :
    case SDL_PIXELFORMAT_BGRA8888    : gl_internal_format = GL_RGBA8; break;
    case SDL_PIXELFORMAT_ARGB2101010 : gl_internal_format = GL_RGB10_A2; break;
    // case SDL_PIXELFORMAT_YV12        : gl_internal_format = ; break;
    // case SDL_PIXELFORMAT_IYUV        : gl_internal_format = ; break;
    // case SDL_PIXELFORMAT_YUY2        : gl_internal_format = ; break;
    // case SDL_PIXELFORMAT_UYVY        : gl_internal_format = ; break;
    // case SDL_PIXELFORMAT_YVYU        : gl_internal_format = ; break;
    // case SDL_PIXELFORMAT_NV12        : gl_internal_format = ; break;
    // case SDL_PIXELFORMAT_NV21        : gl_internal_format = ; break;
    // case SDL_PIXELFORMAT_EXTERNAL_OES: gl_internal_format = ; break;
    }
    SDL_GpuPixelFormat sdl_format = PixelFormatFromGL(gl_internal_format);
    if (gl_internal_format == 0 || sdl_format == SDL_GPUPIXELFMT_INVALID) {
        SDL_SetError("invalid window pixel format");
        return SDL_FALSE;
    }

    if (w == gl_data->w_backbuffer && h == gl_data->h_backbuffer && sdl_format == gl_data->texture_backbuffer_format) {
        return SDL_TRUE;
    }

    // texture storage is immutable: create a new texture and put it in the framebuffer
    GLuint new_texture;
    gl_data->glCreateTextures(GL_TEXTURE_2D, 1, &new_texture);
    if (new_texture == 0) {
        SET_GL_ERROR("Could not create back buffer texture");
        return SDL_FALSE;
    }
    gl_data->glTextureStorage2D(new_texture, 1, gl_internal_format, w, h);
    gl_data->glNamedFramebufferTexture(gl_data->fbo_backbuffer, GL_COLOR_ATTACHMENT0, new_texture, 0);
    CHECK_GL_ERROR;
    // we read backbuffer when blitting to screen
    if (!CheckFrameBuffer(gl_data, gl_data->fbo_backbuffer, SDL_FALSE)) {
        // put back old texture
        gl_data->glNamedFramebufferTexture(gl_data->fbo_backbuffer, GL_COLOR_ATTACHMENT0, gl_data->texture_backbuffer, 0);
        gl_data->glDeleteTextures(1, &new_texture);
        return SDL_FALSE;
    }
    if (gl_data->texture_backbuffer != 0) {
        gl_data->glDeleteTextures(1, &gl_data->texture_backbuffer);
    }
    gl_data->glObjectLabel(GL_TEXTURE, new_texture, -1, "fake back texture");
    CHECK_GL_ERROR;
    gl_data->texture_backbuffer = new_texture;
    gl_data->texture_backbuffer_format = sdl_format;
    gl_data->w_backbuffer = w;
    gl_data->h_backbuffer = h;
    return SDL_TRUE;
}

static int OPENGL_GpuClaimWindow(SDL_GpuDevice *device, SDL_Window *window)
{
    OGL_GpuDevice *gl_data = device->driverdata;

    const Uint32 window_flags = SDL_GetWindowFlags(window);
    SDL_bool changed_window = SDL_FALSE;
    if (!(window_flags & SDL_WINDOW_OPENGL)) {
        changed_window = SDL_TRUE;
        if (SDL_RecreateWindow(window, (window_flags & ~(SDL_WINDOW_VULKAN | SDL_WINDOW_METAL)) | SDL_WINDOW_OPENGL) < 0) {
            return -1;
        }
    }

    if (SDL_GL_MakeCurrent(window, gl_data->context) < 0) {
        if (changed_window) {
            SDL_RecreateWindow(window, window_flags);
        }
        return -1;
    }
    if (gl_data->dummy_window) {
        SDL_DestroyWindow(gl_data->window);
    }
    gl_data->window = window;
    gl_data->dummy_window = SDL_FALSE;

    if (!RecreateBackBufferTexture(device)) {
        if (changed_window) {
            SDL_RecreateWindow(window, window_flags);
        }
        return -1;
    }
    CHECK_GL_ERROR;
    return 0;
}

static int OPENGL_GpuCreateCpuBuffer(SDL_CpuBuffer *buffer, const void *data)
{
    OGL_GpuDevice *gl_data = buffer->device->driverdata;
    GLuint glid;
    if (buffer->buflen > gl_data->max_buffer_size) {
        return SDL_SetError("Cpu buffer too large");
    }
    gl_data->glCreateBuffers(1, &glid);
    // TODO: add usage flag (read|write) to OPENGL_GpuCreateCpuBuffer
    gl_data->glNamedBufferStorage(glid, buffer->buflen, data,
                                  GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    if (buffer->label) {
        gl_data->glObjectLabel(GL_BUFFER, glid, -1, buffer->label);
    }
    CHECK_GL_ERROR;
    buffer->driverdata = (void*)(uintptr_t)glid;
    return 0;
}

static void OPENGL_GpuDestroyCpuBuffer(SDL_CpuBuffer *buffer)
{
    OGL_GpuDevice *gl_data = buffer->device->driverdata;
    GLuint glid = (GLuint)(uintptr_t)buffer->driverdata;
    if (glid != 0) {
        gl_data->glDeleteBuffers(1, &glid);
        CHECK_GL_ERROR;
    }
    buffer->driverdata = NULL;
}

static void *OPENGL_GpuLockCpuBuffer(SDL_CpuBuffer *buffer)
{
    OGL_GpuDevice *gl_data = buffer->device->driverdata;
    GLuint glid = (GLuint)(uintptr_t)buffer->driverdata;
    SDL_assert(glid != 0);
    void* ptr = gl_data->glMapNamedBuffer(glid, GL_READ_WRITE); // GL_READ_ONLY, GL_WRITE_ONLY, or GL_READ_WRITE
    CHECK_GL_ERROR;
    return ptr;
}

static int OPENGL_GpuUnlockCpuBuffer(SDL_CpuBuffer *buffer)
{
    OGL_GpuDevice *gl_data = buffer->device->driverdata;
    GLuint glid = (GLuint)(uintptr_t)buffer->driverdata;
    SDL_assert(glid != 0);
    SDL_bool r = gl_data->glUnmapNamedBuffer(glid);
    CHECK_GL_ERROR;
    return r ? 0 : -1;
}

static int OPENGL_GpuCreateBuffer(SDL_GpuBuffer *buffer)
{
    OGL_GpuDevice *gl_data = buffer->device->driverdata;
    GLuint glid;
    if (buffer->buflen > gl_data->max_buffer_size) {
        return SDL_SetError("Gpu buffer too large");
    }
    gl_data->glCreateBuffers(1, &glid);
    gl_data->glNamedBufferStorage(glid, buffer->buflen, NULL, 0);
    if (buffer->label) {
        gl_data->glObjectLabel(GL_BUFFER, glid, -1, buffer->label);
    }
    CHECK_GL_ERROR;
    buffer->driverdata = (void*)(uintptr_t)glid;
    return 0;
}

static void OPENGL_GpuDestroyBuffer(SDL_GpuBuffer *buffer)
{
    OGL_GpuDevice *gl_data = buffer->device->driverdata;
    GLuint glid = (GLuint)(uintptr_t)buffer->driverdata;
    if (glid != 0) {
        gl_data->glDeleteBuffers(1, &glid);
        CHECK_GL_ERROR;
    }
    buffer->driverdata = NULL;
}

static int GetTextureDimension(SDL_GpuTextureType data_type)
{
    switch (data_type) {
    case SDL_GPUTEXTYPE_1D        : return 1;
    case SDL_GPUTEXTYPE_1D_ARRAY  :
    case SDL_GPUTEXTYPE_CUBE      :
    case SDL_GPUTEXTYPE_2D        : return 2;
    case SDL_GPUTEXTYPE_2D_ARRAY  :
    case SDL_GPUTEXTYPE_CUBE_ARRAY:
    case SDL_GPUTEXTYPE_3D        : return 3;
    }
    SDL_assert(0);
    return 0;
}
static GLenum ToGLTextureTarget(SDL_GpuTextureType data_type)
{
    switch (data_type) {
    case SDL_GPUTEXTYPE_1D        : return GL_TEXTURE_1D;
    case SDL_GPUTEXTYPE_1D_ARRAY  : return GL_TEXTURE_1D_ARRAY;
    case SDL_GPUTEXTYPE_CUBE      : return GL_TEXTURE_CUBE_MAP;
    case SDL_GPUTEXTYPE_2D        : return GL_TEXTURE_2D;
    case SDL_GPUTEXTYPE_3D        : return GL_TEXTURE_3D;
    case SDL_GPUTEXTYPE_2D_ARRAY  : return GL_TEXTURE_2D_ARRAY;
    case SDL_GPUTEXTYPE_CUBE_ARRAY: return GL_TEXTURE_CUBE_MAP_ARRAY;
    }
    SDL_assert(0);
    return 0;
}
static GLenum ToGLInternalFormat(SDL_GpuPixelFormat data_format)
{
    switch (data_format) {
    case SDL_GPUPIXELFMT_B5G6R5          : return GL_RGB565;
    case SDL_GPUPIXELFMT_BGR5A1          : return GL_RGB5_A1;
    case SDL_GPUPIXELFMT_RGBA8           : return GL_RGBA8;
    case SDL_GPUPIXELFMT_RGBA8_sRGB      : return GL_SRGB8_ALPHA8;
    case SDL_GPUPIXELFMT_BGRA8           : return 0; // TODO: BGRA pixel format
    case SDL_GPUPIXELFMT_BGRA8_sRGB      : return 0;
    case SDL_GPUPIXELFMT_Depth24_Stencil8: return GL_DEPTH24_STENCIL8;
    case SDL_GPUPIXELFMT_INVALID         : return 0;
    }
    SDL_assert(0);
    return 0;
}
static int OPENGL_GpuCreateTexture(SDL_GpuTexture *texture)
{
    OGL_GpuDevice *gl_data = texture->device->driverdata;

    SDL_bool compressed = SDL_FALSE; // SDL_GPU_IsCompressedFormat(texture->desc.pixel_format);
    GLsizei w = texture->desc.width;
    GLsizei h = texture->desc.height;
    GLsizei depth = texture->desc.depth_or_slices;
    GLsizei n_mipmap = texture->desc.mipmap_levels;
    SDL_GpuPixelFormat data_format = texture->desc.pixel_format;
    SDL_GpuTextureType data_type = texture->desc.texture_type;

    // SDL_GPUTEXUSAGE_RENDER_TARGET: all formats are color renderable except RGB9_E5 and compressed formats
    // TODO: support SDL_GPUTEXUSAGE_SHADER_READ | SDL_GPUTEXUSAGE_SHADER_WRITE with
    // image load/store (ARB_shader_image_load_store)
    if (texture->desc.usage & (SDL_GPUTEXUSAGE_SHADER_READ | SDL_GPUTEXUSAGE_SHADER_WRITE)) {
        return SDL_SetError("texure usage not supported");
    }

    if (compressed && depth > 1) {
        return SDL_SetError("compressed texture array not supported"); // TODO: compressed texture array support
    }
    if (w > gl_data->max_texture_size
        || h > gl_data->max_texture_size
        || depth > gl_data->max_texture_depth) {
        // TODO: use GL_PROXY_TEXTURE_* to check accurate max size
        return SDL_SetError("texture too big");
    }

    GLenum gl_internal_format = ToGLInternalFormat(data_format);
    if (gl_internal_format == 0) {
        return SDL_SetError("texture format not supported");
    }

    GLenum gl_target = ToGLTextureTarget(data_type);
    if (gl_target == 0) {
        return SDL_SetError("texture type not supported");
    }

    GLuint glid;
    gl_data->glCreateTextures(gl_target, 1, &glid);
    if (glid == 0) {
        return SET_GL_ERROR("could not create texture");
    }
    CHECK_GL_ERROR;

    if (texture->desc.label) {
        gl_data->glObjectLabel(GL_TEXTURE, glid, -1, texture->desc.label);
    }
    gl_data->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    CHECK_GL_ERROR;

    if (compressed) {
        // glTextureParameteri(glid, GL_TEXTURE_BASE_LEVEL, 0);
        // glTextureParameteri(glid, GL_TEXTURE_MAX_LEVEL, n_mipmap - 1);
        // Sint32 block_size = 16;
        // if (   SDL_GPUPIXELFMT_COMPRESSED_RGBA_S3TC_DXT1_EXT       == data_format
        //     || SDL_GPUPIXELFMT_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT == data_format
        //     || SDL_GPUPIXELFMT_COMPRESSED_RED_RGTC1                == data_format
        //     || SDL_GPUPIXELFMT_COMPRESSED_SIGNED_RED_RGTC1         == data_format
        // ) {
        //     block_size = 8;
        // }
        // Sint32 offset = 0;
        // for (Sint32 i = 0; i < n_mipmap; ++i) {
        //     Sint32 size = SDL_max(1, (w + 3) / 4) * SDL_max(1, (h + 3) / 4) * block_size;
        //     // what is the DSA version of glCompressedTexImage2D
        //     // glCompressedTexImage2D(GL_TEXTURE_2D, i, gl_internal_format,
        //     //                        w, h, 0, size, NULL);
        //     offset += size;
        //     w = SDL_max(1, w/2);
        //     h = SDL_max(1, h/2);
        //     CHECK_GL_ERROR;
        // }
    } else {
        gl_data->glTextureParameteri(glid, GL_TEXTURE_BASE_LEVEL, 0);
        gl_data->glTextureParameteri(glid, GL_TEXTURE_MAX_LEVEL,  n_mipmap - 1);
        // TODO do we want to sample or compare depth texture in shader
        gl_data->glTextureParameteri(glid, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        switch (GetTextureDimension(data_type)) {
            case 0: SDL_assert(0); return -1;
            case 1: gl_data->glTextureStorage1D(glid, n_mipmap, gl_internal_format, w); break;
            case 2: gl_data->glTextureStorage2D(glid, n_mipmap, gl_internal_format, w, h); break;
            case 3: gl_data->glTextureStorage3D(glid, n_mipmap, gl_internal_format, w, h, depth); break;
        }
        CHECK_GL_ERROR;
    }
    texture->driverdata = (void*)(uintptr_t)glid;
    return 0;
}

static void OPENGL_GpuDestroyTexture(SDL_GpuTexture *texture)
{
    OGL_GpuDevice *gl_data = texture->device->driverdata;
    GLuint glid = (GLuint)(uintptr_t)texture->driverdata;
    if (glid != 0) {
        gl_data->glDeleteTextures(1, &glid);
        CHECK_GL_ERROR;
    }
    texture->driverdata = NULL;
}

static GLuint CreateShader(OGL_GpuDevice* gl_data, const char* src, SDL_bool is_vert_shader)
{
    SDL_assert(src);
    GLchar error_log[512];
    GLint success = GL_FALSE;

    GLuint shader = gl_data->glCreateShader(is_vert_shader ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
    if (shader == 0) {
        SET_GL_ERROR("could not create shader");
        return 0;
    }

    gl_data->glShaderSource(shader, 1, &src, NULL);
    gl_data->glCompileShader(shader);
    gl_data->glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    error_log[0] = 0;
    gl_data->glGetShaderInfoLog(shader, sizeof(error_log), NULL, error_log);
    error_log[sizeof(error_log)-1] = 0;
    if (error_log[0] != 0) {
        SDL_LogInfo(SDL_LOG_CATEGORY_RESERVED1, "%s: %s",
                    "shader compilation log", error_log);
    }
    if (success == GL_FALSE) {
        gl_data->glDeleteShader(shader);
        SET_GL_ERROR("vertex shader compilation failed (check SDL log for more information)");
        return 0;
    }
    CHECK_GL_ERROR;
    return shader;
}

static int OPENGL_GpuCreateShader(SDL_GpuShader *shader, const Uint8 *bytecode, const Uint32 bytecodelen)
{
    OGL_GpuDevice *gl_data = shader->device->driverdata;
    if (shader->label) {
        PushDebugGroup(gl_data, "create shader: ", shader->label);
    }

    // Interpret bytecode as vert or frag GLSL source
    // NOTE: Put comment on first line to tell if it is a vertex or fragment shader
    SDL_bool is_vert_shader;
    if (SDL_strncmp((const char*)bytecode, "// vert", 7) == 0) {
        is_vert_shader = SDL_TRUE;
    } else if (SDL_strncmp((const char*)bytecode, "// frag", 7) == 0) {
        is_vert_shader = SDL_FALSE;
    } else {
        return -1;
    }

    const char* src = (const char*)bytecode;
    GLuint shader_glid = CreateShader(gl_data, src, is_vert_shader);
    if (shader_glid == 0) {
        return -1;
    }

    if (shader->label) {
        gl_data->glObjectLabel(GL_SHADER, shader_glid, -1, shader->label);
    }
    shader->driverdata = (void*)(uintptr_t)shader_glid;
    gl_data->glPopDebugGroup();
    CHECK_GL_ERROR;
    return 0;
}

static void OPENGL_GpuDestroyShader(SDL_GpuShader *shader)
{
    OGL_GpuDevice *gl_data = shader->device->driverdata;
    GLuint shader_glid = (GLuint)(uintptr_t)shader->driverdata;
    if (shader_glid) {
        gl_data->glDeleteShader(shader_glid);
        CHECK_GL_ERROR;
    }
    shader->driverdata = NULL;
}

static GLint VertexFormatSize(SDL_GpuVertexFormat f)
{
    switch (f) {
    case SDL_GPUVERTFMT_INVALID           : return 0;
    case SDL_GPUVERTFMT_USHORT            :
    case SDL_GPUVERTFMT_SHORT             :
    case SDL_GPUVERTFMT_USHORT_NORMALIZED :
    case SDL_GPUVERTFMT_SHORT_NORMALIZED  :
    case SDL_GPUVERTFMT_HALF              :
    case SDL_GPUVERTFMT_FLOAT             :
    case SDL_GPUVERTFMT_UINT              :
    case SDL_GPUVERTFMT_INT               : return 1;
    case SDL_GPUVERTFMT_UCHAR2            :
    case SDL_GPUVERTFMT_CHAR2             :
    case SDL_GPUVERTFMT_UCHAR2_NORMALIZED :
    case SDL_GPUVERTFMT_CHAR2_NORMALIZED  :
    case SDL_GPUVERTFMT_USHORT2           :
    case SDL_GPUVERTFMT_SHORT2            :
    case SDL_GPUVERTFMT_USHORT2_NORMALIZED:
    case SDL_GPUVERTFMT_SHORT2_NORMALIZED :
    case SDL_GPUVERTFMT_HALF2             :
    case SDL_GPUVERTFMT_FLOAT2            :
    case SDL_GPUVERTFMT_UINT2             :
    case SDL_GPUVERTFMT_INT2              : return 2;
    case SDL_GPUVERTFMT_FLOAT3            :
    case SDL_GPUVERTFMT_UINT3             :
    case SDL_GPUVERTFMT_INT3              : return 3;
    case SDL_GPUVERTFMT_UCHAR4            :
    case SDL_GPUVERTFMT_CHAR4             :
    case SDL_GPUVERTFMT_UCHAR4_NORMALIZED :
    case SDL_GPUVERTFMT_CHAR4_NORMALIZED  :
    case SDL_GPUVERTFMT_USHORT4           :
    case SDL_GPUVERTFMT_SHORT4            :
    case SDL_GPUVERTFMT_USHORT4_NORMALIZED:
    case SDL_GPUVERTFMT_SHORT4_NORMALIZED :
    case SDL_GPUVERTFMT_HALF4             :
    case SDL_GPUVERTFMT_FLOAT4            :
    case SDL_GPUVERTFMT_UINT4             :
    case SDL_GPUVERTFMT_INT4              : return 4;
    }
    SDL_assert(0);
    return 0;
}
static GLenum ToGLVertexType(SDL_GpuVertexFormat f)
{
    switch (f) {
    case SDL_GPUVERTFMT_INVALID           : return GL_NONE;
    case SDL_GPUVERTFMT_UCHAR2            :
    case SDL_GPUVERTFMT_UCHAR4            :
    case SDL_GPUVERTFMT_UCHAR2_NORMALIZED :
    case SDL_GPUVERTFMT_UCHAR4_NORMALIZED : return GL_UNSIGNED_BYTE;
    case SDL_GPUVERTFMT_CHAR2             :
    case SDL_GPUVERTFMT_CHAR4             :
    case SDL_GPUVERTFMT_CHAR2_NORMALIZED  :
    case SDL_GPUVERTFMT_CHAR4_NORMALIZED  : return GL_BYTE;
    case SDL_GPUVERTFMT_USHORT            :
    case SDL_GPUVERTFMT_USHORT2           :
    case SDL_GPUVERTFMT_USHORT4           :
    case SDL_GPUVERTFMT_USHORT_NORMALIZED :
    case SDL_GPUVERTFMT_USHORT2_NORMALIZED:
    case SDL_GPUVERTFMT_USHORT4_NORMALIZED: return GL_UNSIGNED_SHORT;
    case SDL_GPUVERTFMT_SHORT             :
    case SDL_GPUVERTFMT_SHORT2            :
    case SDL_GPUVERTFMT_SHORT4            :
    case SDL_GPUVERTFMT_SHORT_NORMALIZED  :
    case SDL_GPUVERTFMT_SHORT2_NORMALIZED :
    case SDL_GPUVERTFMT_SHORT4_NORMALIZED : return GL_SHORT;
    case SDL_GPUVERTFMT_HALF              :
    case SDL_GPUVERTFMT_HALF2             :
    case SDL_GPUVERTFMT_HALF4             : return GL_HALF_FLOAT;
    case SDL_GPUVERTFMT_FLOAT             :
    case SDL_GPUVERTFMT_FLOAT2            :
    case SDL_GPUVERTFMT_FLOAT3            :
    case SDL_GPUVERTFMT_FLOAT4            : return GL_FLOAT;
    case SDL_GPUVERTFMT_UINT              :
    case SDL_GPUVERTFMT_UINT2             :
    case SDL_GPUVERTFMT_UINT3             :
    case SDL_GPUVERTFMT_UINT4             : return GL_UNSIGNED_INT;
    case SDL_GPUVERTFMT_INT               :
    case SDL_GPUVERTFMT_INT2              :
    case SDL_GPUVERTFMT_INT3              :
    case SDL_GPUVERTFMT_INT4              : return GL_INT;
    }
    SDL_assert(0);
    return 0;
}
static GLboolean IsVertexFormatNormalised(SDL_GpuVertexFormat f)
{
    switch (f) {
    case SDL_GPUVERTFMT_UCHAR2_NORMALIZED :
    case SDL_GPUVERTFMT_UCHAR4_NORMALIZED :
    case SDL_GPUVERTFMT_CHAR2_NORMALIZED  :
    case SDL_GPUVERTFMT_CHAR4_NORMALIZED  :
    case SDL_GPUVERTFMT_USHORT_NORMALIZED :
    case SDL_GPUVERTFMT_USHORT2_NORMALIZED:
    case SDL_GPUVERTFMT_USHORT4_NORMALIZED:
    case SDL_GPUVERTFMT_SHORT_NORMALIZED  :
    case SDL_GPUVERTFMT_SHORT2_NORMALIZED :
    case SDL_GPUVERTFMT_SHORT4_NORMALIZED : return GL_TRUE;
    case SDL_GPUVERTFMT_INVALID           :
    case SDL_GPUVERTFMT_UCHAR2            :
    case SDL_GPUVERTFMT_UCHAR4            :
    case SDL_GPUVERTFMT_CHAR2             :
    case SDL_GPUVERTFMT_CHAR4             :
    case SDL_GPUVERTFMT_USHORT            :
    case SDL_GPUVERTFMT_USHORT2           :
    case SDL_GPUVERTFMT_USHORT4           :
    case SDL_GPUVERTFMT_SHORT             :
    case SDL_GPUVERTFMT_SHORT2            :
    case SDL_GPUVERTFMT_SHORT4            :
    case SDL_GPUVERTFMT_HALF              :
    case SDL_GPUVERTFMT_HALF2             :
    case SDL_GPUVERTFMT_HALF4             :
    case SDL_GPUVERTFMT_FLOAT             :
    case SDL_GPUVERTFMT_FLOAT2            :
    case SDL_GPUVERTFMT_FLOAT3            :
    case SDL_GPUVERTFMT_FLOAT4            :
    case SDL_GPUVERTFMT_UINT              :
    case SDL_GPUVERTFMT_UINT2             :
    case SDL_GPUVERTFMT_UINT3             :
    case SDL_GPUVERTFMT_UINT4             :
    case SDL_GPUVERTFMT_INT               :
    case SDL_GPUVERTFMT_INT2              :
    case SDL_GPUVERTFMT_INT3              :
    case SDL_GPUVERTFMT_INT4              : return GL_FALSE;
    }
    return GL_FALSE;
}

GLuint CreateProgram(OGL_GpuDevice* gl_data, GLuint vert_shader, GLuint frag_shader)
{
    GLchar error_log[512];
    GLint success = GL_FALSE;
    GLuint prog = gl_data->glCreateProgram();
    if (prog == 0) {
        SET_GL_ERROR("could not create shader program");
        return 0;
    }
    CHECK_GL_ERROR;

    gl_data->glAttachShader(prog, vert_shader);
    gl_data->glAttachShader(prog, frag_shader);
    gl_data->glLinkProgram(prog);
    gl_data->glGetProgramiv(prog, GL_LINK_STATUS, &success);
    error_log[0] = 0;
    gl_data->glGetProgramInfoLog(prog, sizeof(error_log), NULL, error_log);
    error_log[sizeof(error_log)-1] = 0;
    if (error_log[0] != 0) {
        SDL_LogInfo(SDL_LOG_CATEGORY_RESERVED1, "%s: %s",
                    "shader program link log", error_log);
    }
    if (success == GL_FALSE) {
        gl_data->glDeleteProgram(prog);
        SET_GL_ERROR("shader program link failed (check SDL log for more information)");
        return 0;
    }
    CHECK_GL_ERROR;

    // gl_data->glUseProgram(prog);
    gl_data->glValidateProgram(prog);
    gl_data->glGetProgramiv(prog, GL_VALIDATE_STATUS, &success);
    error_log[0] = 0;
    gl_data->glGetProgramInfoLog(prog, sizeof(error_log), NULL, error_log);
    error_log[sizeof(error_log)-1] = 0;
    if (error_log[0] != 0) {
        SDL_LogInfo(SDL_LOG_CATEGORY_RESERVED1, "%s: %s",
                    "shader program validation log", error_log);
    }
    if (success == GL_FALSE) {
        gl_data->glDeleteProgram(prog);
        SET_GL_ERROR("shader program valiation failed (check SDL log for more information)");
        return 0;
    }
    CHECK_GL_ERROR;
    return prog;
}

static int OPENGL_GpuCreatePipeline(SDL_GpuPipeline *pipeline)
{
    OGL_GpuDevice *gl_data = pipeline->device->driverdata;
    if (pipeline->desc.label) {
        PushDebugGroup(gl_data, "create pipeline: ", pipeline->desc.label);
    }

    /* GpuPipeline only stores vertex format and program pipeline, other states
     * are set in SetRenderPassPipeline().
     */

    if (pipeline->desc.num_vertex_attributes > gl_data->max_vertex_attrib) {
        gl_data->glPopDebugGroup();
        return SDL_SetError("too many vertex attribute");
    }

    GLuint vao;
    gl_data->glCreateVertexArrays(1, &vao);
    if (vao == 0) {
        gl_data->glPopDebugGroup();
        return SET_GL_ERROR("could not create vertex array");
    }
    if (pipeline->desc.label) {
        gl_data->glObjectLabel(GL_VERTEX_ARRAY, vao, -1, pipeline->desc.label);
    }
    for (Uint32 i = 0; i < pipeline->desc.num_vertex_attributes; ++i) {
        SDL_GpuVertexAttributeDescription* attrib = &pipeline->desc.vertices[i];
        GLint size = VertexFormatSize(attrib->format);
        GLenum type = ToGLVertexType(attrib->format);
        SDL_bool integer = SDL_FALSE;
        GLboolean normalised = IsVertexFormatNormalised(attrib->format);

        if (type == GL_FLOAT || type == GL_HALF_FLOAT/* || type == GL_FIXED || type == GL_DOUBLE*/) { // float
            normalised = GL_FALSE; // OpenGL requirement
        } else { // integer
            if (!normalised) {
                integer = SDL_TRUE;
            }
        }

        // stride is part of the draw command, not the vao
        gl_data->glEnableVertexArrayAttrib(vao, attrib->index);
        /*if (type == GL_DOUBLE) {
            gl_data->glVertexArrayAttribLFormat(vao, attrib->index, size, type, attrib->offset);
        } else */
        if (integer) {
            gl_data->glVertexArrayAttribIFormat(vao, attrib->index, size, type, attrib->offset);
        } else { // float or normalised integer
            gl_data->glVertexArrayAttribFormat(vao, attrib->index, size, type, normalised, attrib->offset);
        }
        gl_data->glVertexArrayAttribBinding(vao, attrib->index, 0);
        CHECK_GL_ERROR;
    }

    GLuint vert_shader = (GLuint)(uintptr_t)pipeline->desc.vertex_shader->driverdata;
    GLuint frag_shader = (GLuint)(uintptr_t)pipeline->desc.fragment_shader->driverdata;
    SDL_assert(vert_shader != 0);
    SDL_assert(frag_shader != 0);
    GLuint program = CreateProgram(gl_data, vert_shader, frag_shader);
    if (program == 0) {
        gl_data->glDeleteVertexArrays(1, &vao);
        gl_data->glPopDebugGroup();
        return -1;
    }
    if (pipeline->desc.label) {
        gl_data->glObjectLabel(GL_PROGRAM, program, -1, pipeline->desc.label);
    }
    CHECK_GL_ERROR;

    SDL_COMPILE_TIME_ASSERT(pointer_stuffing, sizeof(uintptr_t) == sizeof(Uint64));
    pipeline->driverdata = (void*)(uintptr_t)((Uint64)vao<<32 | program);
    gl_data->glPopDebugGroup();
    return 0;
}

static void OPENGL_GpuDestroyPipeline(SDL_GpuPipeline *pipeline)
{
    OGL_GpuDevice *gl_data = pipeline->device->driverdata;
    GLuint program_glid = (GLuint)((uintptr_t)pipeline->driverdata & 0xFFFFFFFF);
    GLuint vao_glid = (GLuint)((uintptr_t)pipeline->driverdata >> 32);
    if (program_glid != 0) {
        gl_data->glDeleteProgram(program_glid);
    }
    if (vao_glid != 0) {
        gl_data->glDeleteVertexArrays(1, &vao_glid);
    }
    CHECK_GL_ERROR;
    pipeline->driverdata = NULL;
}

static GLint ToGLFilter(SDL_GpuSamplerMinMagFilter f, SDL_GpuSamplerMipFilter m)
{
    switch (f) {
    case SDL_GPUMINMAGFILTER_NEAREST:
        switch (m) {
        case SDL_GPUMIPFILTER_NOTMIPMAPPED: return GL_NEAREST;
        case SDL_GPUMIPFILTER_NEAREST     : return GL_NEAREST_MIPMAP_NEAREST;
        case SDL_GPUMIPFILTER_LINEAR      : return GL_NEAREST_MIPMAP_LINEAR;
        }
        break;
    case SDL_GPUMINMAGFILTER_LINEAR:
        switch (m) {
        case SDL_GPUMIPFILTER_NOTMIPMAPPED: return GL_LINEAR;
        case SDL_GPUMIPFILTER_NEAREST     : return GL_LINEAR_MIPMAP_NEAREST;
        case SDL_GPUMIPFILTER_LINEAR      : return GL_LINEAR_MIPMAP_LINEAR;
        }
        break;
    }
    SDL_assert(0);
    return GL_NEAREST;
}
static GLint ToGLWrap(SDL_GpuSamplerAddressMode w)
{
    switch (w) {
    case SDL_GPUSAMPADDR_CLAMPTOEDGE       : return GL_CLAMP_TO_EDGE;
    case SDL_GPUSAMPADDR_MIRRORCLAMPTOEDGE : return GL_MIRROR_CLAMP_TO_EDGE;
    case SDL_GPUSAMPADDR_REPEAT            : return GL_REPEAT;
    case SDL_GPUSAMPADDR_MIRRORREPEAT      : return GL_MIRRORED_REPEAT;
    // FIXME: SDL_GPUSAMPADDR_CLAMPTOZERO does not exist in OpenGL
    // Metal doc:
    // Out-of-range texture coordinates return transparent zero (0,0,0,0) for
    // images with an alpha channel and return opaque zero (0,0,0,1) for images
    // without an alpha channel.
    case SDL_GPUSAMPADDR_CLAMPTOZERO       : return GL_CLAMP_TO_BORDER;
    case SDL_GPUSAMPADDR_CLAMPTOBORDERCOLOR: return GL_CLAMP_TO_BORDER;
    }
    SDL_assert(0);
    return GL_CLAMP_TO_EDGE;
}
static float border_color[3][4] = {
    {0.f, 0.f, 0.f, 0.f},
    {0.f, 0.f, 0.f, 1.f},
    {1.f, 1.f, 1.f, 1.f},
};
static int OPENGL_GpuCreateSampler(SDL_GpuSampler *sampler)
{
    OGL_GpuDevice *gl_data = sampler->device->driverdata;
    GLuint glid;
    gl_data->glCreateSamplers(1, &glid);
    if (glid == 0) {
        return SET_GL_ERROR("could not create sampler");
    }
    if (sampler->desc.label) {
        gl_data->glObjectLabel(GL_SAMPLER, glid, -1, sampler->desc.label);
    }

    gl_data->glSamplerParameteri(glid, GL_TEXTURE_MIN_FILTER,    ToGLFilter(sampler->desc.min_filter, sampler->desc.mip_filter));
    gl_data->glSamplerParameteri(glid, GL_TEXTURE_MAG_FILTER,    ToGLFilter(sampler->desc.mag_filter, SDL_GPUMIPFILTER_NOTMIPMAPPED));
    gl_data->glSamplerParameteri(glid, GL_TEXTURE_WRAP_S,        ToGLWrap(sampler->desc.addrmode_u));
    gl_data->glSamplerParameteri(glid, GL_TEXTURE_WRAP_T,        ToGLWrap(sampler->desc.addrmode_v));
    gl_data->glSamplerParameteri(glid, GL_TEXTURE_WRAP_R,        ToGLWrap(sampler->desc.addrmode_r));
    gl_data->glSamplerParameterfv(glid, GL_TEXTURE_BORDER_COLOR, border_color[sampler->desc.border_color]);
    GLint anisotropy = SDL_min(SDL_max(sampler->desc.max_anisotropy, 1), gl_data->max_anisotropy);
    gl_data->glSamplerParameterf(glid, GL_TEXTURE_MAX_ANISOTROPY, anisotropy);
    CHECK_GL_ERROR;

    sampler->driverdata = (void*)(uintptr_t)glid;
    return 0;
}

static void OPENGL_GpuDestroySampler(SDL_GpuSampler *sampler)
{
    OGL_GpuDevice *gl_data = sampler->device->driverdata;
    GLuint glid = (GLuint)(uintptr_t)sampler->driverdata;
    if (glid != 0) {
        gl_data->glDeleteSamplers(1, &glid);
        CHECK_GL_ERROR;
    }
    sampler->driverdata = NULL;
}

static int OPENGL_GpuCreateCommandBuffer(SDL_GpuCommandBuffer *cmdbuf)
{
#define INITIAL_CMD_BUFFER_CAPACITY (128*1024)
    OPENGL_GpuCommandBuffer* glcmdbuf = SDL_calloc(1, sizeof(*glcmdbuf) + INITIAL_CMD_BUFFER_CAPACITY);
    if (!glcmdbuf) {
        return -1;
    }
    glcmdbuf->capacity_cmd = INITIAL_CMD_BUFFER_CAPACITY;
    cmdbuf->driverdata = glcmdbuf;
    return 0;
}

static int OPENGL_PushCommand(SDL_GpuCommandBuffer *cmdbuf, void* cmd, size_t size_cmd)
{
    OPENGL_GpuCommandBuffer* glcmdbuf = cmdbuf->driverdata;
    size_t new_n;
    if (SDL_size_add_overflow(glcmdbuf->n_cmd, size_cmd, &new_n) != 0) {
        return SDL_OutOfMemory();
    }
    if (new_n > glcmdbuf->capacity_cmd) {
        size_t new_capacity;
        if (SDL_size_mul_overflow(glcmdbuf->capacity_cmd, 2, &new_capacity) != 0) {
            return SDL_OutOfMemory();
        }
        size_t total_size;
        if (SDL_size_add_overflow(new_capacity, sizeof(*glcmdbuf), &total_size) != 0) {
            return SDL_OutOfMemory();
        }
        OPENGL_GpuCommandBuffer* new_glcmdbuf = SDL_realloc(glcmdbuf, total_size);
        if (!new_glcmdbuf) {
            return -1;
        }
        cmdbuf->driverdata = new_glcmdbuf;
        glcmdbuf = new_glcmdbuf;
        glcmdbuf->capacity_cmd = new_capacity;
        if (glcmdbuf->encoding_state.current_render_pass) {
            glcmdbuf->encoding_state.current_render_pass->driverdata =
                &glcmdbuf->encoding_state.currrent_render_pass_data;
        }
    }
    SDL_memcpy(&glcmdbuf->cmd[glcmdbuf->n_cmd], cmd, size_cmd);
    glcmdbuf->n_cmd = new_n;
    return 0;
}

static int OPENGL_GpuStartRenderPass(SDL_GpuRenderPass *pass, Uint32 num_color_attachments,
                                     const SDL_GpuColorAttachmentDescription *color_attachments,
                                     const SDL_GpuDepthAttachmentDescription *depth_attachment,
                                     const SDL_GpuStencilAttachmentDescription *stencil_attachment)
{
    // TODO: check GL_MAX_DRAW_BUFFERS value, minimum is 8 so it is fine for now
    SDL_COMPILE_TIME_ASSERT(max_color_attachment, SDL_GPU_MAX_COLOR_ATTACHMENTS <= 8);

    GLCMD_StartRenderPass cmd;
    SDL_memset(&cmd, 0, sizeof(cmd));
    cmd.type = CMD_START_RENDER_PASS;

    if (pass->label) {
        cmd.pass_label = SDL_strdup(pass->label);
    } else {
        cmd.pass_label = NULL;
    }

    GLsizei render_target_height = INT32_MAX;
    cmd.n_color_attachments = num_color_attachments;
    for (Uint32 i = 0; i < num_color_attachments; ++i) {
        if (color_attachments[i].texture == 0) {
            cmd.color_attachments[i] = 0;
            cmd.draw_buffers[i] = GL_NONE;
        } else {
            // TODO: add layer to SDL_GpuColorAttachmentDescription. if texture
            // is 3D or an array, it allows to select the layer or the face of cube
            // map to draw to, otherwise the attachment is considered layered.
            cmd.color_attachments[i] = (GLuint)(uintptr_t)color_attachments[i].texture->driverdata;
            cmd.draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
            if (color_attachments[i].color_init == SDL_GPUPASSINIT_CLEAR) {
                cmd.clear_color[i][0] = color_attachments[i].clear_red;
                cmd.clear_color[i][1] = color_attachments[i].clear_green;
                cmd.clear_color[i][2] = color_attachments[i].clear_blue;
                cmd.clear_color[i][3] = color_attachments[i].clear_alpha;
            } else if (color_attachments[i].color_init == SDL_GPUPASSINIT_UNDEFINED) {
                cmd.clear_color[i][0] = -1.f;
                cmd.invalidate_buffers[cmd.n_invalid] = GL_COLOR_ATTACHMENT0 + i;
                ++cmd.n_invalid;
            } else {
                cmd.clear_color[i][0] = -1.f;
            }
            render_target_height = SDL_min(render_target_height, color_attachments[i].texture->desc.height);
        }
    }

    if (depth_attachment) {
        cmd.depth_attachment = (GLuint)(uintptr_t)depth_attachment->texture->driverdata;
        cmd.clear_depth_value = -1.f;
        if (depth_attachment->depth_init == SDL_GPUPASSINIT_CLEAR) {
            cmd.clear_depth_value = depth_attachment->clear_depth;
        } else if (depth_attachment->depth_init == SDL_GPUPASSINIT_UNDEFINED) {
            cmd.invalidate_buffers[cmd.n_invalid] = GL_DEPTH_ATTACHMENT;
            ++cmd.n_invalid;
        }
        render_target_height = SDL_min(render_target_height, depth_attachment->texture->desc.height);
    }

    if (stencil_attachment) {
        cmd.stencil_attachment = (GLuint)(uintptr_t)stencil_attachment->texture->driverdata;
        cmd.clear_stencil = SDL_FALSE;
        if (stencil_attachment->stencil_init == SDL_GPUPASSINIT_CLEAR) {
            cmd.clear_stencil = SDL_TRUE;
            // unsigned to int conversion is implementation defined
            SDL_COMPILE_TIME_ASSERT(stencil_size_unequal, sizeof(cmd.clear_stencil_value) == sizeof(stencil_attachment->clear_stencil));
            SDL_memcpy(&cmd.clear_stencil_value, &stencil_attachment->clear_stencil, sizeof(cmd.clear_stencil_value));
        }  else if (stencil_attachment->stencil_init == SDL_GPUPASSINIT_UNDEFINED) {
            cmd.invalidate_buffers[cmd.n_invalid] = GL_STENCIL_ATTACHMENT;
            ++cmd.n_invalid;
        }
        render_target_height = SDL_min(render_target_height, stencil_attachment->texture->desc.height);
    }

    if (OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd)) < 0) {
        return -1;
    }
    OPENGL_GpuCommandBuffer *glcmdbuf = pass->cmdbuf->driverdata;
    OPENGL_GpuRenderPassData *pass_data = &glcmdbuf->encoding_state.currrent_render_pass_data;
    glcmdbuf->encoding_state.current_render_pass = pass;
    pass_data->render_targert_height = render_target_height;
    pass->driverdata = pass_data;
    return 0;
}

static int ExecStartRenderPass(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_StartRenderPass *cmd)
{
    cmdbuf->exec_state.pop_pass_label = cmd->pass_label != NULL;
    if (cmd->pass_label) {
        PushDebugGroup(gl_data, "Start Render Pass: ", cmd->pass_label);
        SDL_free(cmd->pass_label);
    }

    GLuint fbo = gl_data->fbo_render;
    gl_data->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

    // Framebuffer stores textures in GL_COLOR_ATTACHMENTi with glNamedFramebufferTexture().
    // Shader draws to framebuffer's "draw buffer": layout(location = i) out vec4 color_output;
    // Color attachments are bound to draw buffer with glNamedFramebufferDrawBuffers().
    // SDL_Gpu does not make a distinction between color attachment and draw buffer,
    // so we bind draw buffer i to GL_COLOR_ATTACHMENTi.

    // 4 operations on target texture, 4 ways of doing it:
    // - set target texture:
    //      loop over attachment (GL_COLOR_ATTACHMENTi)
    // - bind attachment to draw buffer:
    //      array of draw buffer with each value set to GL_COLOR_ATTACHMENTi or GL_NONE
    // - clear:
    //      loop over draw buffer (GL_COLOR, i_draw_buffer), need to be done after binding attachment to draw buffer
    // - invalidate:
    //      array of attachment (GL_COLOR_ATTACHMENTi)

    // clear operation are affected by scissor and color mask: disable them
    gl_data->glDisable(GL_SCISSOR_TEST);

    gl_data->glNamedFramebufferDrawBuffers(fbo, SDL_GPU_MAX_COLOR_ATTACHMENTS, cmd->draw_buffers);
    for (Uint32 i = 0; i < SDL_GPU_MAX_COLOR_ATTACHMENTS; ++i) {
        if (i >= cmd->n_color_attachments || cmd->color_attachments[i] == 0) {
            gl_data->glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0 + i, 0, 0);
        } else {
            gl_data->glColorMaski(i, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            gl_data->glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0 + i, cmd->color_attachments[i], 0);
            if (cmd->clear_color[i][0] >= 0.f) {
                gl_data->glClearNamedFramebufferfv(fbo, GL_COLOR, i, cmd->clear_color[i]);
            }
        }
        CHECK_GL_ERROR;
    }

    if (cmd->depth_attachment != 0) {
        gl_data->glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, cmd->depth_attachment, 0);
        if (cmd->clear_depth_value >= 0.f) {
            gl_data->glDepthMask(GL_TRUE); // allow writing to depth to clear it
            gl_data->glClearNamedFramebufferfv(fbo, GL_DEPTH, 0, &cmd->clear_depth_value);
        }
        CHECK_GL_ERROR;
    }

    if (cmd->stencil_attachment != 0) {
        gl_data->glNamedFramebufferTexture(fbo, GL_STENCIL_ATTACHMENT, cmd->stencil_attachment, 0);
        if (cmd->clear_stencil) {
            gl_data->glClearNamedFramebufferiv(fbo, GL_STENCIL, 0, &cmd->clear_stencil_value);
        }
        gl_data->glEnable(GL_STENCIL_TEST);
        CHECK_GL_ERROR;
    } else {
        gl_data->glDisable(GL_STENCIL_TEST);
    }

    if (cmd->n_invalid != 0) {
        gl_data->glInvalidateNamedFramebufferData(fbo, cmd->n_invalid, cmd->invalidate_buffers);
        CHECK_GL_ERROR;
    }

    gl_data->glEnable(GL_SCISSOR_TEST);

    if (!CheckFrameBuffer(gl_data, fbo, SDL_TRUE)) {
        gl_data->glDeleteFramebuffers(1, &fbo);
        return -1;
    }
    cmdbuf->exec_state.n_color_attachment = cmd->n_color_attachments;
    CHECK_GL_ERROR;
    return 0;
}

static GLenum ToGLCompareFunc(SDL_GpuCompareFunction f)
{
    switch (f) {
    case SDL_GPUCMPFUNC_NEVER       : return GL_NEVER;
    case SDL_GPUCMPFUNC_LESS        : return GL_LESS;
    case SDL_GPUCMPFUNC_EQUAL       : return GL_EQUAL;
    case SDL_GPUCMPFUNC_LESSEQUAL   : return GL_LEQUAL;
    case SDL_GPUCMPFUNC_GREATER     : return GL_GREATER;
    case SDL_GPUCMPFUNC_NOTEQUAL    : return GL_NOTEQUAL;
    case SDL_GPUCMPFUNC_GREATEREQUAL: return GL_GEQUAL;
    case SDL_GPUCMPFUNC_ALWAYS      : return GL_ALWAYS;
    }
    SDL_assert(0);
    return GL_NEVER;
}
static GLenum ToGLStencilOp(SDL_GpuStencilOperation op)
{
    switch (op) {
    case SDL_GPUSTENCILOP_KEEP          : return GL_KEEP;
    case SDL_GPUSTENCILOP_ZERO          : return GL_ZERO;
    case SDL_GPUSTENCILOP_REPLACE       : return GL_REPLACE;
    case SDL_GPUSTENCILOP_INCREMENTCLAMP: return GL_INCR;
    case SDL_GPUSTENCILOP_DECREMENTCLAMP: return GL_DECR;
    case SDL_GPUSTENCILOP_INVERT        : return GL_INVERT;
    case SDL_GPUSTENCILOP_INCREMENTWRAP : return GL_INCR_WRAP;
    case SDL_GPUSTENCILOP_DECREMENTWRAP : return GL_DECR_WRAP;
    }
    SDL_assert(0);
    return GL_ZERO;
}
static GLenum ToGLBlendMode(SDL_GpuBlendOperation b)
{
    switch (b) {
    case SDL_GPUBLENDOP_ADD            : return GL_FUNC_ADD;
    case SDL_GPUBLENDOP_SUBTRACT       : return GL_FUNC_SUBTRACT;
    case SDL_GPUBLENDOP_REVERSESUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
    case SDL_GPUBLENDOP_MIN            : return GL_MIN;
    case SDL_GPUBLENDOP_MAX            : return GL_MAX;
    }
    SDL_assert(0);
    return GL_FUNC_ADD;
}
static GLenum ToGLBlendFunction(SDL_GpuBlendFactor f)
{
    switch (f) {
    case SDL_GPUBLENDFACTOR_ZERO                    : return GL_ZERO;
    case SDL_GPUBLENDFACTOR_ONE                     : return GL_ONE;
    case SDL_GPUBLENDFACTOR_SOURCECOLOR             : return GL_SRC_COLOR;
    case SDL_GPUBLENDFACTOR_ONEMINUSSOURCECOLOR     : return GL_ONE_MINUS_SRC_COLOR;
    case SDL_GPUBLENDFACTOR_SOURCEALPHA             : return GL_SRC_ALPHA;
    case SDL_GPUBLENDFACTOR_ONEMINUSSOURCEALPHA     : return GL_ONE_MINUS_SRC_ALPHA;
    case SDL_GPUBLENDFACTOR_DESTINATIONCOLOR        : return GL_DST_COLOR;
    case SDL_GPUBLENDFACTOR_ONEMINUSDESTINATIONCOLOR: return GL_ONE_MINUS_DST_COLOR;
    case SDL_GPUBLENDFACTOR_DESTINATIONALPHA        : return GL_DST_ALPHA;
    case SDL_GPUBLENDFACTOR_ONEMINUSDESTINATIONALPHA: return GL_ONE_MINUS_DST_ALPHA;
    case SDL_GPUBLENDFACTOR_SOURCEALPHASATURATED    : return GL_SRC_ALPHA_SATURATE;
    case SDL_GPUBLENDFACTOR_BLENDCOLOR              : return GL_CONSTANT_COLOR;
    case SDL_GPUBLENDFACTOR_ONEMINUSBLENDCOLOR      : return GL_ONE_MINUS_CONSTANT_COLOR;
    case SDL_GPUBLENDFACTOR_BLENDALPHA              : return GL_CONSTANT_ALPHA;
    case SDL_GPUBLENDFACTOR_ONEMINUSBLENDALPHA      : return GL_ONE_MINUS_CONSTANT_ALPHA;
    case SDL_GPUBLENDFACTOR_SOURCE1COLOR            : return GL_SRC1_COLOR;
    case SDL_GPUBLENDFACTOR_ONEMINUSSOURCE1COLOR    : return GL_ONE_MINUS_SRC1_COLOR;
    case SDL_GPUBLENDFACTOR_SOURCE1ALPHA            : return GL_SRC1_ALPHA;
    case SDL_GPUBLENDFACTOR_ONEMINUSSOURCE1ALPHA    : return GL_ONE_MINUS_SRC1_ALPHA;
    }
    SDL_assert(0);
    return GL_ZERO;
}
static GLenum ToGLPrimitive(SDL_GpuPrimitive p)
{
    switch (p) {
    case SDL_GPUPRIM_POINT        : return GL_POINTS;
    case SDL_GPUPRIM_LINE         : return GL_LINES;
    case SDL_GPUPRIM_LINESTRIP    : return GL_LINE_STRIP;
    case SDL_GPUPRIM_TRIANGLE     : return GL_TRIANGLES;
    case SDL_GPUPRIM_TRIANGLESTRIP: return GL_TRIANGLE_STRIP;
    }
    SDL_assert(0);
    return GL_POINTS;
}

static int OPENGL_GpuSetRenderPassPipeline(SDL_GpuRenderPass *pass, SDL_GpuPipeline *pipeline)
{
    GLCMD_SetPipeline cmd;
    SDL_memset(&cmd, 0, sizeof(cmd));
    cmd.type = CMD_SET_PIPELINE;
    if (pipeline->desc.label) {
        cmd.pipeline_label = SDL_strdup(pipeline->desc.label);
    } else {
        cmd.pipeline_label = NULL;
    }

    cmd.vao = (GLuint)((uintptr_t)pipeline->driverdata >> 32);
    cmd.program = (GLuint)((uintptr_t)pipeline->driverdata & 0xFFFFFFFF);
    SDL_assert(cmd.vao != 0);
    SDL_assert(cmd.program != 0);

    OPENGL_GpuRenderPassData *pass_data = pass->driverdata;
    pass_data->primitive = ToGLPrimitive(pipeline->desc.primitive);
    pass_data->stride = pipeline->desc.vertices[0].stride;

    // TODO: what are pipeline->desc->color_attachments[i].pixel_format, depth_format
    // and stencil_format for? attachment textures already have pixel format.

    // set all pipeline states that are global states in OpenGL
    for (Uint32 i = 0; i < pipeline->desc.num_color_attachments; ++i) {
        SDL_GpuPipelineColorAttachmentDescription *desc = &pipeline->desc.color_attachments[i];

        if (desc->blending_enabled) {
            cmd.blend[i].enable = SDL_TRUE;
            cmd.blend[i].rgb_mode   = ToGLBlendMode(desc->rgb_blend_op);
            cmd.blend[i].alpha_mode = ToGLBlendMode(desc->alpha_blend_op);
            cmd.blend[i].func_rgb_src   = ToGLBlendFunction(desc->rgb_src_blend_factor);
            cmd.blend[i].func_alpha_src = ToGLBlendFunction(desc->alpha_src_blend_factor);
            cmd.blend[i].func_rgb_dst   = ToGLBlendFunction(desc->rgb_dst_blend_factor);
            cmd.blend[i].func_alpha_dst = ToGLBlendFunction(desc->alpha_dst_blend_factor);
        } else {
            cmd.blend[i].enable = SDL_FALSE;
        }

        cmd.writemask[i].red   = desc->writemask_enabled_red   ? GL_TRUE : GL_FALSE;
        cmd.writemask[i].green = desc->writemask_enabled_green ? GL_TRUE : GL_FALSE;
        cmd.writemask[i].blue  = desc->writemask_enabled_blue  ? GL_TRUE : GL_FALSE;
        cmd.writemask[i].alpha = desc->writemask_enabled_alpha ? GL_TRUE : GL_FALSE;
    }

    cmd.depth_mask = pipeline->desc.depth_write_enabled ? GL_TRUE : GL_FALSE;
    cmd.depth_func = ToGLCompareFunc(pipeline->desc.depth_function);
    //pipeline->desc.depth_clamp ? glEnable(GL_DEPTH_CLAMP) : glDisable(GL_DEPTH_CLAMP);

    cmd.depth_bias_scale = pipeline->desc.depth_bias_scale;
    cmd.depth_bias       = pipeline->desc.depth_bias;
    cmd.depth_bias_clamp = pipeline->desc.depth_bias_clamp;

    cmd.stencil_front.func       = ToGLCompareFunc(pipeline->desc.depth_stencil_front.stencil_function);
    cmd.stencil_front.ref        = pipeline->desc.depth_stencil_front.stencil_reference;
    cmd.stencil_front.read_mask  = pipeline->desc.depth_stencil_front.stencil_read_mask;
    cmd.stencil_front.write_mask = pipeline->desc.depth_stencil_front.stencil_write_mask;
    cmd.stencil_front.op_sfail   = ToGLStencilOp(pipeline->desc.depth_stencil_front.stencil_fail);
    cmd.stencil_front.op_dpfail  = ToGLStencilOp(pipeline->desc.depth_stencil_front.depth_fail);
    cmd.stencil_front.op_dppass  = ToGLStencilOp(pipeline->desc.depth_stencil_front.depth_and_stencil_pass);

    cmd.stencil_back.func       = ToGLCompareFunc(pipeline->desc.depth_stencil_back.stencil_function);
    cmd.stencil_back.ref        = pipeline->desc.depth_stencil_back.stencil_reference;
    cmd.stencil_back.read_mask  = pipeline->desc.depth_stencil_back.stencil_read_mask;
    cmd.stencil_back.write_mask = pipeline->desc.depth_stencil_back.stencil_write_mask;
    cmd.stencil_back.op_sfail   = ToGLStencilOp(pipeline->desc.depth_stencil_back.stencil_fail);
    cmd.stencil_back.op_dpfail  = ToGLStencilOp(pipeline->desc.depth_stencil_back.depth_fail);
    cmd.stencil_back.op_dppass  = ToGLStencilOp(pipeline->desc.depth_stencil_back.depth_and_stencil_pass);

    cmd.polygon_mode = (pipeline->desc.fill_mode == SDL_GPUFILL_FILL) ? GL_FILL : GL_LINE;

    if (pipeline->desc.cull_face == SDL_GPUCULLFACE_NONE) {
        cmd.enable_cull_face = SDL_FALSE;
    } else {
        cmd.enable_cull_face = SDL_TRUE;
        cmd.front_face = (pipeline->desc.front_face == SDL_GPUFRONTFACE_COUNTER_CLOCKWISE) ? GL_CCW : GL_CW;
        cmd.cull_face  = (pipeline->desc.cull_face == SDL_GPUCULLFACE_BACK) ? GL_BACK : GL_FRONT;
                   //: (pipeline->desc.cull_face == SDL_GPUCULLFACE_FRONT_AND_BACK) ? GL_FRONT_AND_BACK);
    }

    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecSetRenderPassPipeline(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_SetPipeline *cmd)
{
    if (cmdbuf->exec_state.pop_pipeline_label) {
        gl_data->glPopDebugGroup(); // pop previous pipeline
        CHECK_GL_ERROR;
    }
    cmdbuf->exec_state.pop_pipeline_label = cmd->pipeline_label != NULL;
    if (cmd->pipeline_label) {
        PushDebugGroup(gl_data, "Pipeline: ", cmd->pipeline_label);
        SDL_free(cmd->pipeline_label);
        CHECK_GL_ERROR;
    }

    gl_data->glBindVertexArray(cmd->vao);
    gl_data->glUseProgram(cmd->program);
    CHECK_GL_ERROR;

    // set all pipeline states that are global states in OpenGL
    for (Uint32 i = 0; i < cmdbuf->exec_state.n_color_attachment; ++i) {
        if (cmd->blend[i].enable) {
            gl_data->glEnablei(GL_BLEND, i);
            gl_data->glBlendEquationSeparatei(i, cmd->blend[i].rgb_mode, cmd->blend[i].alpha_mode);
            gl_data->glBlendFuncSeparatei(i,
                                          cmd->blend[i].func_rgb_src,
                                          cmd->blend[i].func_alpha_src,
                                          cmd->blend[i].func_rgb_dst,
                                          cmd->blend[i].func_alpha_dst);
        } else {
            gl_data->glDisablei(GL_BLEND, i);
        }
        gl_data->glColorMaski(i,
                              cmd->writemask[i].red,
                              cmd->writemask[i].green,
                              cmd->writemask[i].blue ,
                              cmd->writemask[i].alpha);
        CHECK_GL_ERROR;
    }

    gl_data->glDepthMask(cmd->depth_mask);
    gl_data->glDepthFunc(cmd->depth_func);
    //cmd->depth_clamp ? glEnable(GL_DEPTH_CLAMP) : glDisable(GL_DEPTH_CLAMP);

    // order is reversed compared to Metal:
    // depth_bias_scale = slope scale (Metal) = DZ (OpenGL)
    // depth_bias       = depth bias (Metal)  = r (OpenGL)
    gl_data->glPolygonOffsetClamp(cmd->depth_bias_scale, cmd->depth_bias, cmd->depth_bias_clamp);

    gl_data->glStencilFuncSeparate(GL_FRONT, cmd->stencil_front.func, cmd->stencil_front.ref, cmd->stencil_front.read_mask);
    gl_data->glStencilMaskSeparate(GL_FRONT, cmd->stencil_front.write_mask);
    gl_data->glStencilOpSeparate(GL_FRONT,
                                 cmd->stencil_front.op_sfail,
                                 cmd->stencil_front.op_dpfail,
                                 cmd->stencil_front.op_dppass);

    gl_data->glStencilFuncSeparate(GL_BACK, cmd->stencil_back.func, cmd->stencil_back.ref, cmd->stencil_back.read_mask);
    gl_data->glStencilMaskSeparate(GL_BACK, cmd->stencil_back.write_mask);
    gl_data->glStencilOpSeparate(GL_BACK,
                                 cmd->stencil_back.op_sfail,
                                 cmd->stencil_back.op_dpfail,
                                 cmd->stencil_back.op_dppass);

    gl_data->glPolygonMode(GL_FRONT_AND_BACK, cmd->polygon_mode);

    if (cmd->enable_cull_face) {
        gl_data->glEnable(GL_CULL_FACE);
        gl_data->glFrontFace(cmd->front_face);
        gl_data->glCullFace(cmd->cull_face);
    } else {
        gl_data->glDisable(GL_CULL_FACE);
    }
    CHECK_GL_ERROR;
}

static int OPENGL_GpuSetRenderPassViewport(SDL_GpuRenderPass *pass, double x, double y, double width, double height, double znear, double zfar)
{
    OPENGL_GpuRenderPassData *pass_data = pass->driverdata;
    GLint render_target_height = pass_data->render_targert_height;
    GLCMD_SetViewport cmd;
    cmd.type = CMD_SET_VIEWPORT;
    cmd.x = x;
    cmd.y = render_target_height - y - height;
    cmd.w = width;
    cmd.h = height;
    cmd.near = znear;
    cmd.far = zfar;
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecSetViewport(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_SetViewport *cmd)
{
    gl_data->glViewportIndexedf(0, cmd->x, cmd->y, cmd->w, cmd->h);
    gl_data->glDepthRangeIndexed(0, cmd->near, cmd->far);
    CHECK_GL_ERROR;
}

static int OPENGL_GpuSetRenderPassScissor(SDL_GpuRenderPass *pass, Uint32 x, Uint32 y, Uint32 width, Uint32 height)
{
    OPENGL_GpuRenderPassData *pass_data = pass->driverdata;
    GLsizei render_target_height = pass_data->render_targert_height;
    GLCMD_SetScissor cmd;
    cmd.type = CMD_SET_SCISSOR;
    cmd.x = x;
    cmd.y = render_target_height - y - height;
    cmd.w = width;
    cmd.h = height;
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecSetScissor(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_SetScissor *cmd)
{
    gl_data->glScissor(cmd->x, cmd->y, cmd->w, cmd->h);
    CHECK_GL_ERROR;
}

static int OPENGL_GpuSetRenderPassBlendConstant(SDL_GpuRenderPass *pass, double red, double green, double blue, double alpha)
{
    GLCMD_SetBlendConstant cmd;
    cmd.type = CMD_SET_BLEND_CONSTANT;
    cmd.red = red;
    cmd.green = green;
    cmd.blue = blue;
    cmd.alpha = alpha;
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecSetBlendConstant(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_SetBlendConstant *cmd)
{
    gl_data->glBlendColor(cmd->red, cmd->green, cmd->blue, cmd->alpha);
    CHECK_GL_ERROR;
}

static int OPENGL_GpuSetRenderPassVertexBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 index)
{
    GLCMD_SetBuffer cmd;
    cmd.type = CMD_SET_BUFFER;
    cmd.index = index;
    cmd.buffer = (GLuint)(uintptr_t)buffer->driverdata;
    cmd.offset = offset;
    cmd.size = buffer->buflen - offset; // TODO: add a size parameter to GpuSetRenderPassVertexBuffer()
    SDL_assert(cmd.buffer != 0);
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecSetBuffer(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_SetBuffer *cmd)
{
    // shader:
    // layout(std430, binding = 0) buffer Name {
    //     int data[];
    // };
    gl_data->glBindBufferRange(GL_SHADER_STORAGE_BUFFER, cmd->index, cmd->buffer, cmd->offset, cmd->size);
    CHECK_GL_ERROR;
}

static int OPENGL_GpuSetRenderPassVertexSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, Uint32 index)
{
    GLCMD_SetSampler cmd;
    cmd.type = CMD_SET_SAMPLER;
    cmd.unit = index;
    cmd.sampler = (GLuint)(uintptr_t)sampler->driverdata;
    SDL_assert(cmd.sampler != 0);
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecSetSampler(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_SetSampler *cmd)
{
    gl_data->glBindSampler(cmd->unit, cmd->sampler);
    CHECK_GL_ERROR;
}

static int OPENGL_GpuSetRenderPassVertexTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, Uint32 index)
{
    GLCMD_SetTexture cmd;
    cmd.type = CMD_SET_TEXTURE;
    cmd.unit = index;
    cmd.texture = (GLuint)(uintptr_t)texture->driverdata;
    SDL_assert(cmd.texture != 0);
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecSetTexture(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_SetTexture *cmd)
{
    gl_data->glBindTextureUnit(cmd->unit, cmd->texture); // take an integer index, not an GL_TEXTURE* enum
    CHECK_GL_ERROR;
}

static int OPENGL_GpuSetMeshBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 index)
{
    OPENGL_GpuRenderPassData *pass_data = pass->driverdata;
    // vao is bound in StartRenderPass
    GLCMD_SetMesh cmd;
    cmd.type = CMD_SET_MESH;
    cmd.index = index;
    cmd.buffer = (GLuint)(uintptr_t)buffer->driverdata;
    cmd.offset = offset;
    cmd.stride = pass_data->stride;
    SDL_assert(cmd.buffer != 0);
    SDL_assert(cmd.stride != 0);
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecSetMesh(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_SetMesh *cmd)
{
    gl_data->glBindVertexBuffer(cmd->index, cmd->buffer, cmd->offset, cmd->stride);
    CHECK_GL_ERROR;
}

static int OPENGL_GpuSetRenderPassFragmentBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 index)
{
    return OPENGL_GpuSetRenderPassVertexBuffer(pass, buffer, offset, index);
}

static int OPENGL_GpuSetRenderPassFragmentSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, Uint32 index)
{
    return OPENGL_GpuSetRenderPassVertexSampler(pass, sampler, index);
}

static int OPENGL_GpuSetRenderPassFragmentTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, Uint32 index)
{
    return OPENGL_GpuSetRenderPassVertexTexture(pass, texture, index);
}

static int OPENGL_GpuDraw(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count)
{
    OPENGL_GpuRenderPassData *pass_data = pass->driverdata;
    GLCMD_Draw cmd;
    cmd.type = CMD_DRAW;
    cmd.mode = pass_data->primitive;
    cmd.first = vertex_start;
    cmd.count = vertex_count;
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecDraw(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_Draw *cmd)
{
    gl_data->glDrawArrays(cmd->mode, cmd->first, cmd->count);
    CHECK_GL_ERROR;
}

static GLenum ToGLindexType(SDL_GpuIndexType t)
{
    switch (t) {
    case SDL_GPUINDEXTYPE_UINT16: return GL_UNSIGNED_SHORT;
    case SDL_GPUINDEXTYPE_UINT32: return GL_UNSIGNED_INT;
    }
    return 0;
}
static int OPENGL_GpuDrawIndexed(SDL_GpuRenderPass *pass,
                                 Uint32 index_count, SDL_GpuIndexType index_type,
                                 SDL_GpuBuffer *index_buffer, Uint32 index_offset)
{
    OPENGL_GpuRenderPassData *pass_data = pass->driverdata;
    // vao is bound in StartRenderPass
    GLCMD_DrawIndexed cmd;
    cmd.type = CMD_DRAW_INDEXED;
    cmd.index_buffer = (GLuint)(uintptr_t)index_buffer->driverdata;
    cmd.mode = pass_data->primitive;
    cmd.count = index_count;
    cmd.index_type = ToGLindexType(index_type);
    cmd.indices = (void*)(uintptr_t)index_offset;
    SDL_assert(cmd.index_buffer != 0);
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecDrawIndexed(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_DrawIndexed *cmd)
{
    gl_data->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cmd->index_buffer);
    gl_data->glDrawElements(cmd->mode, cmd->count, cmd->index_type, cmd->indices);
    CHECK_GL_ERROR;
}

static int OPENGL_GpuDrawInstanced(SDL_GpuRenderPass *pass,
                                   Uint32 vertex_start, Uint32 vertex_count,
                                   Uint32 instance_count, Uint32 base_instance)
{
    // OGL_GpuDevice *gl_data = pass->device->driverdata;
    // TODO: OPENGL_GpuDrawInstanced()
    return 0;
}

static void ExecDrawInstanced(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_DrawInstanced *cmd)
{

}

static int OPENGL_GpuDrawInstancedIndexed(SDL_GpuRenderPass *pass, Uint32 index_count,
                                          SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer,
                                          Uint32 index_offset, Uint32 instance_count,
                                          Uint32 base_vertex, Uint32 base_instance)
{
    // OGL_GpuDevice *gl_data = pass->device->driverdata;
    // TODO: OPENGL_GpuDrawInstancedIndexed()
    return 0;
}

static void ExecDrawInstancedIndexed(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_DrawInstancedIndexed *cmd)
{

}

static int OPENGL_GpuEndRenderPass(SDL_GpuRenderPass *pass)
{
    OPENGL_GpuCommandBuffer *glcmdbuf = pass->cmdbuf->driverdata;
    glcmdbuf->encoding_state.current_render_pass = NULL;
    GLCMD_EndRenderPass cmd;
    cmd.type = CMD_END_RENDER_PASS;
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecEndRenderPass(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_EndRenderPass *cmd)
{
    if (cmdbuf->exec_state.pop_pipeline_label) {
        gl_data->glPopDebugGroup(); // pop previous pipeline
        cmdbuf->exec_state.pop_pipeline_label = SDL_FALSE;
    }
    if (cmdbuf->exec_state.pop_pass_label) {
        gl_data->glPopDebugGroup(); // pop render pass
        cmdbuf->exec_state.pop_pass_label = SDL_FALSE;
    }
    CHECK_GL_ERROR;
    cmdbuf->exec_state.n_color_attachment = 0;
}

static int OPENGL_GpuStartBlitPass(SDL_GpuBlitPass *pass)
{
    GLCMD_StartBlitPass cmd;
    cmd.type = CMD_START_BLIT_PASS;
    if (pass->label) {
        cmd.pass_label = SDL_strdup(pass->label);
    } else {
        cmd.pass_label = NULL;
    }
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecStartBlitPass(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_StartBlitPass *cmd)
{
    cmdbuf->exec_state.pop_pass_label = cmd->pass_label != NULL;
    if (cmd->pass_label) {
        PushDebugGroup(gl_data, "Start blit Pass: ", cmd->pass_label);
    }
    CHECK_GL_ERROR;
}

static int OPENGL_GpuCopyBetweenTextures(SDL_GpuBlitPass *pass,
                                         SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel,
                                         Uint32 srcx, Uint32 srcy, Uint32 srcz,
                                         Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                                         SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel,
                                         Uint32 dstx, Uint32 dsty, Uint32 dstz)
{
    // FIXME: check that internal formats are compatible.

    // TODO: texture description has 'depth_or_slice', can we get the same for GpuCopyBetweenTextures()?
    // There is no GL_TEXTURE3D_ARRAY in OpenGL so 'depth_or_slice' go to 'z' argument.
    if (GetTextureDimension(srctex->desc.texture_type) == 3) {
        if (srctex->desc.texture_type != SDL_GPUTEXTYPE_3D) {
            srcz = srcslice;
        }
    }
    if (GetTextureDimension(dsttex->desc.texture_type) == 3) {
        if (dsttex->desc.texture_type != SDL_GPUTEXTYPE_3D) {
            dstz = dstslice;
        }
    }
    GLCMD_CopyTexture cmd;
    cmd.type = CMD_COPY_TEXTURE;
    cmd.src = (GLuint)(uintptr_t)srctex->driverdata;
    cmd.src_target = ToGLTextureTarget(srctex->desc.texture_type);
    cmd.src_level = srclevel;
    cmd.src_x = srcx;
    cmd.src_y = srcy;
    cmd.src_z = srcz;
    cmd.dst = (GLuint)(uintptr_t)dsttex->driverdata;
    cmd.dst_target = ToGLTextureTarget(dsttex->desc.texture_type);
    cmd.dst_level = dstlevel;
    cmd.dst_x = dstx;
    cmd.dst_y = dsty;
    cmd.dst_z = dstz;
    cmd.src_w = srcw;
    cmd.src_h = srch;
    cmd.src_d = srcdepth;
    SDL_assert(cmd.src != 0);
    SDL_assert(cmd.dst != 0);
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecCopyTexture(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_CopyTexture *cmd)
{
    gl_data->glCopyImageSubData(cmd->src, cmd->src_target, cmd->src_level, cmd->src_x, cmd->src_y, cmd->src_z,
                                cmd->dst, cmd->dst_target, cmd->dst_level, cmd->dst_x, cmd->dst_y, cmd->dst_z,
                                cmd->src_w, cmd->src_h, cmd->src_d);
    CHECK_GL_ERROR;
}

static int OPENGL_GpuFillBuffer(SDL_GpuBlitPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 length, Uint8 value)
{
    GLCMD_FillBuffer cmd;
    cmd.type = CMD_FILL_BUFFER;
    cmd.buffer = (GLuint)(uintptr_t)buffer->driverdata;
    cmd.offset = offset;
    cmd.size = length;
    cmd.value = value;
    SDL_assert(cmd.buffer != 0);
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecFillBuffer(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_FillBuffer *cmd)
{
    gl_data->glClearNamedBufferSubData(cmd->buffer, GL_R8, cmd->offset, cmd->size, GL_RED, GL_UNSIGNED_BYTE, &cmd->value);
    CHECK_GL_ERROR;
}

static int OPENGL_GpuGenerateMipmaps(SDL_GpuBlitPass *pass, SDL_GpuTexture *texture)
{
    GLCMD_GenerateMipmaps cmd;
    cmd.type = CMD_GENERATE_MIPMAP;
    cmd.texture = (GLuint)(uintptr_t)texture->driverdata;
    SDL_assert(cmd.texture != 0);
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecGenerateMipmaps(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_GenerateMipmaps *cmd)
{
    gl_data->glGenerateTextureMipmap(cmd->texture);
    CHECK_GL_ERROR;
}

static int CopyBuffer(SDL_GpuBlitPass *pass,
                      GLuint srcbuf, Uint32 srcoffset,
                      GLuint dstbuf, Uint32 dstoffset, Uint32 length)
{
    GLCMD_CopyBuffer cmd;
    cmd.type = CMD_COPY_BUFFER;
    cmd.src = srcbuf;
    cmd.dst = dstbuf;
    cmd.src_offset = srcoffset;
    cmd.dst_offset = dstoffset;
    cmd.size = length;
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static int OPENGL_GpuCopyBufferCpuToGpu(SDL_GpuBlitPass *pass,
                                        SDL_CpuBuffer *srcbuf, Uint32 srcoffset,
                                        SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length)
{
    SDL_assert((GLuint)(uintptr_t)srcbuf->driverdata != 0);
    SDL_assert((GLuint)(uintptr_t)dstbuf->driverdata != 0);
    return CopyBuffer(pass,
                      (GLuint)(uintptr_t)srcbuf->driverdata, srcoffset,
                      (GLuint)(uintptr_t)dstbuf->driverdata, dstoffset, length);
}

static int OPENGL_GpuCopyBufferGpuToCpu(SDL_GpuBlitPass *pass,
                                        SDL_GpuBuffer *srcbuf, Uint32 srcoffset,
                                        SDL_CpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length)
{
    SDL_assert((GLuint)(uintptr_t)srcbuf->driverdata != 0);
    SDL_assert((GLuint)(uintptr_t)dstbuf->driverdata != 0);
    return CopyBuffer(pass,
                      (GLuint)(uintptr_t)srcbuf->driverdata, srcoffset,
                      (GLuint)(uintptr_t)dstbuf->driverdata, dstoffset, length);
}

static int OPENGL_GpuCopyBufferGpuToGpu(SDL_GpuBlitPass *pass,
                                        SDL_GpuBuffer *srcbuf, Uint32 srcoffset,
                                        SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length)
{
    SDL_assert((GLuint)(uintptr_t)srcbuf->driverdata != 0);
    SDL_assert((GLuint)(uintptr_t)dstbuf->driverdata != 0);
    return CopyBuffer(pass,
                      (GLuint)(uintptr_t)srcbuf->driverdata, srcoffset,
                      (GLuint)(uintptr_t)dstbuf->driverdata, dstoffset, length);
}

static void ExecCopyBuffer(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_CopyBuffer *cmd)
{
    gl_data->glCopyNamedBufferSubData(cmd->src, cmd->dst, cmd->src_offset, cmd->dst_offset, cmd->size);
    CHECK_GL_ERROR;
}

static GLenum ToGLDataFormat(SDL_GpuPixelFormat data_format)
{
    switch (data_format) {
    case SDL_GPUPIXELFMT_B5G6R5          : return GL_RGB;
    case SDL_GPUPIXELFMT_BGR5A1          : return GL_RGBA;
    case SDL_GPUPIXELFMT_RGBA8           : return GL_RGBA;
    case SDL_GPUPIXELFMT_RGBA8_sRGB      : return GL_RGBA;
    case SDL_GPUPIXELFMT_BGRA8           : return GL_BGRA;
    case SDL_GPUPIXELFMT_BGRA8_sRGB      : return GL_BGRA;
    case SDL_GPUPIXELFMT_Depth24_Stencil8: return GL_DEPTH_STENCIL; // FIXME: this is not listed as valid type for glTextureSubImage2D
    case SDL_GPUPIXELFMT_INVALID         : return 0;
    }
    SDL_assert(0);
    return 0;
}
static GLenum ToGLDataType(SDL_GpuPixelFormat data_format)
{
    switch (data_format) {
    case SDL_GPUPIXELFMT_B5G6R5          : return GL_UNSIGNED_SHORT_5_6_5;
    case SDL_GPUPIXELFMT_BGR5A1          : return GL_UNSIGNED_SHORT_5_5_5_1; // GL_UNSIGNED_SHORT_1_5_5_5_REV
    case SDL_GPUPIXELFMT_RGBA8           :
    case SDL_GPUPIXELFMT_RGBA8_sRGB      :
    case SDL_GPUPIXELFMT_BGRA8           :
    case SDL_GPUPIXELFMT_BGRA8_sRGB      : return GL_UNSIGNED_INT_8_8_8_8;
    case SDL_GPUPIXELFMT_Depth24_Stencil8: return GL_UNSIGNED_INT_24_8; // FIXME: this is not listed as valid type for glTextureSubImage2D
    case SDL_GPUPIXELFMT_INVALID         : return 0;
    }
    SDL_assert(0);
    return 0;
}
static int OPENGL_GpuCopyFromBufferToTexture(SDL_GpuBlitPass *pass,
                                             SDL_GpuBuffer *srcbuf, Uint32 srcoffset,
                                                                    Uint32 srcpitch, Uint32 srcimgpitch,
                                                                    Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                                             SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel,
                                                                     Uint32 dstx, Uint32 dsty, Uint32 dstz)
{
    GLCMD_CopyFromBufferToTexture cmd;
    cmd.type = CMD_COPY_BUFFER_TO_TEXTURE_1D + GetTextureDimension(dsttex->desc.texture_type) - 1;
    cmd.buffer = (GLuint)(uintptr_t)srcbuf->driverdata;
    cmd.texture = (GLuint)(uintptr_t)dsttex->driverdata;
    cmd.level = dstlevel;
    cmd.dst_x = dstx;
    cmd.dst_y = dsty;
    cmd.dst_z = dstz;
    cmd.dst_w = srcw;
    cmd.dst_h = srch;
    cmd.dst_d = srcdepth;
    cmd.data_format = ToGLDataFormat(dsttex->desc.pixel_format);
    cmd.data_type = ToGLDataType(dsttex->desc.pixel_format);
    cmd.src_offset = srcoffset;
    cmd.src_pitch = srcpitch;
    cmd.src_imgpitch = srcimgpitch;
    SDL_assert((GLuint)(uintptr_t)srcbuf->driverdata != 0);
    SDL_assert((GLuint)(uintptr_t)dsttex->driverdata != 0);
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecCopyBufferToTexture1D(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_CopyFromBufferToTexture *cmd)
{
    gl_data->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, cmd->buffer);
    gl_data->glTextureSubImage1D(cmd->texture, cmd->level, cmd->dst_x, cmd->dst_w, cmd->data_format, cmd->data_type, (void*)(uintptr_t)cmd->src_offset);
    gl_data->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    CHECK_GL_ERROR;
}

static void ExecCopyBufferToTexture2D(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_CopyFromBufferToTexture *cmd)
{
    gl_data->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, cmd->buffer);
    if (cmd->src_pitch % 4 == 0) { // default unpack alignment is 4
        gl_data->glTextureSubImage2D(cmd->texture, cmd->level, cmd->dst_x, cmd->dst_y,
                                     cmd->dst_w, cmd->dst_h,
                                     cmd->data_format, cmd->data_type,
                                     (void*)(uintptr_t)cmd->src_offset);
    } else {
        for (GLsizei i = 0; i < cmd->dst_h; ++i) {
            gl_data->glTextureSubImage2D(cmd->texture, cmd->level, cmd->dst_x, cmd->dst_y+i,
                                         cmd->dst_w, 1,
                                         cmd->data_format, cmd->data_type,
                                         (void*)(uintptr_t)(cmd->src_offset + i*cmd->src_pitch));
        }
    }
    gl_data->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    CHECK_GL_ERROR;
}

static void ExecCopyBufferToTexture3D(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_CopyFromBufferToTexture *cmd)
{
    // TODO: OPENGL_GpuCopyFromBufferToTexture() for 3d texture
}

static int OPENGL_GpuCopyFromTextureToBuffer(SDL_GpuBlitPass *pass,
                                             SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel,
                                                                     Uint32 srcx, Uint32 srcy, Uint32 srcz,
                                                                     Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                                             SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 dstpitch, Uint32 dstimgpitch)
{
    // OGL_GpuDevice *gl_data = dstbuf->device->driverdata;
    // GLuint dstglid = (GLuint)(uintptr_t)dstbuf->driverdata;
    // GLuint srcglid = (GLuint)(uintptr_t)srctex->driverdata;
    // SDL_assert(srcglid != 0);
    // SDL_assert(dstglid != 0);

    // TODO: implement: OPENGL_GpuCopyFromTextureToBuffer()
    // GLuint format = 0;
    // GLuint type = 0;
    // glBindBuffer(GL_PIXEL_PACK_BUFFER, dstglid);
    // glGetTextureImage(srcglid, srclevel, format, type, (void*)(uintptr_t)dstoffset);
    // glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    return SDL_Unsupported();
}

static void ExecCopyTextureToBuffer1D(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_CopyFromTextureToBuffer *cmd)
{

}

static void ExecCopyTextureToBuffer2D(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_CopyFromTextureToBuffer *cmd)
{

}

static void ExecCopyTextureToBuffer3D(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_CopyFromTextureToBuffer *cmd)
{

}

static int OPENGL_GpuEndBlitPass(SDL_GpuBlitPass *pass)
{
    GLCMD_EndBlitPass cmd;
    cmd.type = CMD_END_BLIT_PASS;
    return OPENGL_PushCommand(pass->cmdbuf, &cmd, sizeof(cmd));
}

static void ExecEndBlitPass(OGL_GpuDevice *gl_data, OPENGL_GpuCommandBuffer *cmdbuf, GLCMD_EndBlitPass *cmd)
{
    if (cmdbuf->exec_state.pop_pass_label) {
        gl_data->glPopDebugGroup();
        cmdbuf->exec_state.pop_pass_label = SDL_FALSE;
    }
    CHECK_GL_ERROR;
}

static int OPENGL_GpuSubmitCommandBuffer(SDL_GpuCommandBuffer *cmdbuf, SDL_GpuFence *fence)
{
    OGL_GpuDevice *gl_data = cmdbuf->device->driverdata;
    OPENGL_GpuCommandBuffer *glcmdbuf = cmdbuf->driverdata;

    SDL_assert(!glcmdbuf->encoding_state.current_render_pass);

    GLCMD_TYPE end = CMD_NONE;
    if (OPENGL_PushCommand(cmdbuf, &end, sizeof(end)) != 0) {
        return -1;
    }

    size_t i = 0;
    while (1) {
        GLCMD_TYPE cmd_type;
        SDL_memcpy(&cmd_type, &glcmdbuf->cmd[i], sizeof(cmd_type));
        SDL_assert(cmd_type >= CMD_NONE);
        SDL_assert(cmd_type < CMD_COUNT);
        SDL_assert(i <= glcmdbuf->n_cmd);
        switch (cmd_type) {
        case CMD_COUNT:
        case CMD_NONE:
            goto cmd_end;
        case CMD_START_RENDER_PASS: {
            GLCMD_StartRenderPass cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            if (ExecStartRenderPass(gl_data, glcmdbuf, &cmd) != 0) {
                // TODO: what to do if start render pass fail? skip pass? terminate command buffer?
                // FIXME: remaining debug label are leaked
                goto cmd_end;
            }
            break;
        } case CMD_SET_PIPELINE: {
            GLCMD_SetPipeline cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecSetRenderPassPipeline(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_SET_VIEWPORT: {
            GLCMD_SetViewport cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecSetViewport(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_SET_SCISSOR: {
            GLCMD_SetScissor cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecSetScissor(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_SET_BLEND_CONSTANT: {
            GLCMD_SetBlendConstant cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecSetBlendConstant(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_SET_BUFFER: {
            GLCMD_SetBuffer cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecSetBuffer(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_SET_SAMPLER: {
            GLCMD_SetSampler cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecSetSampler(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_SET_TEXTURE: {
            GLCMD_SetTexture cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecSetTexture(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_SET_MESH: {
            GLCMD_SetMesh cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecSetMesh(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_DRAW: {
            GLCMD_Draw cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecDraw(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_DRAW_INDEXED: {
            GLCMD_DrawIndexed cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecDrawIndexed(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_DRAW_INSTANCED: {
            GLCMD_DrawInstanced cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecDrawInstanced(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_DRAW_INSTANCED_INDEXED: {
            GLCMD_DrawInstancedIndexed cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecDrawInstancedIndexed(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_END_RENDER_PASS: {
            GLCMD_EndRenderPass cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecEndRenderPass(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_START_BLIT_PASS: {
            GLCMD_StartBlitPass cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecStartBlitPass(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_FILL_BUFFER: {
            GLCMD_FillBuffer cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecFillBuffer(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_GENERATE_MIPMAP: {
            GLCMD_GenerateMipmaps cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecGenerateMipmaps(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_COPY_TEXTURE: {
            GLCMD_CopyTexture cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecCopyTexture(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_COPY_BUFFER: {
            GLCMD_CopyBuffer cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecCopyBuffer(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_COPY_BUFFER_TO_TEXTURE_1D: {
            GLCMD_CopyFromBufferToTexture cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecCopyBufferToTexture1D(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_COPY_BUFFER_TO_TEXTURE_2D: {
            GLCMD_CopyFromBufferToTexture cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecCopyBufferToTexture2D(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_COPY_BUFFER_TO_TEXTURE_3D: {
            GLCMD_CopyFromBufferToTexture cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecCopyBufferToTexture3D(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_COPY_TEXTURE_TO_BUFFER_1D: {
            GLCMD_CopyFromTextureToBuffer cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecCopyTextureToBuffer1D(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_COPY_TEXTURE_TO_BUFFER_2D: {
            GLCMD_CopyFromTextureToBuffer cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecCopyTextureToBuffer2D(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_COPY_TEXTURE_TO_BUFFER_3D: {
            GLCMD_CopyFromTextureToBuffer cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecCopyTextureToBuffer3D(gl_data, glcmdbuf, &cmd);
            break;
        } case CMD_END_BLIT_PASS: {
            GLCMD_EndBlitPass cmd;
            SDL_memcpy(&cmd, &glcmdbuf->cmd[i], sizeof(cmd));
            i += sizeof(cmd);
            ExecEndBlitPass(gl_data, glcmdbuf, &cmd);
            break;
        }
        }
    }
    cmd_end:
    SDL_free(glcmdbuf);
    return 0;
}

static void OPENGL_GpuAbandonCommandBuffer(SDL_GpuCommandBuffer *buffer)
{
    // FIXME: debug label are leaked
    SDL_free(buffer->driverdata);
}

static int OPENGL_GpuGetBackbuffer(SDL_GpuDevice *device, SDL_Window *window, SDL_GpuTexture *texture)
{
    OGL_GpuDevice *gl_data = device->driverdata;
    SDL_assert(window != gl_data->window); // SDL_GetGpuBackbuffer() calls ClaimWindow()

    if (SDL_AtomicCAS(&gl_data->window_size_changed, 1, 0)) {
        if (!RecreateBackBufferTexture(device)) {
            SDL_AtomicSet(&gl_data->window_size_changed, 1); // retry next time
            return -1;
        }
    }
    texture->desc.pixel_format = gl_data->texture_backbuffer_format;
    texture->desc.height = gl_data->h_backbuffer;
    texture->desc.width = gl_data->w_backbuffer;
    texture->driverdata = (void*)(uintptr_t)gl_data->texture_backbuffer;
    return 0;
}

static int OPENGL_GpuPresent(SDL_GpuDevice *device, SDL_Window *window, SDL_GpuTexture *backbuffer, int swapinterval)
{
    OGL_GpuDevice *gl_data = device->driverdata;
    GLuint tex_glid = (GLuint)(uintptr_t)backbuffer->driverdata;
    SDL_assert(tex_glid == gl_data->texture_backbuffer);
    SDL_assert(window != gl_data->window); // checked by SDL_GpuPresent()
    if (device->label) {
        PushDebugGroup(gl_data, "Present device: ", device->label);
    }
    CHECK_GL_ERROR;

    if (swapinterval != gl_data->swap_interval) {
        if (SDL_GL_SetSwapInterval(swapinterval) < 1) {
            if (swapinterval == -1) {
                SDL_GL_SetSwapInterval(1);
            }
        }
    }
    // store swap interval even if it fails, don't retry every frame
    gl_data->swap_interval = swapinterval;

    gl_data->glViewportIndexedf(0, 0.f, 0.f, gl_data->w_backbuffer, gl_data->h_backbuffer);
    gl_data->glDisable(GL_SCISSOR_TEST); // blit operation are affected by scissor
    gl_data->glBlitNamedFramebuffer(gl_data->fbo_backbuffer, 0,
                                    0, 0, gl_data->w_backbuffer, gl_data->h_backbuffer,
                                    0, 0, gl_data->w_backbuffer, gl_data->h_backbuffer,
                                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
    CHECK_GL_ERROR;
    gl_data->glEnable(GL_SCISSOR_TEST);
    int r = SDL_GL_SwapWindow(window);
    if (device->label) {
        gl_data->glPopDebugGroup();
    }
    CHECK_GL_ERROR;
    return r;
}

static int OPENGL_GpuCreateFence(SDL_GpuFence *fence)
{
    // OGL_GpuDevice *gl_data = buffer->device->driverdata;
    return 0;
}

static void OPENGL_GpuDestroyFence(SDL_GpuFence *fence)
{
    // OGL_GpuDevice *gl_data = buffer->device->driverdata;
}

static int OPENGL_GpuQueryFence(SDL_GpuFence *fence)
{
    // OGL_GpuDevice *gl_data = buffer->device->driverdata;
    return 1;
}

static int OPENGL_GpuResetFence(SDL_GpuFence *fence)
{
    // OGL_GpuDevice *gl_data = buffer->device->driverdata;
    return 0;
}

static int OPENGL_GpuWaitFence(SDL_GpuFence *fence)
{
    // OGL_GpuDevice *gl_data = buffer->device->driverdata;
    return 0;
}

static int WindowEventWatch(void* user_data, SDL_Event* e)
{
    OGL_GpuDevice* gl_data = user_data;
    if (e->type == SDL_EVENT_WINDOW_RESIZED || e->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        SDL_AtomicSet(&gl_data->window_size_changed, 1);
    }
    return 1;
}

static int OPENGL_GpuCreateDevice(SDL_GpuDevice *device)
{
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    // 4.6 required for polygon offset clamp (and anisotropy)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifndef NDEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG
                                            // | SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG
    );
#endif
    SDL_Window *dummy_window = SDL_CreateWindow("dummy_opengl_window", 256, 256,
                                                SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);

    device->driverdata = SDL_calloc(1, sizeof(OGL_GpuDevice));
    if (!device->driverdata) {
        goto error;
    }

    OGL_GpuDevice* gl_data = device->driverdata;
    gl_data->window = dummy_window;
    gl_data->dummy_window = SDL_TRUE;

    gl_data->context = SDL_GL_CreateContext(dummy_window);
    if (!gl_data->context) {
        goto error;
    }
    SDL_GL_MakeCurrent(dummy_window, gl_data->context);

    gl_data->debug = SDL_TRUE; // TODO: SDL_GpuCreateDevice should take a flags parameter

    // TODO: maybe reset GL attributes

    SDL_AddEventWatch(WindowEventWatch, gl_data);
#ifndef GL_GLEXT_PROTOTYPES
  #define GL_FN(T, N) gl_data->N = (T)SDL_GL_GetProcAddress(#N);
    OPENGL_FUNCTION_X
  #undef GL_FN
  #define GL_FN(T, N) if (!gl_data->N) {goto error;}
    OPENGL_FUNCTION_X
  #undef GL_FN
    PFNGLGETSTRINGPROC   glGetString   = (PFNGLGETSTRINGPROC)  SDL_GL_GetProcAddress("glGetString");
    PFNGLGETINTEGERVPROC glGetIntegerv = (PFNGLGETINTEGERVPROC)SDL_GL_GetProcAddress("glGetIntegerv");
    PFNGLCLIPCONTROLPROC glClipControl = (PFNGLCLIPCONTROLPROC)SDL_GL_GetProcAddress("glClipControl");
    PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC glNamedFramebufferReadBuffer =
        (PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC)SDL_GL_GetProcAddress("glNamedFramebufferReadBuffer");
    PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)SDL_GL_GetProcAddress("glDebugMessageCallback");
    PFNGLDEBUGMESSAGECONTROLPROC  glDebugMessageControl  = (PFNGLDEBUGMESSAGECONTROLPROC) SDL_GL_GetProcAddress("glDebugMessageControl");
    PFNGLCREATEFRAMEBUFFERSPROC glCreateFramebuffers = (PFNGLCREATEFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glCreateFramebuffers");
    CHECK_GL_ERROR;
#endif
    int major = 0;
    int minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    SDL_bool v46 = major > 4 || (major == 4 && minor >= 6);
    if (!v46) {
        SDL_SetError("Could not create gpu device: opengl version %d.%d < 4.6", major, minor);
        goto error;
    }

    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* glsl_version = glGetString(GL_SHADING_LANGUAGE_VERSION);
    CHECK_GL_ERROR;

    SDL_LogDebug(SDL_LOG_CATEGORY_RESERVED1, "%s: %s"   , "Vendor"      , vendor);
    SDL_LogDebug(SDL_LOG_CATEGORY_RESERVED1, "%s: %s"   , "Renderer"    , renderer);
    SDL_LogDebug(SDL_LOG_CATEGORY_RESERVED1, "%s: %d.%d", "Version"     , major, minor);
    SDL_LogDebug(SDL_LOG_CATEGORY_RESERVED1, "%s: %s"   , "GLSL Version", glsl_version);

    glDebugMessageCallback(DebugOutputCallBack, NULL);
    if (gl_data->debug) {
        gl_data->glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
    }

    gl_data->glEnable(GL_BLEND);
    gl_data->glEnable(GL_DEPTH_TEST);
    gl_data->glEnable(GL_SCISSOR_TEST);
    gl_data->glEnable(GL_STENCIL_TEST);

    // TODO: more convention to choose
    // glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
    // glEnable(GL_PRIMITIVE_RESTART);
    // glPrimitiveRestartIndex(UINT16_MAX);

    glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &gl_data->max_anisotropy); // return at least 2
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &gl_data->max_vertex_attrib);

    // TODO: get max_buffer_size
    gl_data->max_buffer_size = 128*1024*1024; // spec guarantees that SSBOs can be up to 128MB

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_data->max_texture_size);
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &gl_data->max_texture_depth);

    /* To be compatible with Metal coordinate we use opposite convention: the
     * number of y-flip is identical.
     * For texture upload: data[0] is uv=(0,0) for Metal and OpenGL
     *
     *               Metal     default OpenGL    our OpenGL
     * NDC        :  y up           y up           y down
     * framebuffer:  y down         y up           y up
     * texture    :  y down         y up           y up
     */
    glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);

    glCreateFramebuffers(1, &gl_data->fbo_render);
    if (gl_data->fbo_render == 0) {
        SDL_SetError("Could not create render framebuffer");
        goto error;
    }
    gl_data->glObjectLabel(GL_FRAMEBUFFER, gl_data->fbo_render, -1, "render fbo");

    /* OpenGL has a back framebuffer not a back texture. We create a texture,
     * put it in a framebuffer, and blit that framebuffer to the back buffer in
     * GpuPresent()
     */
    glCreateFramebuffers(1, &gl_data->fbo_backbuffer);
    if (gl_data->fbo_backbuffer == 0) {
        SDL_SetError("Could not create fake back framebuffer");
        goto error;
    }
    gl_data->glObjectLabel(GL_FRAMEBUFFER, gl_data->fbo_backbuffer, -1, "fake back fbo");
    GLuint draw_buffer = GL_COLOR_ATTACHMENT0;
    gl_data->glNamedFramebufferDrawBuffers(gl_data->fbo_backbuffer, 1, &draw_buffer);
    glNamedFramebufferReadBuffer(gl_data->fbo_backbuffer, GL_COLOR_ATTACHMENT0);
    if (!RecreateBackBufferTexture(device)) {
        goto error;
    }

    {
    device->DestroyDevice = OPENGL_GpuDestroyDevice;
    device->ClaimWindow = OPENGL_GpuClaimWindow;
    device->CreateCpuBuffer = OPENGL_GpuCreateCpuBuffer;
    device->DestroyCpuBuffer = OPENGL_GpuDestroyCpuBuffer;
    device->LockCpuBuffer = OPENGL_GpuLockCpuBuffer;
    device->UnlockCpuBuffer = OPENGL_GpuUnlockCpuBuffer;
    device->CreateBuffer = OPENGL_GpuCreateBuffer;
    device->DestroyBuffer = OPENGL_GpuDestroyBuffer;
    device->CreateTexture = OPENGL_GpuCreateTexture;
    device->DestroyTexture = OPENGL_GpuDestroyTexture;
    device->CreateShader = OPENGL_GpuCreateShader;
    device->DestroyShader = OPENGL_GpuDestroyShader;
    device->CreatePipeline = OPENGL_GpuCreatePipeline;
    device->DestroyPipeline = OPENGL_GpuDestroyPipeline;
    device->CreateSampler = OPENGL_GpuCreateSampler;
    device->DestroySampler = OPENGL_GpuDestroySampler;
    device->CreateCommandBuffer = OPENGL_GpuCreateCommandBuffer;
    device->StartRenderPass = OPENGL_GpuStartRenderPass;
    device->SetRenderPassPipeline = OPENGL_GpuSetRenderPassPipeline;
    device->SetRenderPassViewport = OPENGL_GpuSetRenderPassViewport;
    device->SetRenderPassScissor = OPENGL_GpuSetRenderPassScissor;
    device->SetRenderPassBlendConstant = OPENGL_GpuSetRenderPassBlendConstant;
    device->SetRenderPassVertexBuffer = OPENGL_GpuSetRenderPassVertexBuffer;
    device->SetRenderPassVertexSampler = OPENGL_GpuSetRenderPassVertexSampler;
    device->SetRenderPassVertexTexture = OPENGL_GpuSetRenderPassVertexTexture;
    device->SetRenderPassFragmentBuffer = OPENGL_GpuSetRenderPassFragmentBuffer;
    device->SetRenderPassFragmentSampler = OPENGL_GpuSetRenderPassFragmentSampler;
    device->SetRenderPassFragmentTexture = OPENGL_GpuSetRenderPassFragmentTexture;
    device->SetMesh = OPENGL_GpuSetMeshBuffer;
    device->Draw = OPENGL_GpuDraw;
    device->DrawIndexed = OPENGL_GpuDrawIndexed;
    device->DrawInstanced = OPENGL_GpuDrawInstanced;
    device->DrawInstancedIndexed = OPENGL_GpuDrawInstancedIndexed;
    device->EndRenderPass = OPENGL_GpuEndRenderPass;
    device->StartBlitPass = OPENGL_GpuStartBlitPass;
    device->CopyBetweenTextures = OPENGL_GpuCopyBetweenTextures;
    device->FillBuffer = OPENGL_GpuFillBuffer;
    device->GenerateMipmaps = OPENGL_GpuGenerateMipmaps;
    device->CopyBufferCpuToGpu = OPENGL_GpuCopyBufferCpuToGpu;
    device->CopyBufferGpuToCpu = OPENGL_GpuCopyBufferGpuToCpu;
    device->CopyBufferGpuToGpu = OPENGL_GpuCopyBufferGpuToGpu;
    device->CopyFromBufferToTexture = OPENGL_GpuCopyFromBufferToTexture;
    device->CopyFromTextureToBuffer = OPENGL_GpuCopyFromTextureToBuffer;
    device->EndBlitPass = OPENGL_GpuEndBlitPass;
    device->SubmitCommandBuffer = OPENGL_GpuSubmitCommandBuffer;
    device->AbandonCommandBuffer = OPENGL_GpuAbandonCommandBuffer;
    device->GetBackbuffer = OPENGL_GpuGetBackbuffer;
    device->Present = OPENGL_GpuPresent;
    device->CreateFence = OPENGL_GpuCreateFence;
    device->DestroyFence = OPENGL_GpuDestroyFence;
    device->QueryFence = OPENGL_GpuQueryFence;
    device->ResetFence = OPENGL_GpuResetFence;
    device->WaitFence = OPENGL_GpuWaitFence;
    }
    CHECK_GL_ERROR;
    return 0;

    error:
    OPENGL_GpuDestroyDevice(device);
    return -1;
}

const SDL_GpuDriver OPENGL_GpuDriver = {
    "opengl", OPENGL_GpuCreateDevice
};

#endif /* SDL_GPU_OPENGL */

/* vi: set ts=4 sw=4 expandtab: */
