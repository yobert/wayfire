#ifndef DESKTOP_API_HPP
#define DESKTOP_API_HPP

#include <compositor.h>
#include <libweston-desktop.h>
#include <algorithm>

#include "debug.hpp"
#include "core.hpp"
#include "output.hpp"
#include "workspace-manager.hpp"
#include "signal-definitions.hpp"
#include "view.hpp"

void desktop_surface_added(weston_desktop_surface *desktop_surface, void *shell)
{
    debug << "desktop_surface_added " << desktop_surface << std::endl;
    core->add_view(desktop_surface);
}

void desktop_surface_removed(weston_desktop_surface *surface, void *user_data)
{
    debug << "desktop_surface_removed " << surface << std::endl;

    auto view = core->find_view(surface);
    weston_desktop_surface_unlink_view(view->handle);

    pixman_region32_fini(&view->surface->pending.input);
    pixman_region32_init(&view->surface->pending.input);
    pixman_region32_fini(&view->surface->input);
    pixman_region32_init(&view->surface->input);

    view->destroyed = true;

    if (view->output)
    {
        auto sig_data = destroy_view_signal{view};
        view->output->emit_signal("destroy-view", &sig_data);

        if (view->parent)
        {
            auto it = std::find(view->parent->children.begin(), view->parent->children.end(), view);
            assert(it != view->parent->children.end());
            view->parent->children.erase(it);
        }
    }

    core->erase_view(view, view->keep_count <= 0);
}

void desktop_surface_commited (weston_desktop_surface *desktop_surface,
        int32_t sx, int32_t sy, void *data)
{
    auto view = core->find_view(desktop_surface);
    assert(view != nullptr);

    if (view->surface->width == 0) {
        return;
    }

    view->map(sx, sy);
}

void desktop_surface_set_xwayland_position(weston_desktop_surface *desktop_surface,
        int32_t x, int32_t y, void *shell)
{
    auto view = core->find_view(desktop_surface);
    assert(view != nullptr);

    view->xwayland.is_xorg = true;
    view->xwayland.x = x;
    view->xwayland.y = y;
}

void desktop_surface_move(weston_desktop_surface *ds, weston_seat *seat,
        uint32_t serial, void *shell)
{
    auto view = core->find_view(ds);

    auto main_surface = weston_surface_get_main_surface(view->surface);
    if (main_surface == view->surface) {
        move_request_signal req;
        req.view = core->find_view(main_surface);
        req.serial = serial;
        view->output->emit_signal("move-request", &req);
    }
}

void desktop_surface_resize(weston_desktop_surface *ds, weston_seat *seat,
        uint32_t serial, weston_desktop_surface_edge edges, void *shell)
{
    auto view = core->find_view(ds);

    auto main_surface = weston_surface_get_main_surface(view->surface);
    if (main_surface == view->surface) {
        resize_request_signal req;
        req.view = core->find_view(main_surface);
        req.edges = edges;
        req.serial = serial;
        view->output->emit_signal("resize-request", &req);
    }
}

void desktop_surface_maximized_requested(weston_desktop_surface *ds,
        bool maximized, void *shell)
{
    auto view = core->find_view(ds);
    if (!view || view->maximized == maximized)
        return;

    view_maximized_signal data;
    data.view = view;
    data.state = maximized;

    if (view->is_mapped) {
        view->output->emit_signal("view-maximized-request", &data);
    } else if (maximized) {
        view->set_geometry(view->output->workspace->get_workarea());
        view->output->emit_signal("view-maximized", &data);
    }

    view->set_maximized(maximized);
}

void desktop_surface_fullscreen_requested(weston_desktop_surface *ds,
        bool full, weston_output *output, void *shell)
{
    auto view = core->find_view(ds);
    if (!view || view->fullscreen == full)
        return;

    auto wo = (output ? core->get_output(output) : view->output);
    assert(wo);

    if (view->output != wo)
    {
        auto pg = view->output->get_full_geometry();
        auto ng = wo->get_full_geometry();

        core->move_view_to_output(view, wo);
        view->move(view->geometry.x + ng.x - pg.x, view->geometry.y + ng.y - pg.y);
    }

    view_fullscreen_signal data;
    data.view = view;
    data.state = full;

    if (view->is_mapped) {
        wo->emit_signal("view-fullscreen-request", &data);
    } else if (full) {
        view->set_geometry(view->output->get_full_geometry());
        view->output->emit_signal("view-fullscreen", &data);
    }

    view->set_fullscreen(full);
}

void desktop_surface_set_parent(weston_desktop_surface *ds,
                                weston_desktop_surface *parent_ds,
                                void *data)
{
    auto view = core->find_view(ds);
    auto parent = core->find_view(parent_ds);

    if (!view || !parent)
        return;

    view->parent = parent;
    parent->children.push_back(view);

    view_set_parent_signal sdata; sdata.view = view;
    view->output->emit_signal("view-set-parent", &sdata);
}

#endif /* end of include guard: DESKTOP_API_HPP */
