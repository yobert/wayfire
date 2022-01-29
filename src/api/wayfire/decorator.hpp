#pragma once

#include <wayfire/geometry.hpp>

namespace wf
{
struct decoration_margin_t
{
    int top;
    int bottom;
    int left;
    int right;
};

/**
 * In Wayland, window decorations (including titlebar, window frame, close
 * button, etc.) can be provided by the client - if 'client-side decorations'
 * are used, or by the compositor - 'server-side decorations'.
 *
 * Server-side decorations are implementable in Wayfire via custom
 * compositor-managed subsurfaces attached to the main surface. In addition,
 * plugins providing server-side decorations should implement the following
 * interface and attach a decorator to the toplevel_t they are decorating.
 *
 * The purpose of this decorator is twofold:
 * 1. It gives the toplevel information about the size of decorations, in order
 *   to manage the base and window manager geometries.
 * 2. It gives the decoration plugin the opportunity to be the first to react
 *   to state changes in the toplevel, thus ensuring that the decorations have
 *   reached a correct state when the regular signals for state change are
 *   emitted afterwards.
 */
class toplevel_decorator_t
{
  public:
    /**
     * Get the size of the decoration.
     */
    virtual decoration_margin_t get_margins() = 0;

    virtual void notify_activated(bool active)
    {}
    virtual void notify_resized(wf::geometry_t view_geometry)
    {}
    virtual void notify_tiled()
    {}
    virtual void notify_fullscreen()
    {}

    toplevel_decorator_t() = default;
    virtual ~toplevel_decorator_t() = default;
    toplevel_decorator_t(const toplevel_decorator_t &) = default;
    toplevel_decorator_t(toplevel_decorator_t &&) = default;
    toplevel_decorator_t& operator =(const toplevel_decorator_t&) = default;
    toplevel_decorator_t& operator =(toplevel_decorator_t&&) = default;
};
}
