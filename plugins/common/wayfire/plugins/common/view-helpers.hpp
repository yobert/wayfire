#pragma once

#include <wayfire/view.hpp>

namespace wf
{
/**
 * Check whether the given view is a toplevel view.
 */
inline bool is_view_toplevel(wayfire_view view)
{
    if (!view)
    {
        return false;
    }

    if (view->dsurf()->get_role() != desktop_surface_t::role::TOPLEVEL)
    {
        return false;
    }

    return true;
}

inline bool is_view_desktop_environment(wayfire_view view)
{
    if (!view)
    {
        return false;
    }

    if (view->dsurf()->get_role() != desktop_surface_t::role::DESKTOP_ENVIRONMENT)
    {
        return false;
    }

    return true;
}
}
