#include <algorithm>
extern "C"
{
#define static
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/util/region.h>
#undef static
}

#include "priv-view.hpp"
#include "opengl.hpp"
#include "core.hpp"
#include "output.hpp"
#include "debug.hpp"
#include "signal-definitions.hpp"

void handle_surface_committed(wl_listener*, void *data)
{
    auto wlr_surf = (wlr_surface*) data;
    auto surface = wf_surface_from_void(wlr_surf->data);
    assert(surface);

    surface->commit();
}

void handle_subsurface_created(wl_listener*, void *data)
{
    auto sub = static_cast<wlr_subsurface*> (data);
    if (sub->surface->data)
        return;

    auto parent = wf_surface_from_void(sub->parent->data);
    if (!parent)
    {
        log_error("subsurface created with invalid parent!");
        return;
    }

    auto it = std::find_if(parent->surface_children.begin(),
                           parent->surface_children.end(),
                           [=] (wayfire_surface_t *surface)
                            { return surface->surface == sub->surface; });

    if (it == parent->surface_children.end())
    {
        auto surf = new wayfire_surface_t(parent);
        surf->map(sub->surface);
    } else
    {
        log_debug("adding same subsurface twice");
    }
}

void handle_subsurface_destroyed(wl_listener*, void *data)
{
    auto wlr_surf = (wlr_surface*) data;
    auto surface = wf_surface_from_void(wlr_surf->data);

    log_error ("subsurface destroyed %p", wlr_surf);

    surface->unmap();
    surface->dec_keep_count();
}

wayfire_surface_t::wayfire_surface_t(wayfire_surface_t* parent)
{
    inc_keep_count();
    this->parent_surface = parent;

    if (parent)
    {
        set_output(parent->output);
        parent->surface_children.push_back(this);
    }

    new_sub.notify   = handle_subsurface_created;
    committed.notify = handle_surface_committed;
    destroy.notify   = nullptr;
}

wayfire_surface_t::~wayfire_surface_t()
{
    if (parent_surface)
    {
        auto it = parent_surface->surface_children.begin();
        while(it != parent_surface->surface_children.end())
        {
            if (*it == this)
                it = parent_surface->surface_children.erase(it);
            else
                ++it;
        }
    }

    for (auto c : surface_children)
        c->parent_surface = nullptr;
}

wayfire_surface_t *wayfire_surface_t::get_main_surface()
{
    return this->parent_surface ? this->parent_surface->get_main_surface() : this;
}

bool wayfire_surface_t::is_subsurface()
{
    return wlr_surface_is_subsurface(surface);
}

void wayfire_surface_t::get_child_position(int &x, int &y)
{
    auto sub = wlr_subsurface_from_wlr_surface(surface);
    x = sub->current.x;
    y = sub->current.y;
}

void wayfire_surface_t::get_child_offset(int &x, int &y)
{
    x = 0;
    y = 0;
};

void wayfire_surface_t::send_frame_done(const timespec& time)
{
    wlr_surface_send_frame_done(surface, &time);
}

bool wayfire_surface_t::accepts_input(int32_t sx, int32_t sy)
{
    if (!surface)
        return false;

    return wlr_surface_point_accepts_input(surface, sx, sy);
}

wf_point wayfire_surface_t::get_output_position()
{
    auto pos = parent_surface->get_output_position();

    int dx, dy;
    get_child_position(dx, dy);
    pos.x += dx; pos.y += dy;

    return pos;
}

wf_geometry wayfire_surface_t::get_output_geometry()
{
    if (!is_mapped())
        return geometry;

    auto pos = get_output_position();
    return {
        pos.x, pos.y,
        surface->current.width,
        surface->current.height
    };
}

void wayfire_surface_t::map(wlr_surface *surface)
{
    assert(!this->surface && surface);
    this->surface = surface;

    /* force surface_send_enter() */
    set_output(output);

    wl_signal_add(&surface->events.new_subsurface, &new_sub);
    wl_signal_add(&surface->events.commit,         &committed);

    /* map by default if this is a subsurface, only toplevels/popups have map/unmap events */
    if (wlr_surface_is_subsurface(surface))
    {
        destroy.notify = handle_subsurface_destroyed;
        wl_signal_add(&surface->events.destroy, &destroy);
    }

    surface->data = this;
    damage();

    wlr_subsurface *sub;
    wl_list_for_each(sub, &surface->subsurfaces, parent_link)
        handle_subsurface_created(NULL, sub);
}

void wayfire_surface_t::unmap()
{
    assert(this->surface);
    damage();

    this->surface = nullptr;

    if (this->output)
    {
        _surface_unmapped_signal data;
        data.surface = this;
        output->emit_signal("_surface_unmapped", &data);
    }

    wl_list_remove(&new_sub.link);
    wl_list_remove(&committed.link);
    if (destroy.notify)
        wl_list_remove(&destroy.link);
}

wlr_buffer* wayfire_surface_t::get_buffer()
{
    if (surface && wlr_surface_has_buffer(surface))
        return surface->buffer;

    return nullptr;
}

void wayfire_surface_t::inc_keep_count()
{
    ++keep_count;
}

void wayfire_surface_t::dec_keep_count()
{
    --keep_count;
    if (!keep_count)
        destruct();
}

void wayfire_surface_t::damage(pixman_region32_t *region)
{
    int n_rect;
    auto rects = pixman_region32_rectangles(region, &n_rect);

    for (int i = 0; i < n_rect; i++)
        damage(wlr_box_from_pixman_box(rects[i]));
}

void wayfire_surface_t::damage(const wlr_box& box)
{
    if (parent_surface)
        parent_surface->damage(box);
}

void wayfire_surface_t::damage()
{
    /* TODO: bounding box damage */
    damage(geometry);
}

void wayfire_surface_t::update_output_position()
{
    wf_geometry rect = get_output_geometry();

    /* TODO: recursively damage children? */
    if (is_subsurface() && rect != geometry)
    {
        damage(geometry);
        damage(rect);

        geometry = rect;
    }
}

void wayfire_surface_t::apply_surface_damage(int x, int y)
{
    pixman_region32_t dmg;
    pixman_region32_init(&dmg);
    pixman_region32_copy(&dmg, &surface->buffer_damage);

    wl_output_transform transform = wlr_output_transform_invert(surface->current.transform);
    wlr_region_transform(&dmg, &dmg, transform, surface->current.buffer_width, surface->current.buffer_height);
    float scale = 1.0 / (float)surface->current.scale;
    wlr_region_scale(&dmg, &dmg, scale);

    if (surface->current.scale != 1 || surface->current.scale != output->handle->scale)
        wlr_region_expand(&dmg, &dmg, 1);

    pixman_region32_translate(&dmg, x, y);
    damage(&dmg);
    pixman_region32_fini(&dmg);
}

void wayfire_surface_t::commit()
{
    update_output_position();
    auto pos = get_output_position();
    apply_surface_damage(pos.x, pos.y);
}

void wayfire_surface_t::set_output(wayfire_output *out)
{
    if (output && surface)
        wlr_surface_send_leave(surface, output->handle);

    if (out && surface)
        wlr_surface_send_enter(surface, out->handle);

    output = out;
    for (auto c : surface_children)
        c->set_output(out);
}

void wayfire_surface_t::for_each_surface_recursive(wf_surface_iterator_callback call,
                                                   int x, int y, bool reverse)
{
    if (reverse)
    {
        if (is_mapped())
            call(this, x, y);

        int dx, dy;

        for (auto c : surface_children)
        {
            if (!c->is_mapped())
                continue;

            c->get_child_position(dx, dy);
            c->for_each_surface_recursive(call, x + dx, y + dy, reverse);
        }
    } else
    {
        auto it = surface_children.rbegin();
        int dx, dy;

        while(it != surface_children.rend())
        {
            auto& c = *it;

            if (c->is_mapped())
            {
                c->get_child_position(dx, dy);
                c->for_each_surface_recursive(call, x + dx, y + dy, reverse);
            }

            ++it;
        }

        if (is_mapped())
            call(this, x, y);
    }
}

void wayfire_surface_t::for_each_surface(wf_surface_iterator_callback call, bool reverse)
{
    auto pos = get_output_position();
    for_each_surface_recursive(call, pos.x, pos.y, reverse);
}

void wayfire_surface_t::_wlr_render_box(const wlr_fb_attribs& fb, int x, int y, const wlr_box& scissor)
{
    if (!get_buffer())
        return;

    wlr_box geometry {x, y, surface->current.width, surface->current.height};
    geometry = get_output_box_from_box(geometry, output->handle->scale);

    float projection[9];
    wlr_matrix_projection(projection, fb.width, fb.height, fb.transform);

    float matrix[9];
    wlr_matrix_project_box(matrix, &geometry, surface->current.transform, 0, projection);

    wlr_renderer_begin(core->renderer, fb.width, fb.height);
    auto sbox = scissor; wlr_renderer_scissor(core->renderer, &sbox);
    wlr_render_texture_with_matrix(core->renderer, get_buffer()->texture, matrix, alpha);

#ifdef WAYFIRE_GRAPHICS_DEBUG
    float scissor_proj[9];
    wlr_matrix_projection(scissor_proj, fb.width, fb.height, WL_OUTPUT_TRANSFORM_NORMAL);

    float col[4] = {0, 0.2, 0, 0.5};
    wlr_render_rect(core->renderer, &scissor, col, scissor_proj);
#endif

    wlr_renderer_end(core->renderer);
}

void wayfire_surface_t::_render_pixman(const wlr_fb_attribs& fb, int x, int y, pixman_region32_t *damage)
{
    int n_rect;
    auto rects = pixman_region32_rectangles(damage, &n_rect);

    for (int i = 0; i < n_rect; i++)
    {
        auto rect = wlr_box_from_pixman_box(rects[i]);
        auto box = get_scissor_box(output, rect);
        _wlr_render_box(fb, x, y, box);
    }
}

void wayfire_surface_t::render_pixman(const wlr_fb_attribs& fb, int x, int y, pixman_region32_t *damage)
{
    if (damage)
        return _render_pixman(fb, x, y, damage);

    pixman_region32_t full_region;

//    auto box = get_output_box_from_box({x, y, geometry.width, geometry.height}, output->handle->scale);
    pixman_region32_init_rect(&full_region, 0, 0, fb.width, fb.height);

    _render_pixman(fb, x, y, &full_region);
    pixman_region32_fini(&full_region);
}

void wayfire_surface_t::render_fb(pixman_region32_t *damage, wf_framebuffer fb)
{
    if (!is_mapped() || !wlr_surface_has_buffer(surface))
        return;

    GL_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb.fb));
    auto obox = get_output_geometry();

    wlr_fb_attribs attribs;
    attribs.width = output->handle->width;
    attribs.height = output->handle->height;
    attribs.transform = output->handle->transform;

    render_pixman(attribs, obox.x - fb.geometry.x, obox.y - fb.geometry.y, damage);
}
