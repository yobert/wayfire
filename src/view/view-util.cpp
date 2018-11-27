#include "priv-view.hpp"
#include "core.hpp"
#include "output.hpp"
#include <cstring>

extern "C"
{
#include <wlr/types/wlr_xdg_shell_v6.h>
#define namespace namespace_t
#include <wlr/types/wlr_layer_shell_v1.h>
#undef namespace
#include <wlr/types/wlr_xdg_shell.h>
#define class class_t
#define static
#include <wlr/xwayland.h>
#undef static
#undef class
}

bool operator == (const wf_geometry& a, const wf_geometry& b)
{
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

bool operator != (const wf_geometry& a, const wf_geometry& b)
{
    return !(a == b);
}

wf_point operator + (const wf_point& a, const wf_point& b)
{
    return {a.x + b.x, a.y + b.y};
}

wf_point operator + (const wf_point& a, const wf_geometry& b)
{
    return {a.x + b.x, a.y + b.y};
}

wf_geometry operator + (const wf_geometry &a, const wf_point& b)
{
    return {
        a.x + b.x,
        a.y + b.y,
        a.width,
        a.height
    };
}

wf_point operator - (const wf_point& a)
{
    return {-a.x, -a.y};
}

bool point_inside(wf_point point, wf_geometry rect)
{
    if(point.x < rect.x || point.y < rect.y)
        return false;

    if(point.x >= rect.x + rect.width)
        return false;

    if(point.y >= rect.y + rect.height)
        return false;

    return true;
}

bool rect_intersect(wf_geometry screen, wf_geometry win)
{
    if (win.x + (int32_t)win.width <= screen.x ||
        win.y + (int32_t)win.height <= screen.y)
        return false;

    if (screen.x + (int32_t)screen.width <= win.x ||
        screen.y + (int32_t)screen.height <= win.y)
        return false;
    return true;
}

wayfire_surface_t* wf_surface_from_void(void *handle)
{
    return static_cast<wayfire_surface_t*> (handle);
}

wayfire_view_t* wf_view_from_void(void *handle)
{
    return dynamic_cast<wayfire_view_t*> (wf_surface_from_void(handle));
}

wayfire_view wl_surface_to_wayfire_view(wl_resource *resource)
{
    auto surface = (wlr_surface*) wl_resource_get_user_data(resource);

    void *handle = NULL;

    if (wlr_surface_is_xdg_surface_v6(surface))
        handle = wlr_xdg_surface_v6_from_wlr_surface(surface)->data;

    if (wlr_surface_is_xdg_surface(surface))
        handle = wlr_xdg_surface_from_wlr_surface(surface)->data;

    if (wlr_surface_is_layer_surface(surface))
        handle = wlr_layer_surface_v1_from_wlr_surface(surface)->data;

    if (wlr_surface_is_xwayland_surface(surface))
        handle = wlr_xwayland_surface_from_wlr_surface(surface)->data;

    return core->find_view(wf_surface_from_void(handle));
}

wlr_box wlr_box_from_pixman_box(const pixman_box32_t& box)
{
    wlr_box d;
    d.x = box.x1;
    d.y = box.y1;
    d.width = box.x2 - box.x1;
    d.height = box.y2 - box.y1;

    return d;
}
