#pragma once

#include <wayfire/toplevel.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/util.hpp>

namespace wf
{
/**
 * The toplevel_t implementation for xdg-shell toplevels.
 */
class xdg_toplevel_t : public wf::toplevel_t
{
  private:
    toplevel_state_t _current;

    bool needs_decor;

    int moving_counter   = 0;
    int resizing_counter = 0;
    uint32_t resizing_edges = 0;

    uint32_t last_configure_serial;
    wf::point_t xdg_surface_offset = {0, 0};

    /** Last request to the client */
    wf::dimensions_t last_size_request = {0, 0};
    bool should_resize_client(wf::dimensions_t request,
        wf::dimensions_t current_size);

    bool has_xdg_decoration_csd = true;
    std::unique_ptr<decorator_frame_t_t> decorator;
    wlr_xdg_toplevel *toplevel;

    wf::wl_listener_wrapper on_commit;
    wf::wl_listener_wrapper on_destroy;
    wf::wl_listener_wrapper on_request_move;
    wf::wl_listener_wrapper on_request_resize;
    wf::wl_listener_wrapper on_request_minimize;
    wf::wl_listener_wrapper on_request_maximize;
    wf::wl_listener_wrapper on_request_fullscreen;

    void commit();
    void destroy();

  public:
    xdg_toplevel_t(wlr_xdg_toplevel *toplevel,
        wf::output_t *initial_output);

    bool should_be_decorated() final;
    toplevel_state_t& current() final;

    void set_minimized(bool minimized) final;
    void set_tiled(uint32_t edges) final;
    void set_fullscreen(bool fullscreen) final;
    void set_activated(bool active) final;
    void set_geometry(wf::geometry_t g) final;
    void move(int x, int y) final;

    void set_output(wf::output_t *new_output) final;
    void set_moving(bool moving) final;
    bool is_moving() final;
    void set_resizing(bool resizing, uint32_t edges = 0) final;
    bool is_resizing() final;
    void request_native_size() final;
    void set_decoration(std::unique_ptr<decorator_frame_t_t> frame) final;

    // private APIs for wayfire-core
    // Result of an outdated design which I don't want to update right now
    void set_decoration_mode(bool decorated);
};
}
