#include <core.hpp>
#include "simple-background.hpp"

wf_cube_simple_background::wf_cube_simple_background()
{
    background_color = (*core->config)["cube"]->get_option("background", "0 0 0 1");
}

void wf_cube_simple_background::render_frame(const wf_framebuffer& fb,
    wf_cube_animation_attribs&)
{
    OpenGL::render_begin(fb);
    OpenGL::clear(background_color->as_cached_color(),
        GL_COLOR_BUFFER_BIT);
    OpenGL::render_end();
}
