#include "xwayland-desktop-surface.hpp"
#include <wayfire/debug.hpp>
#include "../view-impl.hpp"

wf::xwayland_desktop_surface_t::xwayland_desktop_surface_t(
    wlr_xwayland_surface *xw) : wlr_desktop_surface_t(xw->surface)
{
    this->xw = xw;

    on_destroy.set_callback([&] (void*)
    {
        destroy();
    });
    on_set_title.set_callback([&] (void*)
    {
        set_title(nonull(xw->title));
    });
    on_set_class.set_callback([&] (void*)
    {
        set_app_id(nonull(xw->class_t));
    });
    on_ping_timeout.set_callback([&] (void*)
    {
        wf::emit_ping_timeout_signal(this);
    });

    on_destroy.connect(&xw->events.destroy);
    on_set_title.connect(&xw->events.set_title);
    on_set_class.connect(&xw->events.set_class);
    on_ping_timeout.connect(&xw->events.ping_timeout);
}

void wf::xwayland_desktop_surface_t::ping()
{
    if (xw)
    {
        wlr_xwayland_surface_ping(xw);
    }
}

void wf::xwayland_desktop_surface_t::close()
{
    if (xw)
    {
        wlr_xwayland_surface_close(xw);
    }
}

void wf::xwayland_desktop_surface_t::destroy()
{
    this->xw = NULL;

    on_destroy.disconnect();
    on_set_title.disconnect();
    on_set_class.disconnect();
    on_ping_timeout.disconnect();
}
