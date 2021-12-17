#include <wayfire/signal-definitions.hpp>
#include "xdg-desktop-surface.hpp"
#include "../view-impl.hpp"
#include <wayfire/debug.hpp>
#include <wayfire/core.hpp>

xdg_desktop_surface_t::xdg_desktop_surface_t(wlr_xdg_surface *surf) :
    wlr_desktop_surface_t(surf->surface)
{
    this->xdg_surface = surf;
    on_ping_timeout.set_callback([=] (void*)
    {
        wf::emit_ping_timeout_signal(this);
    });
    on_ping_timeout.connect(&surf->events.ping_timeout);

    on_destroy.set_callback([=] (void*)
    {
        destroy();
    });
}

void xdg_desktop_surface_t::destroy()
{
    // Make inert
    this->xdg_surface = NULL;
    on_ping_timeout.disconnect();
    on_destroy.disconnect();
}

void xdg_desktop_surface_t::ping()
{
    if (xdg_surface)
    {
        wlr_xdg_surface_ping(xdg_surface);
    }
}

xdg_toplevel_dsurface_t::xdg_toplevel_dsurface_t(wlr_xdg_toplevel *toplevel) :
    xdg_desktop_surface_t(toplevel->base)
{
    this->toplevel = toplevel;
    on_set_title.set_callback([=] (void*)
    {
        set_title(nonull(toplevel->title));
    });
    on_set_title.connect(&toplevel->events.set_title);
    on_set_title.emit(NULL); // initial title

    on_set_app_id.set_callback([=] (void*)
    {
        set_app_id(nonull(toplevel->app_id));
    });
    on_set_app_id.connect(&toplevel->events.set_app_id);
    on_set_app_id.emit(NULL); // initial app-id
}

void xdg_toplevel_dsurface_t::close()
{
    if (toplevel)
    {
        wlr_xdg_toplevel_send_close(toplevel->base);
    }
}

void xdg_toplevel_dsurface_t::destroy()
{
    this->toplevel = NULL;
    on_set_title.disconnect();
    on_set_app_id.disconnect();

    xdg_desktop_surface_t::destroy();
}

xdg_popup_dsurface_t::xdg_popup_dsurface_t(wlr_xdg_popup *popup,
    const wf::dsurface_sptr_t& parent) :
    xdg_desktop_surface_t(popup->base)
{
    this->popup = popup;

    this->current_role = wf::desktop_surface_t::role::UNMANAGED;
    this->keyboard_focus_enabled = false;

    parent_app_id_changed.set_callback([=] (wf::signal_data_t*)
    {
        this->set_app_id(parent->get_app_id());
    });
    parent_title_changed.set_callback([=] (wf::signal_data_t*)
    {
        this->set_title(parent->get_title());
    });
    parent->connect_signal("app-id-changed", &this->parent_app_id_changed);
    parent->connect_signal("title-changed", &this->parent_title_changed);
}

void xdg_popup_dsurface_t::destroy()
{
    this->popup = NULL;
    parent_app_id_changed.disconnect();
    parent_title_changed.disconnect();
    xdg_desktop_surface_t::destroy();
}

void xdg_popup_dsurface_t::close()
{
    pending_close.run_once([=] ()
    {
        if (popup)
        {
            wlr_xdg_popup_destroy(popup->base);
        }
    });
}
