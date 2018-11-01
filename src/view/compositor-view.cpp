#include "priv-view.hpp"
#include "output.hpp"
#include "workspace-manager.hpp"
#include "compositor-view.hpp"

wayfire_compositor_view_t::wayfire_compositor_view_t()
{
    role = WF_VIEW_ROLE_TOPLEVEL;
}

wf_point wayfire_compositor_view_t::get_output_position()
{
    return {geometry.x, geometry.y};
}

wf_geometry wayfire_compositor_view_t::get_output_geometry()
{
    return geometry;
}

wf_geometry wayfire_compositor_view_t::get_wm_geometry()
{
    return geometry;
}

void wayfire_compositor_view_t::map()
{
    if (map_state)
        return;

    output->attach_view(self());
    emit_view_map(self());

    map_state = true;
}

void wayfire_compositor_view_t::unmap()
{
    if (!map_state)
        return;

    if (output)
        emit_view_unmap(self());

    map_state = false;
}

void wayfire_compositor_view_t::close()
{
    unmap();
    destroy();
}

void wayfire_compositor_view_t::render_fb(pixman_region32_t *damage, wf_framebuffer fb)
{
    /* We only want to circumvent wayfire_surface_t::render_fb,
     * because it relies on the surface having a buffer
     *
     * If there is a transform though, we're fine with what wayfire_view_t provides */
    if (has_transformer())
        return wayfire_view_t::render_fb(damage, fb);

    GL_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb.fb));
    auto obox = get_output_geometry();

    wlr_fb_attribs attribs;
    attribs.width = output->handle->width;
    attribs.height = output->handle->height;
    attribs.transform = output->handle->transform;

    render_pixman(attribs, obox.x - fb.geometry.x, obox.y - fb.geometry.y, damage);
}
