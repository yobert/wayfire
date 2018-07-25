#include "priv-view.hpp"
#include "xdg-shell-v6.hpp"
#include "debug.hpp"
#include "core.hpp"
#include "decorator.hpp"

static void handle_v6_new_popup(wl_listener*, void*);
static void handle_v6_map(wl_listener*, void *data);
static void handle_v6_unmap(wl_listener*, void *data);
static void handle_v6_destroy(wl_listener*, void *data);
static void handle_v6_popup_destroy(wl_listener*, void *data);

/* TODO: Figure out a way to animate this */
wayfire_xdg6_popup::wayfire_xdg6_popup(wlr_xdg_popup_v6 *popup)
    :wayfire_surface_t(wf_surface_from_void(popup->parent->surface->data))
{
    assert(parent_surface);
    this->popup = popup;

    destroy.notify       = handle_v6_popup_destroy;
    new_popup.notify     = handle_v6_new_popup;
    m_popup_map.notify   = handle_v6_map;
    m_popup_unmap.notify = handle_v6_unmap;

    wl_signal_add(&popup->base->events.new_popup, &new_popup);
    wl_signal_add(&popup->base->events.map,       &m_popup_map);
    wl_signal_add(&popup->base->events.unmap,     &m_popup_unmap);
    wl_signal_add(&popup->base->events.destroy,   &destroy);

    popup->base->data = this;
}

wayfire_xdg6_popup::~wayfire_xdg6_popup()
{
    wl_list_remove(&new_popup.link);
    wl_list_remove(&m_popup_map.link);
    wl_list_remove(&m_popup_unmap.link);
    wl_list_remove(&destroy.link);
}

void wayfire_xdg6_popup::get_child_position(int &x, int &y)
{
    struct wlr_xdg_surface_v6 *parent = popup->parent;
    x = parent->geometry.x + popup->geometry.x - popup->base->geometry.x;
    y = parent->geometry.y + popup->geometry.y - popup->base->geometry.y;
}

void handle_v6_new_popup(wl_listener*, void *data)
{
    auto popup = static_cast<wlr_xdg_popup_v6*> (data);
    auto parent = wf_surface_from_void(popup->parent->surface->data);
    if (!parent)
    {
        log_error("attempting to create a popup with unknown parent");
        return;
    }

    new wayfire_xdg6_popup(popup);
}

static void handle_v6_map(wl_listener*, void *data)
{
    auto surface = static_cast<wlr_xdg_surface_v6*> (data);
    auto wf_surface = wf_surface_from_void(surface->data);

    assert(wf_surface);
    wf_surface->map(surface->surface);
}

static void handle_v6_unmap(wl_listener*, void *data)
{
    auto surface = static_cast<wlr_xdg_surface_v6*> (data);
    auto wf_surface = wf_surface_from_void(surface->data);

    assert(wf_surface);
    wf_surface->unmap();
}

static void handle_v6_popup_destroy(wl_listener*, void *data)
{
    auto surface = static_cast<wlr_xdg_surface_v6*> (data);
    auto wf_surface = wf_surface_from_void(surface->data);

    assert(wf_surface);
    wf_surface->destroyed = 1;
    wf_surface->dec_keep_count();
}

static void handle_v6_destroy(wl_listener*, void *data)
{
    auto surface = static_cast<wlr_xdg_surface_v6*> (data);
    auto view = wf_view_from_void(surface->data);

    assert(view);
    view->destroy();
}

static void handle_v6_request_move(wl_listener*, void *data)
{
    auto ev = static_cast<wlr_xdg_toplevel_v6_move_event*> (data);
    auto view = wf_view_from_void(ev->surface->data);
    view->move_request();
}

static void handle_v6_request_resize(wl_listener*, void *data)
{
    auto ev = static_cast<wlr_xdg_toplevel_v6_resize_event*> (data);
    auto view = wf_view_from_void(ev->surface->data);
    view->resize_request();
}

static void handle_v6_request_maximized(wl_listener*, void *data)
{
    auto surf = static_cast<wlr_xdg_surface_v6*> (data);
    auto view = wf_view_from_void(surf->data);
    view->maximize_request(surf->toplevel->client_pending.maximized);
}

static void handle_v6_request_fullscreen(wl_listener*, void *data)
{
    auto ev = static_cast<wlr_xdg_toplevel_v6_set_fullscreen_event*> (data);
    auto view = wf_view_from_void(ev->surface->data);
    auto wo = core->get_output(ev->output);
    view->fullscreen_request(wo, ev->fullscreen);
}

void handle_v6_set_parent(wl_listener* listener, void *data)
{
    auto surface = static_cast<wlr_xdg_surface_v6*> (data);
    auto view = wf_view_from_void(surface->data);
    auto parent = surface->toplevel->parent ?
        wf_view_from_void(surface->toplevel->parent->data)->self() : nullptr;

    assert(view);
    view->set_toplevel_parent(parent);
}

wayfire_xdg6_view::wayfire_xdg6_view(wlr_xdg_surface_v6 *s)
    : wayfire_view_t(), v6_surface(s)
{
    log_info ("new xdg_shell_v6 surface: %s app-id: %s",
              nonull(v6_surface->toplevel->title),
              nonull(v6_surface->toplevel->app_id));

    destroy_ev.notify         = handle_v6_destroy;
    new_popup.notify          = handle_v6_new_popup;
    map_ev.notify             = handle_v6_map;
    unmap_ev.notify           = handle_v6_unmap;
    set_parent_ev.notify      = handle_v6_set_parent;
    request_move.notify       = handle_v6_request_move;
    request_resize.notify     = handle_v6_request_resize;
    request_maximize.notify   = handle_v6_request_maximized;
    request_fullscreen.notify = handle_v6_request_fullscreen;

    wlr_xdg_surface_v6_ping(s);

    wl_signal_add(&v6_surface->events.destroy, &destroy_ev);
    wl_signal_add(&s->events.new_popup,        &new_popup);
    wl_signal_add(&v6_surface->events.map,     &map_ev);
    wl_signal_add(&v6_surface->events.unmap,   &unmap_ev);
    wl_signal_add(&v6_surface->toplevel->events.set_parent,         &set_parent_ev);
    wl_signal_add(&v6_surface->toplevel->events.request_move,       &request_move);
    wl_signal_add(&v6_surface->toplevel->events.request_resize,     &request_resize);
    wl_signal_add(&v6_surface->toplevel->events.request_maximize,   &request_maximize);
    wl_signal_add(&v6_surface->toplevel->events.request_fullscreen, &request_fullscreen);

    v6_surface->data = this;
}

void wayfire_xdg6_view::commit()
{
    wayfire_view_t::commit();

    if (v6_surface->geometry.x != xdg_surface_offset.x ||
        v6_surface->geometry.y != xdg_surface_offset.y)
    {
        move(geometry.x + xdg_surface_offset.x, geometry.y + xdg_surface_offset.y, false);
        xdg_surface_offset = {v6_surface->geometry.x, v6_surface->geometry.y};
    }
}

void wayfire_xdg6_view::map(wlr_surface *surface)
{
    if (v6_surface->toplevel->client_pending.maximized)
        maximize_request(true);

    if (v6_surface->toplevel->client_pending.fullscreen)
        fullscreen_request(output, true);

    if (v6_surface->toplevel->parent)
    {
        auto parent = wf_view_from_void(v6_surface->toplevel->parent->data)->self();
        set_toplevel_parent(parent);
    }

    wayfire_view_t::map(surface);
    xdg_surface_offset = {v6_surface->geometry.x, v6_surface->geometry.y};
}

void wayfire_xdg6_view::get_child_offset(int &x, int &y)
{
    x = xdg_surface_offset.x;
    y = xdg_surface_offset.y;
}

wf_geometry wayfire_xdg6_view::get_wm_geometry()
{
    wf_geometry wm;
    if (!v6_surface || !v6_surface->geometry.width || !v6_surface->geometry.height)
    {
        wm = get_output_geometry();
    } else
    {
        auto opos = get_output_position();
        wm = {
            v6_surface->geometry.x + opos.x,
            v6_surface->geometry.y + opos.y,
            v6_surface->geometry.width,
            v6_surface->geometry.height
        };
    }

    if (frame)
        wm = frame->expand_wm_geometry(wm);

    return wm;
}

void wayfire_xdg6_view::activate(bool act)
{
    wayfire_view_t::activate(act);
    wlr_xdg_toplevel_v6_set_activated(v6_surface, act);
}

void wayfire_xdg6_view::set_maximized(bool max)
{
    wayfire_view_t::set_maximized(max);
    wlr_xdg_toplevel_v6_set_maximized(v6_surface, max);
}

void wayfire_xdg6_view::set_fullscreen(bool full)
{
    wayfire_view_t::set_fullscreen(full);
    wlr_xdg_toplevel_v6_set_fullscreen(v6_surface, full);
}

void wayfire_xdg6_view::move(int w, int h, bool send)
{
    wayfire_view_t::move(w, h, send);
}

void wayfire_xdg6_view::resize(int w, int h, bool send)
{
    damage();
    if (frame)
        frame->calculate_resize_size(w, h);

    wlr_xdg_toplevel_v6_set_size(v6_surface, w, h);
}

std::string wayfire_xdg6_view::get_app_id()
{
    return nonull(v6_surface->toplevel->app_id);
}

std::string wayfire_xdg6_view::get_title()
{
    return nonull(v6_surface->toplevel->title);
}

void wayfire_xdg6_view::close()
{
    wlr_xdg_surface_v6_send_close(v6_surface);
}

wayfire_xdg6_view::~wayfire_xdg6_view()
{ }

void wayfire_xdg6_view::destroy()
{
    wl_list_remove(&destroy_ev.link);
    wl_list_remove(&new_popup.link);
    wl_list_remove(&map_ev.link);
    wl_list_remove(&unmap_ev.link);
    wl_list_remove(&request_move.link);
    wl_list_remove(&request_resize.link);
    wl_list_remove(&request_maximize.link);
    wl_list_remove(&request_fullscreen.link);
    wl_list_remove(&set_parent_ev.link);

    v6_surface = nullptr;
    wayfire_view_t::destroy();
}

static void notify_v6_created(wl_listener*, void *data)
{
    auto surf = static_cast<wlr_xdg_surface_v6*> (data);
    if (surf->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL)
        core->add_view(nonstd::make_unique<wayfire_xdg6_view> (surf));
}

static wlr_xdg_shell_v6 *v6_handle;
static wl_listener v6_created;

void init_xdg_shell_v6()
{
    v6_created.notify = notify_v6_created;
    v6_handle = wlr_xdg_shell_v6_create(core->display);
    wl_signal_add(&v6_handle->events.new_surface, &v6_created);
}
