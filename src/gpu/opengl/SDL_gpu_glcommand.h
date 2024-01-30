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

#ifndef SDL_gpu_glcommand_h_
#define SDL_gpu_glcommand_h_

#ifdef SDL_GPU_OPENGL

#include "SDL.h"

#include "../SDL_sysgpu.h"

#include <GL/glcorearb.h>

typedef enum GLCMD_TYPE {
    CMD_NONE,

    CMD_START_RENDER_PASS,
    CMD_SET_PIPELINE,
    CMD_SET_VIEWPORT,
    CMD_SET_SCISSOR,
    CMD_SET_BLEND_CONSTANT,
    CMD_SET_BUFFER,
    CMD_SET_SAMPLER,
    CMD_SET_TEXTURE,
    CMD_SET_MESH,
    CMD_DRAW,
    CMD_DRAW_INDEXED,
    CMD_DRAW_INSTANCED,
    CMD_DRAW_INSTANCED_INDEXED,
    CMD_END_RENDER_PASS,

    CMD_START_BLIT_PASS,
    CMD_FILL_BUFFER,
    CMD_GENERATE_MIPMAP,
    CMD_COPY_TEXTURE,
    CMD_COPY_BUFFER,
    CMD_COPY_BUFFER_TO_TEXTURE_1D,
    CMD_COPY_BUFFER_TO_TEXTURE_2D,
    CMD_COPY_BUFFER_TO_TEXTURE_3D,
    CMD_COPY_TEXTURE_TO_BUFFER_1D,
    CMD_COPY_TEXTURE_TO_BUFFER_2D,
    CMD_COPY_TEXTURE_TO_BUFFER_3D,
    CMD_END_BLIT_PASS,

    CMD_COUNT,
} GLCMD_TYPE;

typedef struct GLCMD_StartRenderPass {
    GLCMD_TYPE type;
    Uint32 n_color_attachments;
    GLuint color_attachments[SDL_GPU_MAX_COLOR_ATTACHMENTS];
    GLuint depth_attachment;
    GLuint stencil_attachment;
    GLuint draw_buffers[SDL_GPU_MAX_COLOR_ATTACHMENTS];
    GLsizei n_invalid;
    GLenum invalidate_buffers[SDL_GPU_MAX_COLOR_ATTACHMENTS+2];
    float clear_color[SDL_GPU_MAX_COLOR_ATTACHMENTS][4];
    float clear_depth_value;
    SDL_bool clear_stencil;
    GLint clear_stencil_value;
    char* pass_label; ///< owned
} GLCMD_StartRenderPass;

typedef struct GLCMD_SetPipeline {
    GLCMD_TYPE type;
    GLuint vao;
    GLuint program;

    struct {
        SDL_bool enable;
        GLenum rgb_mode;
        GLenum alpha_mode;
        GLenum func_rgb_src;
        GLenum func_alpha_src;
        GLenum func_rgb_dst;
        GLenum func_alpha_dst;
    } blend[SDL_GPU_MAX_COLOR_ATTACHMENTS];

    struct {
        GLboolean red;
        GLboolean green;
        GLboolean blue;
        GLboolean alpha;
    } writemask[SDL_GPU_MAX_COLOR_ATTACHMENTS];

    GLboolean depth_mask;
    GLenum depth_func;

    GLfloat depth_bias_scale;
    GLfloat depth_bias;
    GLfloat depth_bias_clamp;

    struct {
        GLenum func;
        GLint  ref;
        GLuint read_mask;
        GLuint write_mask;
        GLenum op_sfail;
        GLenum op_dpfail;
        GLenum op_dppass;
    } stencil_front;

    struct {
        GLenum func;
        GLint  ref;
        GLuint read_mask;
        GLuint write_mask;
        GLenum op_sfail;
        GLenum op_dpfail;
        GLenum op_dppass;
    } stencil_back;

    GLenum polygon_mode;

    SDL_bool enable_cull_face;
    GLenum front_face;
    GLenum cull_face;

    char* pipeline_label;
} GLCMD_SetPipeline;

typedef struct GLCMD_SetViewport {
    GLCMD_TYPE type;
    GLfloat x;
    GLfloat y;
    GLfloat w;
    GLfloat h;
    GLdouble near;
    GLdouble far;
} GLCMD_SetViewport;

typedef struct GLCMD_SetScissor {
    GLCMD_TYPE type;
    GLint x;
    GLint y;
    GLsizei w;
    GLsizei h;
} GLCMD_SetScissor;

typedef struct GLCMD_SetBlendConstant {
    GLCMD_TYPE type;
    GLfloat red;
    GLfloat green;
    GLfloat blue;
    GLfloat alpha;
} GLCMD_SetBlendConstant;

typedef struct GLCMD_SetBuffer {
    GLCMD_TYPE type;
    GLuint index;
    GLuint buffer;
    GLintptr offset;
    GLsizeiptr size;
} GLCMD_SetBuffer;

typedef struct GLCMD_SetTexture {
    GLCMD_TYPE type;
    GLuint unit;
    GLuint texture;
} GLCMD_SetTexture;

typedef struct GLCMD_SetSampler {
    GLCMD_TYPE type;
    GLuint unit;
    GLuint sampler;
} GLCMD_SetSampler;

typedef struct GLCMD_SetMesh {
    GLCMD_TYPE type;
    GLuint index;
    GLuint buffer;
    GLintptr offset;
    GLsizei stride;
} GLCMD_SetMesh;

typedef struct GLCMD_Draw {
    GLCMD_TYPE type;
    GLenum mode;
    GLint first;
    GLsizei count;
} GLCMD_Draw;

typedef struct GLCMD_DrawIndexed {
    GLCMD_TYPE type;
    GLuint index_buffer;
    GLenum mode;
    GLsizei count;
    GLenum index_type;
    const void *indices;
} GLCMD_DrawIndexed;

typedef struct GLCMD_DrawInstanced {
    GLCMD_TYPE type;
    int dummy;
} GLCMD_DrawInstanced;

typedef struct GLCMD_DrawInstancedIndexed {
    GLCMD_TYPE type;
    int dummy;
} GLCMD_DrawInstancedIndexed;

typedef struct GLCMD_EndRenderPass {
    GLCMD_TYPE type;
    int dummy;
} GLCMD_EndRenderPass;

typedef struct GLCMD_StartBlitPass {
    GLCMD_TYPE type;
    char* pass_label;
} GLCMD_StartBlitPass;

typedef struct GLCMD_CopyTexture {
    GLCMD_TYPE type;
    GLuint src;
    GLenum src_target;
    GLint src_level;
    GLint src_x;
    GLint src_y;
    GLint src_z;
    GLuint dst;
    GLenum dst_target;
    GLint dst_level;
    GLint dst_x;
    GLint dst_y;
    GLint dst_z;
    GLsizei src_w;
    GLsizei src_h;
    GLsizei src_d;
} GLCMD_CopyTexture;

typedef struct GLCMD_FillBuffer {
    GLCMD_TYPE type;
    GLuint buffer;
    GLintptr offset;
    GLsizeiptr size;
    Uint8 value;
} GLCMD_FillBuffer;

typedef struct GLCMD_GenerateMipmaps {
    GLCMD_TYPE type;
    GLuint texture;
} GLCMD_GenerateMipmaps;

typedef struct GLCMD_CopyBuffer {
    GLCMD_TYPE type;
    GLuint src;
    GLuint dst;
    GLintptr src_offset;
    GLintptr dst_offset;
    GLsizeiptr size;
} GLCMD_CopyBuffer;

typedef struct GLCMD_CopyFromBufferToTexture {
    GLCMD_TYPE type;
    GLuint buffer;
    GLuint texture;
    GLint level;
    GLint dst_x;
    GLint dst_y;
    GLint dst_z;
    GLsizei dst_w;
    GLsizei dst_h;
    GLsizei dst_d;
    GLenum data_format;
    GLenum data_type;
    Uint32 src_offset;
    Uint32 src_pitch;
    Uint32 src_imgpitch;
} GLCMD_CopyFromBufferToTexture;

typedef struct GLCMD_CopyFromTextureToBuffer {
    GLCMD_TYPE type;
    int dummy;
} GLCMD_CopyFromTextureToBuffer;

typedef struct GLCMD_EndBlitPass {
    GLCMD_TYPE type;
    int dummy;
} GLCMD_EndBlitPass;



#endif // SDL_GPU_OPENGL
#endif // SDL_gpu_glcommand_h_
