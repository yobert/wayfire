#include "xwayland-toplevel.hpp"

wf::xwayland_toplevel_t::xwayland_toplevel_t(
    wlr_xwayland_surface *xw, wf::output_t *initial_output)
{
    this->xw = xw;
    this->_current.primary_output = initial_output;
}

wf::toplevel_state_t& wf::xwayland_toplevel_t::current()
{
    return _current;
}

bool wf::xwayland_toplevel_t::should_be_decorated()
{}

void wf::xwayland_toplevel_t::set_minimized(bool minimized)
{}

void wf::xwayland_toplevel_t::set_tiled(uint32_t edges)
{}

void wf::xwayland_toplevel_t::set_fullscreen(bool fullscreen)
{}

void wf::xwayland_toplevel_t::set_activated(bool active)
{}

void wf::xwayland_toplevel_t::move(int x, int y)
{}

void wf::xwayland_toplevel_t::set_geometry(wf::geometry_t g)
{}

void wf::xwayland_toplevel_t::set_output(wf::output_t *new_output)
{}

void wf::xwayland_toplevel_t::set_moving(bool moving)
{}

bool wf::xwayland_toplevel_t::is_moving()
{}

void wf::xwayland_toplevel_t::set_resizing(bool resizing, uint32_t edges)
{}

bool wf::xwayland_toplevel_t::is_resizing()
{}

void wf::xwayland_toplevel_t::request_native_size()
{}

void wf::xwayland_toplevel_t::set_decoration(
    std::unique_ptr<decorator_frame_t_t> frame)
{}
