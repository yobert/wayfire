#pragma once

#include "../wlr-desktop-surface.hpp"
#include <wayfire/nonstd/wlroots-full.hpp>

namespace wf
{
class xwayland_desktop_surface_t : public wlr_desktop_surface_t
{
  public:
    xwayland_desktop_surface_t(wlr_xwayland_surface *xw);

    void ping() final;
    void close() final;
    void destroy();

  private:
    wf::wl_listener_wrapper on_destroy;
    wf::wl_listener_wrapper on_set_title;
    wf::wl_listener_wrapper on_set_class;
    wf::wl_listener_wrapper on_ping_timeout;
    wf::wl_listener_wrapper on_map;

    wlr_xwayland_surface *xw;

    void update_kb_focus_enabled();
};
}
