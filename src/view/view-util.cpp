#include "priv-view.hpp"
#include "core.hpp"

extern "C"
{
#include <wlr/config.h>

#include <wlr/types/wlr_xdg_shell_v6.h>
#define namespace namespace_t
#include <wlr/types/wlr_layer_shell_v1.h>
#undef namespace
#include <wlr/types/wlr_xdg_shell.h>

#if WLR_HAS_XWAYLAND
#define class class_t
#define static
#include <wlr/xwayland.h>
#undef static
#undef class
#endif

}

wayfire_surface_t* wf_surface_from_void(void *handle)
{
    return static_cast<wayfire_surface_t*> (handle);
}

wayfire_view_t* wf_view_from_void(void *handle)
{
    return dynamic_cast<wayfire_view_t*> (wf_surface_from_void(handle));
}

wayfire_compositor_surface_t *wf_compositor_surface_from_surface(wayfire_surface_t *surface)
{
    return dynamic_cast<wayfire_compositor_surface_t*> (surface);
}

wayfire_compositor_interactive_view *interactive_view_from_view(wayfire_view_t *view)
{
    return dynamic_cast<wayfire_compositor_interactive_view*> (view);
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

#if WLR_HAS_XWAYLAND
    if (wlr_surface_is_xwayland_surface(surface))
        handle = wlr_xwayland_surface_from_wlr_surface(surface)->data;
#endif

    return core->find_view(wf_surface_from_void(handle));
}

