#pragma once

#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/geometry.hpp>
#include <wayfire/view.hpp>
#include "../wlr-desktop-surface.hpp"
#include "../surface-impl.hpp"

class xdg_desktop_surface_t : public wlr_desktop_surface_t
{
  protected:
    xdg_desktop_surface_t(wlr_xdg_surface *surf);
    void ping() final;

    wf::wl_listener_wrapper on_destroy;
    wf::wl_listener_wrapper on_ping_timeout;
    wlr_xdg_surface *xdg_surface;

    virtual void destroy();
};

class xdg_toplevel_dsurface_t : public xdg_desktop_surface_t
{
  public:
    xdg_toplevel_dsurface_t(wlr_xdg_toplevel *top);

    void destroy() final;
    void close() final;

    wf::wl_listener_wrapper on_set_title;
    wf::wl_listener_wrapper on_set_app_id;
    wlr_xdg_toplevel *toplevel;
};

class xdg_popup_dsurface_t : public xdg_desktop_surface_t
{
  protected:
    wf::signal_connection_t parent_title_changed;
    wf::signal_connection_t parent_app_id_changed;

    wf::wl_idle_call pending_close;
    wlr_xdg_popup *popup;

  public:
    xdg_popup_dsurface_t(
        wlr_xdg_popup *popup,
        const wf::dsurface_sptr_t& parent);

    void destroy() final;
    void close() final;
};
