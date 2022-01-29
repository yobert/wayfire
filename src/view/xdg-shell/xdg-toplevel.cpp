#include "xdg-toplevel.hpp"
#include "../core/core-impl.hpp"
#include "../view-impl.hpp"
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-helpers.hpp>
#include <wayfire/output-layout.hpp>

wf::xdg_toplevel_t::xdg_toplevel_t(wlr_xdg_toplevel *toplevel,
    wf::output_t *initial_output)
{
    this->_current.primary_output = initial_output;
    this->toplevel = toplevel;

    on_destroy.set_callback([&] (void*)
    {
        destroy();
    });
    on_request_move.set_callback([&] (void*)
    {
        toplevel_emit_move_request({this});
    });
    on_request_resize.set_callback([&] (auto data)
    {
        auto ev = static_cast<wlr_xdg_toplevel_resize_event*>(data);
        toplevel_emit_resize_request({this}, ev->edges);
    });
    on_request_minimize.set_callback([&] (void*)
    {
        toplevel_emit_minimize_request({this}, true);
    });
    on_request_maximize.set_callback([&] (void *data)
    {
        toplevel_emit_tile_request({this}, (toplevel->requested.maximized ?
            wf::TILED_EDGES_ALL : 0));
    });
    on_request_fullscreen.set_callback([&] (void *data)
    {
        auto ev = static_cast<wlr_xdg_toplevel_set_fullscreen_event*>(data);
        auto wo = wf::get_core().output_layout->find_output(ev->output);
        toplevel_emit_fullscreen_request({this}, wo, ev->fullscreen);
    });

    on_request_move.connect(&toplevel->events.request_move);
    on_request_resize.connect(&toplevel->events.request_resize);
    on_request_maximize.connect(&toplevel->events.request_maximize);
    on_request_minimize.connect(&toplevel->events.request_minimize);
    on_request_fullscreen.connect(&toplevel->events.request_fullscreen);
}

static wf::geometry_t get_xdg_geometry(wlr_xdg_toplevel *toplevel)
{
    wlr_box xdg_geometry;
    wlr_xdg_surface_get_geometry(toplevel->base, &xdg_geometry);
    return xdg_geometry;
}

void wf::xdg_toplevel_t::commit()
{
    auto wmg = get_xdg_geometry(toplevel);
    if (decorator)
    {
        wmg = wf::expand_with_margins(wmg, decorator->get_margins());
    }

    auto wmoffset = wf::origin(wmg);
    wf::dimensions_t full_size = {
        toplevel->base->surface->current.width,
        toplevel->base->surface->current.height,
    };

    if ((wmoffset != xdg_surface_offset) ||
        (wf::dimensions(wmg) != wf::dimensions(_current.geometry)) ||
        (full_size != wf::dimensions(_current.base_geometry)))
    {
        // Something changed. Recalculate both geometries we have.
        wf::toplevel_geometry_changed_signal data;
        data.toplevel     = {this};
        data.old_geometry = _current.geometry;

        _current.geometry = adjust_geometry_for_gravity(_current.geometry,
            this->resizing_edges, wf::dimensions(wmg));

        const auto& wm = _current.geometry;
        _current.base_geometry = {
            .x     = wm.x - wmoffset.x,
            .y     = wm.y - wmoffset.y,
            .width = full_size.width,
            .height = full_size.height,
        };

        xdg_surface_offset = wmoffset;
        if (decorator)
        {
            decorator->notify_resized(wm);
        }

        emit_toplevel_signal(this, "geometry-changed", &data);
    }

    /* Clear the resize edges.
     * This is must be done here because if the user(or plugin) resizes too fast,
     * the shell client might still haven't configured the surface, and in this
     * case the next commit(here) needs to still have access to the gravity */
    if (!this->is_resizing())
    {
        this->resizing_edges = 0;
    }

    if (toplevel->base->current.configure_serial == this->last_configure_serial)
    {
        this->last_size_request = wf::dimensions(wmg);
    }
}

void wf::xdg_toplevel_t::destroy()
{
    on_destroy.disconnect();
    on_request_move.disconnect();
    on_request_resize.disconnect();
    on_request_maximize.disconnect();
    on_request_minimize.disconnect();
    on_request_fullscreen.disconnect();

    toplevel = nullptr;
}

bool wf::xdg_toplevel_t::should_be_decorated()
{
    if (!toplevel)
    {
        return false;
    }

    bool has_csd = has_xdg_decoration_csd;
    auto surface = toplevel->base->surface;
    if (wf::get_core_impl().uses_csd.count(surface))
    {
        has_csd = wf::get_core_impl().uses_csd[surface];
    }

    return !has_csd;
}

wf::toplevel_state_t& wf::xdg_toplevel_t::current()
{
    return _current;
}

void wf::xdg_toplevel_t::set_minimized(bool minimized)
{
    _current.minimized = minimized;

    wf::toplevel_minimized_signal data;
    data.toplevel = {this};
    data.state    = minimized;
    wf::emit_toplevel_signal(this, "minimized", &data);
}

void wf::xdg_toplevel_t::set_tiled(uint32_t edges)
{
    if (toplevel)
    {
        wlr_xdg_toplevel_set_tiled(toplevel->base, edges);
        last_configure_serial = wlr_xdg_toplevel_set_maximized(toplevel->base,
            (edges == wf::TILED_EDGES_ALL));
    }

    wf::toplevel_tiled_signal data;
    data.toplevel  = {this};
    data.old_edges = _current.tiled_edges;

    _current.tiled_edges = edges;
    if (decorator)
    {
        decorator->notify_tiled();
    }

    data.new_edges = edges;
    wf::emit_toplevel_signal(this, "tiled", &data);
}

void wf::xdg_toplevel_t::set_fullscreen(bool fullscreen)
{
    if (toplevel)
    {
        last_configure_serial =
            wlr_xdg_toplevel_set_fullscreen(toplevel->base, fullscreen);
    }

    _current.fullscreen = fullscreen;
    if (decorator)
    {
        decorator->notify_fullscreen();
    }

    wf::toplevel_fullscreen_signal data;
    data.toplevel = {this};
    data.state    = fullscreen;
    wf::emit_toplevel_signal(this, "fullscreen", &data);
}

void wf::xdg_toplevel_t::set_activated(bool active)
{
    if (toplevel)
    {
        last_configure_serial =
            wlr_xdg_toplevel_set_activated(toplevel->base, active);
    }

    _current.activated = active;
    if (decorator)
    {
        decorator->notify_activated(active);
    }

    // XXX: no signals here, do we need them at all??
}

bool wf::xdg_toplevel_t::should_resize_client(
    wf::dimensions_t request, wf::dimensions_t current_geometry)
{
    /*
     * Do not send a configure if the client will retain its size.
     * This is needed if a client starts with one size and immediately resizes
     * again.
     *
     * If we do configure it with the given size, then it will think that we
     * are requesting the given size, and won't resize itself again.
     */
    if (this->last_size_request == wf::dimensions_t{0, 0})
    {
        return request != current_geometry;
    } else
    {
        return request != last_size_request;
    }
}

void wf::xdg_toplevel_t::move(int x, int y)
{
    wf::toplevel_geometry_changed_signal data;
    data.toplevel     = {this};
    data.old_geometry = _current.geometry;

    auto offset = xdg_surface_offset;
    if (decorator)
    {
        auto margins = decorator->get_margins();
        offset.x -= margins.left;
        offset.y -= margins.top;
    }

    _current.geometry.x = x;
    _current.geometry.y = y;
    _current.base_geometry.x    = x - offset.x;
    _current.base_geometry.y    = y - offset.y;
    _current.flags.has_position = true;

    wf::emit_toplevel_signal(this, "geometry-changed", &data);
}

void wf::xdg_toplevel_t::set_geometry(wf::geometry_t g)
{
    if (toplevel)
    {
        auto w = g.width;
        auto h = g.height;
        if (decorator)
        {
            const auto inside =
                wf::shrink_by_margins(g, decorator->get_margins());
            w = inside.width;
            h = inside.height;
        }

        auto current_geometry = get_xdg_geometry(toplevel);
        wf::dimensions_t current_size{current_geometry.width,
            current_geometry.height};
        if (should_resize_client({w, h}, current_size))
        {
            this->last_size_request = {w, h};
            last_configure_serial   =
                wlr_xdg_toplevel_set_size(toplevel->base, w, h);
        }
    }

    move(g.x, g.y);
}

void wf::xdg_toplevel_t::set_output(wf::output_t *new_output)
{
    wf::toplevel_output_changed_signal data;
    data.toplevel   = {this};
    data.old_output = _current.primary_output;

    _current.primary_output = new_output;
    wf::emit_toplevel_signal(this, "output-changed", &data);
}

void wf::xdg_toplevel_t::set_moving(bool moving)
{
    this->moving_counter += moving ? 1 : -1;
    if (this->moving_counter < 0)
    {
        LOGE("in_continuous_move counter dropped below 0!");
    }
}

bool wf::xdg_toplevel_t::is_moving()
{
    return this->moving_counter;
}

void wf::xdg_toplevel_t::set_resizing(bool resizing, uint32_t edges)
{
    if (resizing)
    {
        resizing_edges = edges;
    }

    this->resizing_counter += resizing ? 1 : -1;
    if (this->resizing_counter < 0)
    {
        LOGE("in_continuous_resize counter dropped below 0!");
    }
}

bool wf::xdg_toplevel_t::is_resizing()
{
    return this->resizing_counter;
}

void wf::xdg_toplevel_t::request_native_size()
{
    if (toplevel)
    {
        last_configure_serial =
            wlr_xdg_toplevel_set_size(toplevel->base, 0, 0);
    }
}

void wf::xdg_toplevel_t::set_decoration(
    std::unique_ptr<toplevel_decorator_t> frame)
{
    // TODO
}

void wf::xdg_toplevel_t::set_decoration_mode(bool decorated)
{
    bool old_status = should_be_decorated();
    this->has_xdg_decoration_csd = decorated;
    if (old_status != should_be_decorated())
    {
        wf::toplevel_decoration_state_updated_signal data;
        data.toplevel = {this};
        wf::emit_toplevel_signal(this, "decoration-state-updated", &data);
    }
}
