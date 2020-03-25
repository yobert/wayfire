#pragma once

#include <cairo.h>
#include <wayfire/opengl.hpp>

/**
 * Upload the data from the cairo surface to the OpenGL texture.
 *
 * @param surface The source cairo surface.
 * @param buffer  The buffer to upload data to.
 */
static void cairo_surface_upload_to_texture(
    cairo_surface_t *surface, wf::framebuffer_base_t& buffer)
{
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);

    buffer.allocate(width, height);

    auto src = cairo_image_surface_get_data(surface);

    GL_CALL(glBindTexture(GL_TEXTURE_2D, buffer.tex));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
            GL_UNSIGNED_BYTE, src));
}
