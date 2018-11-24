#ifndef DRIVER_H
#define DRIVER_H

#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include <config.hpp>
#include <nonstd/noncopyable.hpp>

extern "C"
{
#include <wlr/types/wlr_box.h>
}

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <map>

class wayfire_output;
using wf_geometry = wlr_box;

void gl_call(const char*, uint32_t, const char*);

#ifndef __STRING
#  define __STRING(x) #x
#endif

/* recommended to use this to make OpenGL calls, since it offers easier debugging */
/* This macro is taken from WLC source code */
#define GL_CALL(x) x; gl_call(__PRETTY_FUNCTION__, __LINE__, __STRING(x))

#define TEXTURE_TRANSFORM_INVERT_X     (1 << 0)
#define TEXTURE_TRANSFORM_INVERT_Y     (1 << 1)
#define TEXTURE_TRANSFORM_USE_COLOR    (1 << 2)
#define TEXTURE_USE_TEX_GEOMETRY       (1 << 4)

struct gl_geometry
{
    float x1, y1, x2, y2;
};

/* Simple framebuffer, used mostly to allocate framebuffers for workspace
 * streams.
 *
 * Resources (tex/fb) are not automatically destroyed */
struct wf_framebuffer_base : public noncopyable_t
{
    GLuint tex = -1, fb = -1;
    int32_t viewport_width = 0, viewport_height = 0;

    wf_framebuffer_base() = default;
    wf_framebuffer_base(wf_framebuffer_base&& other);
    wf_framebuffer_base& operator = (wf_framebuffer_base&& other);

    /* The functions below assume they are called between
     * OpenGL::render_begin() and OpenGL::render_end() */

    /* will invalidate texture contents if width or height changes.
     * If tex and/or fb haven't been set, it creates them
     * Return true if texture was created/invalidated */
    bool allocate(int width, int height);

    /* Make the framebuffer current, and adjust viewport to its size */
    void bind() const;

    /* Set the GL scissor to the given box, after inverting it to match GL
     * coordinate space */
    void scissor(wlr_box box) const;

    /* Will destroy the texture and framebuffer
     * Warning: will destroy tex/fb even if they have been allocated outside of
     * allocate() */
    void release();

    /* Reset the framebuffer, WITHOUT freeing resources.
     * There is no need to call reset() after release() */
    void reset();

    private:
    void copy_state(wf_framebuffer_base&& other);
};

/* A more feature-complete framebuffer.
 * It represents an area of the output, with the corresponding dimensions,
 * transforms, etc */
struct wf_framebuffer : public wf_framebuffer_base
{
    wf_geometry geometry = {0, 0, 0, 0};

    glm::mat4 transform = glm::mat4(1.0);
    uint32_t wl_transform = WL_OUTPUT_TRANSFORM_NORMAL;
};

namespace OpenGL
{
    /* NOT API
     * Initialize OpenGL helper functions */
    void init();
    /* NOT API
     * Destroys the default GL program and resources */
    void fini();
    /* NOT API
     * Indicate we have started repainting the given output */
    void bind_output(wayfire_output *output);
    /* NOT API
     * Indicate the output frame has been finished */
    void unbind_output(wayfire_output *output);

    /* "Begin" rendering to the given framebuffer and the given viewport.
     * All rendering operations should happen between render_begin and render_end, because
     * that's the only time we're guaranteed we have a valid GLES context
     *
     * The other functions below assume they are called between render_begin()
     * and render_end() */
    void render_begin(); // use if you just want to bind GL context but won't draw
    void render_begin(const wf_framebuffer_base& fb);
    void render_begin(int32_t viewport_width, int32_t viewport_height, uint32_t fb = 0);

    /* Call this to indicate an end of the rendering.
     * Resets bound framebuffer and scissor box.
     * render_end() must be called for each render_begin() */
    void render_end();

    /* Clear the currently bound framebuffer with the given color */
    void clear(wf_color color, uint32_t mask = GL_COLOR_BUFFER_BIT);

    /* texg arguments are used only when bits has USE_TEX_GEOMETRY
     * if you don't wish to use them, simply pass {} as argument */
    void render_transformed_texture(GLuint text,
                                    const gl_geometry& g,
                                    const gl_geometry& texg,
                                    glm::mat4 transform = glm::mat4(1.0),
                                    glm::vec4 color = glm::vec4(1.f),
                                    uint32_t bits = 0);

    /* Reads the shader source from the given file and compiles it */
    GLuint load_shader(std::string path, GLuint type);
    /* Compiles the given shader source */
    GLuint compile_shader(std::string source, GLuint type);
}

/* utils */
glm::mat4 get_output_matrix_from_transform(wl_output_transform transform);
glm::mat4 output_get_projection(wayfire_output *output);

#endif
