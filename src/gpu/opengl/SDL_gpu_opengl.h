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

#ifndef SDL_gpu_opengl_h_
#define SDL_gpu_opengl_h_

#ifdef SDL_GPU_OPENGL

#include "SDL.h"
#include "../SDL_sysgpu.h"

#include <GL/glcorearb.h>

// driver_data:
// SDL_CpuBuffer        -> buffer glid
// SDL_GpuBuffer        -> buffer glid
// SDL_GpuTexture       -> texture glid
// SDL_GpuShader        -> shader glid
// SDL_GpuPipeline      -> vao glid<<32 | program glid
// SDL_GpuSampler       -> sampler glid

typedef struct OPENGL_GpuRenderPassData {
    GLsizei stride;
    GLenum primitive;
    GLsizei render_targert_height;
} OPENGL_GpuRenderPassData;

typedef struct OPENGL_GpuCommandBuffer {
    struct {
        // we encode only 1 render pass at a time, store render pass data here
        // instead of malloc'ing them
        OPENGL_GpuRenderPassData currrent_render_pass_data;
        SDL_GpuRenderPass *current_render_pass;
    } encoding_state;

    struct {
        int n_color_attachment;
        SDL_bool pop_pass_label;
        SDL_bool pop_pipeline_label;
    } exec_state;

    size_t capacity_cmd;
    size_t n_cmd;
    char cmd[];
} OPENGL_GpuCommandBuffer;

// SDL_GpuBlitPass      ->
// SDL_GpuFence         ->

// grep -E 'gl_data->gl[A-Z][0-Z]*\(' -o ./SDL_gpu_opengl.c | sort -u
// sed -E  's/gl_data->(gl[A-Z].*)\(/GL_FN(PFN\U\1PROC, \L\1), \\/'
#define OPENGL_FUNCTION_X \
GL_FN(PFNGLATTACHSHADERPROC, glAttachShader); \
GL_FN(PFNGLBINDBUFFERPROC, glBindBuffer); \
GL_FN(PFNGLBINDBUFFERRANGEPROC, glBindBufferRange); \
GL_FN(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer); \
GL_FN(PFNGLBINDSAMPLERPROC, glBindSampler); \
GL_FN(PFNGLBINDTEXTUREUNITPROC, glBindTextureUnit); \
GL_FN(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray); \
GL_FN(PFNGLBINDVERTEXBUFFERPROC, glBindVertexBuffer); \
GL_FN(PFNGLBLENDCOLORPROC, glBlendColor); \
GL_FN(PFNGLBLENDEQUATIONSEPARATEIPROC, glBlendEquationSeparatei); \
GL_FN(PFNGLBLENDFUNCSEPARATEIPROC, glBlendFuncSeparatei); \
GL_FN(PFNGLBLITNAMEDFRAMEBUFFERPROC, glBlitNamedFramebuffer); \
GL_FN(PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC, glCheckNamedFramebufferStatus); \
GL_FN(PFNGLCLEARNAMEDBUFFERSUBDATAPROC, glClearNamedBufferSubData); \
GL_FN(PFNGLCLEARNAMEDFRAMEBUFFERFVPROC, glClearNamedFramebufferfv); \
GL_FN(PFNGLCLEARNAMEDFRAMEBUFFERIVPROC, glClearNamedFramebufferiv); \
GL_FN(PFNGLCOLORMASKIPROC, glColorMaski); \
GL_FN(PFNGLCOMPILESHADERPROC, glCompileShader); \
GL_FN(PFNGLCOPYIMAGESUBDATAPROC, glCopyImageSubData); \
GL_FN(PFNGLCOPYNAMEDBUFFERSUBDATAPROC, glCopyNamedBufferSubData); \
GL_FN(PFNGLCREATEBUFFERSPROC, glCreateBuffers); \
GL_FN(PFNGLCREATEPROGRAMPROC, glCreateProgram); \
GL_FN(PFNGLCREATESAMPLERSPROC, glCreateSamplers); \
GL_FN(PFNGLCREATESHADERPROC, glCreateShader); \
GL_FN(PFNGLCREATETEXTURESPROC, glCreateTextures); \
GL_FN(PFNGLCREATEVERTEXARRAYSPROC, glCreateVertexArrays); \
GL_FN(PFNGLCULLFACEPROC, glCullFace); \
GL_FN(PFNGLDELETEBUFFERSPROC, glDeleteBuffers); \
GL_FN(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers); \
GL_FN(PFNGLDELETEPROGRAMPROC, glDeleteProgram); \
GL_FN(PFNGLDELETESAMPLERSPROC, glDeleteSamplers); \
GL_FN(PFNGLDELETESHADERPROC, glDeleteShader); \
GL_FN(PFNGLDELETETEXTURESPROC, glDeleteTextures); \
GL_FN(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays); \
GL_FN(PFNGLDEPTHFUNCPROC, glDepthFunc); \
GL_FN(PFNGLDEPTHMASKPROC, glDepthMask); \
GL_FN(PFNGLDEPTHRANGEINDEXEDPROC, glDepthRangeIndexed); \
GL_FN(PFNGLDISABLEPROC, glDisable); \
GL_FN(PFNGLDISABLEIPROC, glDisablei); \
GL_FN(PFNGLDRAWARRAYSPROC, glDrawArrays); \
GL_FN(PFNGLDRAWELEMENTSPROC, glDrawElements); \
GL_FN(PFNGLENABLEPROC, glEnable); \
GL_FN(PFNGLENABLEIPROC, glEnablei); \
GL_FN(PFNGLENABLEVERTEXARRAYATTRIBPROC, glEnableVertexArrayAttrib); \
GL_FN(PFNGLFRONTFACEPROC, glFrontFace); \
GL_FN(PFNGLGENERATETEXTUREMIPMAPPROC, glGenerateTextureMipmap); \
GL_FN(PFNGLGETERRORPROC, glGetError); \
GL_FN(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog); \
GL_FN(PFNGLGETPROGRAMIVPROC, glGetProgramiv); \
GL_FN(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog); \
GL_FN(PFNGLGETSHADERIVPROC, glGetShaderiv); \
GL_FN(PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC, glInvalidateNamedFramebufferData); \
GL_FN(PFNGLLINKPROGRAMPROC, glLinkProgram); \
GL_FN(PFNGLMAPNAMEDBUFFERPROC, glMapNamedBuffer); \
GL_FN(PFNGLNAMEDBUFFERSTORAGEPROC, glNamedBufferStorage); \
GL_FN(PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC, glNamedFramebufferDrawBuffers); \
GL_FN(PFNGLNAMEDFRAMEBUFFERTEXTUREPROC, glNamedFramebufferTexture); \
GL_FN(PFNGLOBJECTLABELPROC, glObjectLabel); \
GL_FN(PFNGLPOLYGONMODEPROC, glPolygonMode); \
GL_FN(PFNGLPOLYGONOFFSETCLAMPPROC, glPolygonOffsetClamp); \
GL_FN(PFNGLPOPDEBUGGROUPPROC, glPopDebugGroup); \
GL_FN(PFNGLPUSHDEBUGGROUPPROC, glPushDebugGroup); \
GL_FN(PFNGLSAMPLERPARAMETERFPROC, glSamplerParameterf); \
GL_FN(PFNGLSAMPLERPARAMETERFVPROC, glSamplerParameterfv); \
GL_FN(PFNGLSAMPLERPARAMETERIPROC, glSamplerParameteri); \
GL_FN(PFNGLSCISSORPROC, glScissor); \
GL_FN(PFNGLSHADERSOURCEPROC, glShaderSource); \
GL_FN(PFNGLSTENCILFUNCSEPARATEPROC, glStencilFuncSeparate); \
GL_FN(PFNGLSTENCILMASKSEPARATEPROC, glStencilMaskSeparate); \
GL_FN(PFNGLSTENCILOPSEPARATEPROC, glStencilOpSeparate); \
GL_FN(PFNGLTEXTUREPARAMETERIPROC, glTextureParameteri); \
GL_FN(PFNGLTEXTURESTORAGE1DPROC, glTextureStorage1D); \
GL_FN(PFNGLTEXTURESTORAGE2DPROC, glTextureStorage2D); \
GL_FN(PFNGLTEXTURESTORAGE3DPROC, glTextureStorage3D); \
GL_FN(PFNGLTEXTURESUBIMAGE1DPROC, glTextureSubImage1D); \
GL_FN(PFNGLTEXTURESUBIMAGE2DPROC, glTextureSubImage2D); \
GL_FN(PFNGLUNMAPNAMEDBUFFERPROC, glUnmapNamedBuffer); \
GL_FN(PFNGLUSEPROGRAMPROC, glUseProgram); \
GL_FN(PFNGLVALIDATEPROGRAMPROC, glValidateProgram); \
GL_FN(PFNGLVERTEXARRAYATTRIBBINDINGPROC, glVertexArrayAttribBinding); \
GL_FN(PFNGLVERTEXARRAYATTRIBFORMATPROC, glVertexArrayAttribFormat); \
GL_FN(PFNGLVERTEXARRAYATTRIBIFORMATPROC, glVertexArrayAttribIFormat); \
GL_FN(PFNGLVERTEXARRAYATTRIBLFORMATPROC, glVertexArrayAttribLFormat); \
GL_FN(PFNGLVIEWPORTINDEXEDFPROC, glViewportIndexedf);

typedef struct OGL_GpuDevice
{
    SDL_GLContext *context;
    SDL_Window *window;
    SDL_bool dummy_window; // true if we own the window
    SDL_bool debug;
    GLuint fbo_render;
    GLuint fbo_backbuffer;
    GLuint texture_backbuffer;
    SDL_GpuPixelFormat texture_backbuffer_format;
    int w_backbuffer;
    int h_backbuffer;
    int swap_interval;
    GLint max_anisotropy;
    GLsizei max_texture_size;
    GLsizei max_texture_depth;
    GLsizeiptr max_buffer_size;
    GLint max_vertex_attrib;
    SDL_AtomicInt window_size_changed;

#ifndef GL_GLEXT_PROTOTYPES
#define GL_FN(T, N) T N;
    OPENGL_FUNCTION_X
#undef GL_FN
#endif
} OGL_GpuDevice;

#endif /* SDL_GPU_OPENGL */
#endif /* SDL_gpu_opengl_h_ */
