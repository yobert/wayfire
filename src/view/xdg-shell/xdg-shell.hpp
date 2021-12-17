#pragma once

#include "../view-impl.hpp"

class wayfire_xdg_popup : public wf::wlr_view_t
{
  protected:
    wf::wl_listener_wrapper on_destroy;
    wf::wl_listener_wrapper on_new_popup;
    wf::wl_listener_wrapper on_map;
    wf::wl_listener_wrapper on_unmap;
    wf::signal_connection_t parent_geometry_changed;

    wlr_xdg_popup *popup;
    void unconstrain();
    void update_position();

  public:
    wayfire_xdg_popup(wlr_xdg_popup *popup);

    wlr_view_t *popup_parent;
    void map() final;
    void commit() final;

    wf::point_t get_window_offset() final;
    void destroy() final;
};

void create_xdg_popup(wlr_xdg_popup *popup);

class wayfire_xdg_view : public wf::wlr_view_t
{
  private:
    wf::wl_listener_wrapper on_map;
    wf::wl_listener_wrapper on_unmap;
    wf::wl_listener_wrapper on_destroy;
    wf::wl_listener_wrapper on_new_popup;
    wf::wl_listener_wrapper on_set_parent;
    wf::wl_listener_wrapper on_show_window_menu;

    wlr_xdg_toplevel *xdg_toplevel;

  public:
    wayfire_xdg_view(wlr_xdg_toplevel *toplevel);
    wf::point_t get_window_offset() final;
    void destroy() final;
};
