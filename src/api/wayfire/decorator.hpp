#ifndef DECORATOR_HPP
#define DECORATOR_HPP

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
 * A view decorator is used to create decorations around views.
 * It is used to tell the view about the size of the decorations, and also
 * provides callbacks for the decoration, so that it can be the first to react
 * to changes in the view state.
 */
class view_decorator_t
{
  public:
    /**
     * Get the size of the decoration.
     */
    virtual decoration_margin_t get_margins() = 0;

    virtual void notify_view_activated(bool active)
    {}
    virtual void notify_view_resized(wf::geometry_t view_geometry)
    {}
    virtual void notify_view_tiled()
    {}
    virtual void notify_view_fullscreen()
    {}

    view_decorator_t() = default;
    virtual ~view_decorator_t() = default;
    view_decorator_t(const view_decorator_t &) = default;
    view_decorator_t(view_decorator_t &&) = default;
    view_decorator_t& operator =(const view_decorator_t&) = default;
    view_decorator_t& operator =(view_decorator_t&&) = default;
};
}

#endif /* end of include guard: DECORATOR_HPP */
