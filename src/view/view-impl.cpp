#include "wayfire/core.hpp"
#include "../core/core-impl.hpp"
#include "../output/gtk-shell.hpp"
#include "view-impl.hpp"
#include "wayfire/decorator.hpp"
#include "wayfire/signal-definitions.hpp"
#include "wayfire/workspace-manager.hpp"
#include <wayfire/util/log.hpp>
#include <wayfire/debug.hpp>

#include "xdg-shell.hpp"

bool wf::wlr_desktop_surface_t::accepts_focus() const
{
    return keyboard_focus_enabled;
}

void wf::wlr_desktop_surface_t::handle_keyboard_enter()
{
    auto seat = wf::get_core().get_current_seat();
    auto kbd  = wlr_seat_get_keyboard(seat);
    wlr_seat_keyboard_notify_enter(seat,
        view->get_main_surface()->get_wlr_surface(),
        kbd ? kbd->keycodes : NULL,
        kbd ? kbd->num_keycodes : 0,
        kbd ? &kbd->modifiers : NULL);
}

void wf::wlr_desktop_surface_t::handle_keyboard_leave()
{
    auto seat = wf::get_core().get_current_seat();
    wlr_seat_keyboard_clear_focus(seat);
}

void wf::wlr_desktop_surface_t::handle_keyboard_key(wlr_event_keyboard_key event)
{
    auto seat = wf::get_core().get_current_seat();
    wlr_seat_keyboard_notify_key(seat,
        event.time_msec, event.keycode, event.state);
}

wf::keyboard_surface_t& wf::wlr_desktop_surface_t::get_keyboard_focus()
{
    return *this;
}

wf::desktop_surface_t::role wf::wlr_desktop_surface_t::get_role() const
{
    return current_role;
}

void wf::wlr_desktop_surface_t::close()
{
    view->close();
}

void wf::wlr_desktop_surface_t::ping()
{
    view->ping();
}

/* if the view isn't mapped, then it will be positioned properly in map() */
if (is_mapped())
{
    reposition_relative_to_parent(self());
}

static void reposition_relative_to_parent(wayfire_view view)
{
    if (!view->parent)
    {
        return;
    }

    auto parent_geometry = view->parent->get_wm_geometry();
    auto wm_geometry     = view->get_wm_geometry();
    auto scr_size = view->get_output()->get_screen_size();
    // Guess which workspace the parent is on
    wf::point_t center = {
        parent_geometry.x + parent_geometry.width / 2,
        parent_geometry.y + parent_geometry.height / 2,
    };
    wf::point_t parent_ws = {
        (int)std::floor(1.0 * center.x / scr_size.width),
        (int)std::floor(1.0 * center.y / scr_size.height),
    };

    auto workarea = view->get_output()->render->get_ws_box(
        view->get_output()->workspace->get_current_workspace() + parent_ws);
    if (view->parent->is_mapped())
    {
        auto parent_g = view->parent->get_wm_geometry();
        wm_geometry.x = parent_g.x + (parent_g.width - wm_geometry.width) / 2;
        wm_geometry.y = parent_g.y + (parent_g.height - wm_geometry.height) / 2;
    } else
    {
        /* if we have a parent which still isn't mapped, we cannot determine
         * the view's position, so we center it on the screen */
        wm_geometry.x = workarea.width / 2 - wm_geometry.width / 2;
        wm_geometry.y = workarea.height / 2 - wm_geometry.height / 2;
    }

    /* make sure view is visible afterwards */
    wm_geometry = wf::clamp(wm_geometry, workarea);
    view->move(wm_geometry.x, wm_geometry.y);
    if (wf::dimensions(wm_geometry) != wf::dimensions_t(wm_geometry))
    {
        view->resize(wm_geometry.width, wm_geometry.height);
    }
}

wf::wlr_view_t::wlr_view_t() : wf::view_interface_t(nullptr)
{
    auto dsurf = std::make_shared<wf::wlr_desktop_surface_t>();
    dsurf->view    = this;
    this->dsurface = dsurf.get();
    this->set_desktop_surface(dsurf);
}

void wf::wlr_view_t::handle_app_id_changed(std::string new_app_id)
{
    this->dsurface->app_id = new_app_id;

    app_id_changed_signal data;
    data.view = self();
    emit_signal("app-id-changed", &data);
}

void wf::wlr_view_t::handle_title_changed(std::string new_title)
{
    this->dsurface->title = new_title;

    title_changed_signal data;
    data.view = self();
    emit_signal("title-changed", &data);
}

std::string wf::wlr_desktop_surface_t::get_app_id()
{
    return this->app_id;
}

std::string wf::wlr_desktop_surface_t::get_title()
{
    return this->title;
}

void wf::wlr_view_t::handle_minimize_hint(wf::view_interface_t *relative_to,
    const wlr_box & hint)
{
    if (relative_to->get_output() != get_output())
    {
        LOGE("Minimize hint set to surface on a different output, "
             "problems might arise");
        /* TODO: translate coordinates in case minimize hint is on another output */
    }

    auto box = relative_to->get_output_geometry();
    box.x    += hint.x;
    box.y    += hint.y;
    box.width = hint.width;
    box.height = hint.height;

    set_minimize_hint(box);
}

void wf::wlr_view_t::set_position(int x, int y,
    wf::geometry_t old_geometry, bool send_signal)
{
    auto obox = get_output_geometry();
    auto wm   = get_wm_geometry();

    view_geometry_changed_signal data;
    data.view = self();
    data.old_geometry = old_geometry;

    view_damage_raw(self(), last_bounding_box);
    /* obox.x - wm.x is the current difference in the output and wm geometry */
    geometry.x = x + obox.x - wm.x;
    geometry.y = y + obox.y - wm.y;

    /* Make sure that if we move the view while it is unmapped, its snapshot
     * is still valid coordinates */
    if (view_impl->offscreen_buffer.valid())
    {
        view_impl->offscreen_buffer.geometry.x += x - data.old_geometry.x;
        view_impl->offscreen_buffer.geometry.y += y - data.old_geometry.y;
    }

    damage();

    if (send_signal)
    {
        emit_signal("geometry-changed", &data);
        wf::get_core().emit_signal("view-geometry-changed", &data);
        if (get_output())
        {
            get_output()->emit_signal("view-geometry-changed", &data);
        }
    }

    last_bounding_box = get_bounding_box();
}

void wf::wlr_view_t::move(int x, int y)
{
    set_position(x, y, get_wm_geometry(), true);
}

void wf::wlr_view_t::adjust_anchored_edge(wf::dimensions_t new_size)
{
    if (view_impl->edges)
    {
        auto wm = get_wm_geometry();
        if (view_impl->edges & WLR_EDGE_LEFT)
        {
            wm.x += geometry.width - new_size.width;
        }

        if (view_impl->edges & WLR_EDGE_TOP)
        {
            wm.y += geometry.height - new_size.height;
        }

        set_position(wm.x, wm.y,
            get_wm_geometry(), false);
    }
}

void wf::wlr_view_t::update_size()
{
    if (!is_mapped())
    {
        return;
    }

    auto current_size = get_main_surface()->output().get_size();
    if ((current_size.width == geometry.width) &&
        (current_size.height == geometry.height))
    {
        return;
    }

    /* Damage current size */
    view_damage_raw(self(), last_bounding_box);
    adjust_anchored_edge(current_size);

    view_geometry_changed_signal data;
    data.view = self();
    data.old_geometry = get_wm_geometry();

    geometry.width  = current_size.width;
    geometry.height = current_size.height;

    /* Damage new size */
    last_bounding_box = get_bounding_box();
    view_damage_raw(self(), last_bounding_box);
    emit_signal("geometry-changed", &data);
    wf::get_core().emit_signal("view-geometry-changed", &data);
    if (get_output())
    {
        get_output()->emit_signal("view-geometry-changed", &data);
    }

    if (view_impl->frame)
    {
        view_impl->frame->notify_view_resized(get_wm_geometry());
    }
}

bool wf::wlr_view_t::should_resize_client(
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

wf::geometry_t wf::wlr_view_t::get_output_geometry()
{
    return geometry;
}

wf::geometry_t wf::wlr_view_t::get_wm_geometry()
{
    if (view_impl->frame)
    {
        return view_impl->frame->expand_wm_geometry(geometry);
    } else
    {
        return geometry;
    }
}

bool wf::wlr_view_t::should_be_decorated()
{
    return (dsurface->get_role() == desktop_surface_t::role::TOPLEVEL) &&
           !has_client_decoration;
}

void wf::wlr_view_t::set_decoration_mode(bool use_csd)
{
    bool was_decorated = should_be_decorated();
    this->has_client_decoration = use_csd;
    if ((was_decorated != should_be_decorated()) && is_mapped())
    {
        wf::view_decoration_state_updated_signal data;
        data.view = self();

        this->emit_signal("decoration-state-updated", &data);
        if (get_output())
        {
            get_output()->emit_signal("view-decoration-state-updated", &data);
        }
    }
}

void wf::wlr_view_t::commit()
{
    update_size();

    /* Clear the resize edges.
     * This is must be done here because if the user(or plugin) resizes too fast,
     * the shell client might still haven't configured the surface, and in this
     * case the next commit(here) needs to still have access to the gravity */
    if (!view_impl->in_continuous_resize)
    {
        view_impl->edges = 0;
    }

    this->last_bounding_box = get_bounding_box();
}

void wf::wlr_view_t::map()
{
    auto surface = get_main_surface()->get_wlr_surface();
    if (wf::get_core_impl().uses_csd.count(surface))
    {
        this->has_client_decoration = wf::get_core_impl().uses_csd[surface];
    }

    dynamic_cast<wf::wlr_surface_base_t*>(get_main_surface().get())->map();
    update_size();

    if (dsurface->get_role() == desktop_surface_t::role::TOPLEVEL)
    {
        if (!parent)
        {
            get_output()->workspace->add_view(self(), wf::LAYER_WORKSPACE);
        }

        get_output()->focus_view(self(), true);
    }

    damage();
    this->emit_map();
    /* Might trigger repositioning */
    set_toplevel_parent(this->parent);
}

void wf::wlr_view_t::unmap()
{
    damage();
    emit_view_pre_unmap(self());
    set_decoration(nullptr);
    dynamic_cast<wf::wlr_surface_base_t*>(get_main_surface().get())->unmap();
    emit_view_unmap(self());
}

void wf::emit_view_map_signal(wayfire_view view, bool has_position)
{
    wf::view_mapped_signal data;
    data.view = view;
    data.is_positioned = has_position;
    view->get_output()->emit_signal("view-mapped", &data);
    view->emit_signal("mapped", &data);
}

void wf::emit_ping_timeout_signal(wayfire_view view)
{
    wf::view_ping_timeout_signal data;
    data.view = view;
    view->emit_signal("ping-timeout", &data);
}

void wf::wlr_view_t::emit_map()
{
    emit_view_map_signal(self(), false);
}

void wf::emit_view_map(wayfire_view view)
{
    emit_view_map_signal(view, false);
}

void wf::emit_view_unmap(wayfire_view view)
{
    view_unmapped_signal data;
    data.view = view;

    if (view->get_output())
    {
        view->get_output()->emit_signal("view-unmapped", &data);
        view->get_output()->emit_signal("view-disappeared", &data);
    }

    view->emit_signal("unmapped", &data);
}

void wf::emit_view_pre_unmap(wayfire_view view)
{
    view_pre_unmap_signal data;
    data.view = view;

    if (view->get_output())
    {
        view->get_output()->emit_signal("view-pre-unmapped", &data);
    }

    view->emit_signal("pre-unmapped", &data);
}

void wf::wlr_view_t::destroy()
{
    view_impl->is_alive = false;
    /* Drop the internal reference created in surface_interface_t */
    unref();
}

wf::point_t wf::wlr_view_t::get_window_offset()
{
    return {0, 0};
}

bool wf::wlr_desktop_surface_t::is_focuseable() const
{
    return keyboard_focus_enabled;
}

void wf::init_desktop_apis()
{
    init_xdg_shell();
    init_layer_shell();

    wf::option_wrapper_t<bool> xwayland_enabled("core/xwayland");
    if (xwayland_enabled == 1)
    {
        init_xwayland();
    }
}

wf::surface_interface_t*wf::wf_surface_from_void(void *handle)
{
    return static_cast<wf::surface_interface_t*>(handle);
}

wayfire_view wf::wl_surface_to_wayfire_view(wl_resource *resource)
{
    auto surface = (wlr_surface*)wl_resource_get_user_data(resource);

    void *handle = NULL;
    if (wlr_surface_is_xdg_surface(surface))
    {
        handle = wlr_xdg_surface_from_wlr_surface(surface)->data;
    }

    if (wlr_surface_is_layer_surface(surface))
    {
        handle = wlr_layer_surface_v1_from_wlr_surface(surface)->data;
    }

#if WF_HAS_XWAYLAND
    if (wlr_surface_is_xwayland_surface(surface))
    {
        handle = wlr_xwayland_surface_from_wlr_surface(surface)->data;
    }

#endif

    wf::view_interface_t *view = static_cast<wf::view_interface_t*>(handle);
    return view ? view->self() : nullptr;
}

void wf::view_interface_t::set_resizing(bool resizing, uint32_t edges)
{
    view_impl->update_windowed_geometry(self(), get_wm_geometry());
    /* edges are reset on the next commit */
    if (resizing)
    {
        this->view_impl->edges = edges;
    }

    auto& in_resize = this->view_impl->in_continuous_resize;
    in_resize += resizing ? 1 : -1;

    if (in_resize < 0)
    {
        LOGE("in_continuous_resize counter dropped below 0!");
    }
}

void wf::view_interface_t::set_moving(bool moving)
{
    view_impl->update_windowed_geometry(self(), get_wm_geometry());
    auto& in_move = this->view_impl->in_continuous_move;

    in_move += moving ? 1 : -1;
    if (in_move < 0)
    {
        LOGE("in_continuous_move counter dropped below 0!");
    }
}

void wf::view_interface_t::request_native_size()
{
    /* no-op */
}

wf::geometry_t wf::view_interface_t::get_wm_geometry()
{
    return get_output_geometry();
}

void wf::view_interface_t::set_minimized(bool minim)
{
    minimized = minim;
    if (minimized)
    {
        view_disappeared_signal data;
        data.view = self();
        get_output()->emit_signal("view-disappeared", &data);
        get_output()->workspace->add_view(self(), wf::LAYER_MINIMIZED);
    } else
    {
        get_output()->workspace->add_view(self(), wf::LAYER_WORKSPACE);
        get_output()->focus_view(self(), true);
    }

    view_minimized_signal data;
    data.view  = self();
    data.state = minimized;
    this->emit_signal("minimized", &data);
    get_output()->emit_signal("view-minimized", &data);
}

void wf::view_interface_t::set_tiled(uint32_t edges)
{
    if (edges)
    {
        view_impl->update_windowed_geometry(self(), get_wm_geometry());
    }

    wf::view_tiled_signal data;
    data.view = self();
    data.old_edges = this->tiled_edges;
    data.new_edges = edges;

    this->tiled_edges = edges;
    if (view_impl->frame)
    {
        view_impl->frame->notify_view_tiled();
    }

    this->emit_signal("tiled", &data);
    if (this->get_output())
    {
        get_output()->emit_signal("view-tiled", &data);
    }
}

void wf::view_interface_t::set_fullscreen(bool full)
{
    /* When fullscreening a view, we want to store the last geometry it had
     * before getting fullscreen so that we can restore to it */
    if (full && !fullscreen)
    {
        view_impl->update_windowed_geometry(self(), get_wm_geometry());
    }

    fullscreen = full;
    if (view_impl->frame)
    {
        view_impl->frame->notify_view_fullscreen();
    }

    view_fullscreen_signal data;
    data.view  = self();
    data.state = full;
    data.desired_size = {0, 0, 0, 0};

    if (get_output())
    {
        get_output()->emit_signal("view-fullscreen", &data);
    }

    this->emit_signal("fullscreen", &data);
}

void wf::view_interface_t::set_activated(bool active)
{
    if (view_impl->frame)
    {
        view_impl->frame->notify_view_activated(active);
    }

    activated = active;
}
