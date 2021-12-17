#include <wayfire/util/log.hpp>
#include <wayfire/debug.hpp>
#include "wayfire/core.hpp"
#include "../surface-impl.hpp"
#include <wayfire/output.hpp>
#include "xdg-shell.hpp"
#include "xdg-toplevel.hpp"
#include "xdg-desktop-surface.hpp"
#include <wayfire/workspace-manager.hpp>
#include <wayfire/signal-definitions.hpp>

wayfire_xdg_popup::wayfire_xdg_popup(wlr_xdg_popup *popup)
{
    auto surf = std::make_shared<wf::wlr_surface_base_t>(popup->base->surface);
    set_main_surface(surf);

    auto parent_surface = wf::wf_surface_from_void(popup->parent->data);
    auto view = wf::get_core().find_views_with_surface(parent_surface).front();
    this->popup_parent = dynamic_cast<wlr_view_t*>(view.get());

    auto dsurf = std::make_shared<xdg_popup_dsurface_t>(
        popup, popup_parent->dsurf());
    this->set_desktop_surface(dsurf);

    this->popup = popup;
    this->set_output(popup_parent->get_output());

    LOGI("New xdg popup");
    on_map.set_callback([&] (void*)
    {
        map();
    });
    on_unmap.set_callback([&] (void*)
    {
        unmap();
    });
    on_destroy.set_callback([&] (void*)
    {
        destroy();
    });
    on_new_popup.set_callback([&] (void *data)
    {
        create_xdg_popup((wlr_xdg_popup*)data);
    });

    on_map.connect(&popup->base->events.map);
    on_unmap.connect(&popup->base->events.unmap);
    on_destroy.connect(&popup->base->events.destroy);
    on_new_popup.connect(&popup->base->events.new_popup);

    popup->base->data = this;
    parent_geometry_changed.set_callback([=] (wf::signal_data_t*)
    {
        this->update_position();
    });

    popup_parent->connect_signal("bounding-box-changed",
        &this->parent_geometry_changed);

    unconstrain();
}

void wayfire_xdg_popup::map()
{
    uint32_t parent_layer =
        get_output()->workspace->get_view_layer(popup_parent->self());

    wf::layer_t target_layer = wf::LAYER_UNMANAGED;
    if (parent_layer > wf::LAYER_WORKSPACE)
    {
        target_layer = (wf::layer_t)parent_layer;
    }

    get_output()->workspace->add_view(self(), target_layer);

    wlr_view_t::map();
    update_position();
}

void wayfire_xdg_popup::commit()
{
    wlr_view_t::commit();
    update_position();
}

void wayfire_xdg_popup::update_position()
{
    if (!popup_parent->is_mapped() || !is_mapped())
    {
        return;
    }

    wf::pointf_t popup_offset = {
        1.0 * popup->geometry.x + popup_parent->get_window_offset().x,
        1.0 * popup->geometry.y + popup_parent->get_window_offset().y,
    };

    auto parent_geometry = popup_parent->get_origin();
    popup_offset.x += parent_geometry.x - get_window_offset().x;
    popup_offset.y += parent_geometry.y - get_window_offset().y;

    popup_offset = popup_parent->transform_point(popup_offset);
    this->origin = {
        static_cast<int>(popup_offset.x),
        static_cast<int>(popup_offset.y)
    };
    update_bbox();
}

void wayfire_xdg_popup::unconstrain()
{
    wf::view_interface_t *toplevel_parent = this;
    while (true)
    {
        auto as_popup = dynamic_cast<wayfire_xdg_popup*>(toplevel_parent);
        if (as_popup)
        {
            toplevel_parent = as_popup->popup_parent;
        } else
        {
            break;
        }
    }

    if (!get_output() || !toplevel_parent)
    {
        return;
    }

    auto box = get_output()->get_relative_geometry();
    auto wm  = toplevel_parent->get_origin();
    box.x -= wm.x;
    box.y -= wm.y;

    wlr_xdg_popup_unconstrain_from_box(popup, &box);
}

void wayfire_xdg_popup::destroy()
{
    on_map.disconnect();
    on_unmap.disconnect();
    on_destroy.disconnect();
    on_new_popup.disconnect();

    wlr_view_t::destroy();
}

wf::point_t wayfire_xdg_popup::get_window_offset()
{
    return {
        popup->base->current.geometry.x,
        popup->base->current.geometry.y,
    };
}

void create_xdg_popup(wlr_xdg_popup *popup)
{
    auto parent = wf::wf_surface_from_void(popup->parent->data);
    if (!parent)
    {
        LOGE("attempting to create a popup with unknown parent");

        return;
    }

    wf::get_core().add_view(std::make_unique<wayfire_xdg_popup>(popup));
}

wayfire_xdg_view::wayfire_xdg_view(wlr_xdg_toplevel *top) :
    wf::wlr_view_t(), xdg_toplevel(top)
{
    auto surf = std::make_shared<wf::wlr_surface_base_t>(top->base->surface);
    set_main_surface(surf);

    auto dsurf = std::make_shared<xdg_toplevel_dsurface_t>(top);
    this->set_desktop_surface(dsurf);

    auto wo = wf::get_core().get_active_output();
    if (top->requested.fullscreen && top->requested.fullscreen_output)
    {
        wo = (wf::output_t*)top->requested.fullscreen_output->data;
    }

    auto tlvl = std::make_shared<wf::xdg_toplevel_t>(top, wo);
    this->set_toplevel(tlvl);
    this->setup_toplevel_tracking();

    LOGI("new xdg_shell_stable surface: ", xdg_toplevel->title,
        " app-id: ", xdg_toplevel->app_id);
    on_map.set_callback([&] (void*) { map(); });
    on_unmap.set_callback([&] (void*) { unmap(); });
    on_destroy.set_callback([&] (void*) { destroy(); });
    on_new_popup.set_callback([&] (void *data)
    {
        create_xdg_popup((decltype(xdg_toplevel->base->popup))data);
    });

    on_show_window_menu.set_callback([&] (void *data)
    {
        wlr_xdg_toplevel_show_window_menu_event *event =
            (wlr_xdg_toplevel_show_window_menu_event*)data;
        auto view   = self();
        auto output = view->get_output();
        if (!output)
        {
            return;
        }

        wf::view_show_window_menu_signal d;
        d.view = view;
        d.relative_position.x = event->x;
        d.relative_position.y = event->y;
        output->emit_signal("view-show-window-menu", &d);
        wf::get_core().emit_signal("view-show-window-menu", &d);
    });
    on_set_parent.set_callback([&] (void*)
    {
        auto parent = xdg_toplevel->parent ?
            static_cast<wf::view_interface_t*>(xdg_toplevel->parent->data) : nullptr;
        set_toplevel_parent(parent);
    });

    on_map.connect(&xdg_toplevel->base->events.map);
    on_unmap.connect(&xdg_toplevel->base->events.unmap);
    on_destroy.connect(&xdg_toplevel->base->events.destroy);
    on_new_popup.connect(&xdg_toplevel->base->events.new_popup);

    on_set_parent.connect(&xdg_toplevel->events.set_parent);
    on_show_window_menu.connect(&xdg_toplevel->events.request_show_window_menu);

    set_output(wo);
    xdg_toplevel->base->data = dynamic_cast<view_interface_t*>(this);
    // set initial parent
    on_set_parent.emit(nullptr);

    if (xdg_toplevel->requested.fullscreen)
    {
        tlvl->set_fullscreen(true);
        tlvl->set_geometry(wo->get_relative_geometry());
    }

    if (xdg_toplevel->requested.maximized)
    {
        tlvl->set_tiled(wf::TILED_EDGES_ALL);
        tlvl->set_geometry(wo->workspace->get_workarea());
    }
}

wf::point_t wayfire_xdg_view::get_window_offset()
{
    if (xdg_toplevel)
    {
        wlr_box box;
        wlr_xdg_surface_get_geometry(xdg_toplevel->base, &box);
        return wf::origin(box);
    }

    return {0, 0};
}

void wayfire_xdg_view::destroy()
{
    on_map.disconnect();
    on_unmap.disconnect();
    on_destroy.disconnect();
    on_new_popup.disconnect();
    on_set_parent.disconnect();
    on_show_window_menu.disconnect();

    xdg_toplevel = nullptr;
    wf::wlr_view_t::destroy();
}

static wlr_xdg_shell *xdg_handle = nullptr;

void wf::init_xdg_shell()
{
    static wf::wl_listener_wrapper on_xdg_created;
    xdg_handle = wlr_xdg_shell_create(wf::get_core().display);

    if (xdg_handle)
    {
        on_xdg_created.set_callback([&] (void *data)
        {
            auto surf = static_cast<wlr_xdg_surface*>(data);
            if (surf->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL)
            {
                wf::get_core().add_view(
                    std::make_unique<wayfire_xdg_view>(surf->toplevel));
            }
        });
        on_xdg_created.connect(&xdg_handle->events.new_surface);
    }
}
