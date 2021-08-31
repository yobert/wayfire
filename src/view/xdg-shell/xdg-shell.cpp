#include <wayfire/util/log.hpp>
#include <wayfire/debug.hpp>
#include "wayfire/core.hpp"
#include "../surface-impl.hpp"
#include "wayfire/output.hpp"
#include "wayfire/decorator.hpp"
#include "wayfire/render-manager.hpp"
#include "xdg-shell.hpp"
#include "wayfire/output-layout.hpp"
#include <wayfire/workspace-manager.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/transaction/transaction.hpp>

wayfire_xdg_popup::wayfire_xdg_popup(wlr_xdg_popup *popup) :
    wf::wlr_view_t()
{
    this->popup_parent =
        dynamic_cast<wlr_view_t*>(wf::wf_surface_from_void(popup->parent->data));
    this->popup = popup;
    this->role  = wf::VIEW_ROLE_UNMANAGED;
    this->view_impl->keyboard_focus_enabled = false;
    this->set_output(popup_parent->get_output());
}

void wayfire_xdg_popup::initialize()
{
    LOGI("New xdg popup");
    on_map.set_callback([&] (void*) { map(this->popup->base->surface); });
    on_unmap.set_callback([&] (void*)
    {
        pending_close.disconnect();
        unmap();
    });
    on_destroy.set_callback([&] (void*) { destroy(); });
    on_new_popup.set_callback([&] (void *data)
    {
        create_xdg_popup((wlr_xdg_popup*)data);
    });
    on_ping_timeout.set_callback([&] (void*)
    {
        wf::emit_ping_timeout_signal(self());
    });

    on_map.connect(&popup->base->events.map);
    on_unmap.connect(&popup->base->events.unmap);
    on_destroy.connect(&popup->base->events.destroy);
    on_new_popup.connect(&popup->base->events.new_popup);
    on_ping_timeout.connect(&popup->base->events.ping_timeout);

    popup->base->data = this;
    parent_geometry_changed.set_callback([=] (wf::signal_data_t*)
    {
        this->update_position();
    });
    parent_app_id_changed.set_callback([=] (wf::signal_data_t*)
    {
        this->handle_app_id_changed(popup_parent->get_app_id());
    });
    parent_title_changed.set_callback([=] (wf::signal_data_t*)
    {
        this->handle_title_changed(popup_parent->get_title());
    });

    popup_parent->connect_signal("geometry-changed",
        &this->parent_geometry_changed);
    popup_parent->connect_signal("app-id-changed",
        &this->parent_app_id_changed);
    popup_parent->connect_signal("title-changed",
        &this->parent_title_changed);

    unconstrain();
}

void wayfire_xdg_popup::map(wlr_surface *surface)
{
    uint32_t parent_layer =
        get_output()->workspace->get_view_layer(popup_parent->self());

    wf::layer_t target_layer = wf::LAYER_UNMANAGED;
    if (parent_layer > wf::LAYER_WORKSPACE)
    {
        target_layer = (wf::layer_t)parent_layer;
    }

    get_output()->workspace->add_view(self(), target_layer);

    wlr_view_t::map(surface);
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

    auto parent_geometry = popup_parent->get_output_geometry();
    popup_offset.x += parent_geometry.x - get_window_offset().x;
    popup_offset.y += parent_geometry.y - get_window_offset().y;

    popup_offset = popup_parent->transform_point(popup_offset);
    this->move(popup_offset.x, popup_offset.y);
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
    auto wm  = toplevel_parent->get_output_geometry();
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
    on_ping_timeout.disconnect();

    wlr_view_t::destroy();
}

wf::point_t wayfire_xdg_popup::get_window_offset()
{
    return {
        popup->base->current.geometry.x,
        popup->base->current.geometry.y,
    };
}

void wayfire_xdg_popup::close()
{
    pending_close.run_once([=] ()
    {
        if (is_mapped())
        {
            wlr_xdg_popup_destroy(popup->base);
        }
    });
}

void wayfire_xdg_popup::ping()
{
    if (popup)
    {
        wlr_xdg_surface_ping(popup->base);
    }
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

#include "xdg-toplevel.hpp"

wayfire_xdg_view::wayfire_xdg_view(wlr_xdg_toplevel *top) :
    wf::wlr_view_t(), xdg_toplevel(top)
{}

std::unique_ptr<wf::txn::view_transaction_t> wayfire_xdg_view::next_state()
{
    using type = wf::view_impl_transaction_t<
        wayfire_xdg_view,
        xdg_view_geometry_t,
        xdg_view_gravity_t,
        xdg_view_state_t>;

    return std::make_unique<type>(this);
}

wf::geometry_t get_cached_xdg_geometry(wlr_xdg_toplevel *toplevel)
{
    if (toplevel->base->pending.has_geometry)
    {
        return toplevel->base->pending.geometry;
    }

    auto st = &toplevel->base->surface->pending;

    // FIXME: this is incorrect in case the view has subsurfaces, however,
    // these are rare occasions
    return {0, 0, st->width, st->height};
}

wf::geometry_t get_xdg_geometry(wlr_xdg_toplevel *toplevel)
{
    wlr_box xdg_geometry;
    wlr_xdg_surface_get_geometry(toplevel->base, &xdg_geometry);

    return xdg_geometry;
}

void wayfire_xdg_view::initialize()
{
    lockmgr = std::make_unique<wf::wlr_surface_manager_t>(
        xdg_toplevel->base->surface);
    wlr_view_t::initialize();
    LOGI("new xdg_shell_stable surface: ", xdg_toplevel->title,
        " app-id: ", xdg_toplevel->app_id);

    handle_title_changed(nonull(xdg_toplevel->title));
    handle_app_id_changed(nonull(xdg_toplevel->app_id));

    on_map.set_callback([&] (void*)
    {
        auto tx = wf::txn::transaction_t::create();
        tx->add_instruction(std::make_unique<xdg_view_map_t>(this));

        if ((pending().geometry.width == 0) ||
            (pending().geometry.height == 0))
        {
            // Add initial size
            wlr_box box;
            wlr_xdg_surface_get_geometry(xdg_toplevel->base, &box);
            auto ns = next_state();
            ns->set_geometry({100, 100, box.width, box.height});
            ns->schedule_in(tx);
        }

        wf::txn::transaction_manager_t::get().submit(std::move(tx));
    });
    on_unmap.set_callback([&] (void*)
    {
        this->emit_signal("__kill-tx", nullptr);

        if (state().mapped)
        {
            // If the map transaction did not succeed, we do not need to map
            // at all.
            auto tx = wf::txn::transaction_t::create();
            tx->add_instruction(std::make_unique<xdg_view_unmap_t>(this));
            wf::txn::transaction_manager_t::get().submit(std::move(tx));
        }
    });
    on_destroy.set_callback([&] (void*) { destroy(); });
    on_new_popup.set_callback([&] (void *data)
    {
        create_xdg_popup((decltype(xdg_toplevel->base->popup))data);
    });

    on_set_title.set_callback([&] (void*)
    {
        handle_title_changed(nonull(xdg_toplevel->title));
    });
    on_set_app_id.set_callback([&] (void*)
    {
        handle_app_id_changed(nonull(xdg_toplevel->app_id));
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
            wf::wf_view_from_void(xdg_toplevel->parent->data)->self() : nullptr;
        set_toplevel_parent(parent);
    });
    on_ping_timeout.set_callback([&] (void*)
    {
        wf::emit_ping_timeout_signal(self());
    });

    on_request_move.set_callback([&] (void*) { move_request(); });
    on_request_resize.set_callback([&] (auto data)
    {
        auto ev = static_cast<wlr_xdg_toplevel_resize_event*>(data);
        resize_request(ev->edges);
    });
    on_request_minimize.set_callback([&] (void*) { minimize_request(true); });
    on_request_maximize.set_callback([&] (void *data)
    {
        tile_request(xdg_toplevel->requested.maximized ?
            wf::TILED_EDGES_ALL : 0);
    });
    on_request_fullscreen.set_callback([&] (void *data)
    {
        auto ev = static_cast<wlr_xdg_toplevel_set_fullscreen_event*>(data);
        auto wo = wf::get_core().output_layout->find_output(ev->output);
        fullscreen_request(wo, ev->fullscreen);
    });

    on_map.connect(&xdg_toplevel->base->events.map);
    on_unmap.connect(&xdg_toplevel->base->events.unmap);
    on_destroy.connect(&xdg_toplevel->base->events.destroy);
    on_new_popup.connect(&xdg_toplevel->base->events.new_popup);
    on_ping_timeout.connect(&xdg_toplevel->base->events.ping_timeout);

    on_set_title.connect(&xdg_toplevel->events.set_title);
    on_set_app_id.connect(&xdg_toplevel->events.set_app_id);
    on_set_parent.connect(&xdg_toplevel->events.set_parent);
    on_request_move.connect(&xdg_toplevel->events.request_move);
    on_request_resize.connect(&xdg_toplevel->events.request_resize);
    on_request_maximize.connect(&xdg_toplevel->events.request_maximize);
    on_request_minimize.connect(&xdg_toplevel->events.request_minimize);
    on_show_window_menu.connect(&xdg_toplevel->events.request_show_window_menu);
    on_request_fullscreen.connect(&xdg_toplevel->events.request_fullscreen);

    // Lock the initial surface state
    on_precommit.set_callback([&] (void*) { handle_precommit(); });
    on_precommit.connect(&xdg_toplevel->base->surface->events.precommit);

    xdg_toplevel->base->data = dynamic_cast<view_interface_t*>(this);
    // set initial parent
    on_set_parent.emit(nullptr);

    if (xdg_toplevel->requested.fullscreen)
    {
        fullscreen_request(get_output(), true);
    }

    if (xdg_toplevel->requested.maximized)
    {
        next_state()
            ->set_tiled(wf::TILED_EDGES_ALL)
            ->set_geometry(get_output()->workspace->get_workarea())
            ->submit();
    }
}

wayfire_xdg_view::~wayfire_xdg_view()
{}

void wayfire_xdg_view::handle_precommit()
{
    if (!is_mapped())
    {
        // Size will be set on map
        return;
    }

    if (lockmgr->current_lock() != 0)
    {
        // A transaction is holding a lock on the view, so it is responsible
        // for managing the view.
        return;
    }

    auto surface = get_wlr_surface();

    auto cached_geometry  = get_cached_xdg_geometry(xdg_toplevel);
    auto current_geometry = get_xdg_geometry(xdg_toplevel);
    bool surface_changed  = (cached_geometry != current_geometry);

    auto current = &surface->current;
    auto pending = &surface->pending;
    surface_changed |= pending->width != current->width;
    surface_changed |= pending->height != current->height;

    bool position_changed = false;
    position_changed |= pending->dx != 0;
    position_changed |= pending->dy != 0;

    if (!surface_changed && !position_changed)
    {
        // Allow next commit
        return;
    }

    wf::geometry_t target = state().geometry;
    if (position_changed)
    {
        target.x    += pending->dx;
        target.y    += pending->dy;
        target.width = cached_geometry.width;
        target.height = cached_geometry.height;

        if (view_impl->frame)
        {
            auto margins =
                view_impl->frame->get_margins();

            target.width  += margins.left + margins.right;
            target.height += margins.top + margins.bottom;
        }
    } else
    {
        if (view_impl->frame)
        {
            cached_geometry = wf::expand_with_margins(cached_geometry,
                view_impl->frame->get_margins());
        }

        target = wf::align_with_gravity(
            target, cached_geometry, state().gravity);
    }

    // Grab a lock now, otherwise, wlroots will do the commit
    lockmgr->lock();

    auto tx = wf::txn::transaction_t::create();
    tx->add_instruction(
        std::make_unique<xdg_view_geometry_t>(this, target, true));
    wf::txn::transaction_manager_t::get().submit(std::move(tx));
}

void wayfire_xdg_view::map(wlr_surface *surface)
{
    wlr_view_t::map(surface);
    create_toplevel();
}

bool wayfire_xdg_view::is_mapped() const
{
    return pending().mapped;
}

void wayfire_xdg_view::commit()
{
    wlr_view_t::commit();
}

wf::point_t wayfire_xdg_view::get_window_offset()
{
    return xdg_surface_offset;
}

wf::geometry_t wayfire_xdg_view::get_wm_geometry()
{
    return pending().geometry;

// auto output_g     = get_output_geometry();
// auto xdg_geometry = get_xdg_geometry(xdg_toplevel);
//
// wf::geometry_t wm = {
// .x     = output_g.x + xdg_surface_offset.x,
// .y     = output_g.y + xdg_surface_offset.y,
// .width = xdg_geometry.width,
// .height = xdg_geometry.height
// };
//
// if (view_impl->frame)
// {
// wm = view_impl->frame->expand_wm_geometry(wm);
// }
//
// return wm;
}

void wayfire_xdg_view::set_activated(bool act)
{
    /* we don't send activated or deactivated for shell views,
     * they should always be active */
    if (this->role == wf::VIEW_ROLE_DESKTOP_ENVIRONMENT)
    {
        act = true;
    }

    last_configure_serial =
        wlr_xdg_toplevel_set_activated(xdg_toplevel->base, act);
    wf::wlr_view_t::set_activated(act);
    wlr_surface_from_resource
}

void wayfire_xdg_view::set_fullscreen(bool full)
{
    wf::wlr_view_t::set_fullscreen(full);
    last_configure_serial =
        wlr_xdg_toplevel_set_fullscreen(xdg_toplevel->base, full);
}

void wayfire_xdg_view::move(int x, int y)
{
    set_geometry({x, y, pending().geometry.width, pending().geometry.height});
}

void wayfire_xdg_view::set_geometry(wf::geometry_t g)
{
    if (g == pending().geometry)
    {
        return;
    }

    auto ns = next_state();
    ns->set_geometry(g);
    ns->submit();
}

void wayfire_xdg_view::resize(int w, int h)
{
    set_geometry({get_wm_geometry().x, get_wm_geometry().y, w, h});
}

void wayfire_xdg_view::request_native_size()
{
    LOGI("request native size!");
    last_configure_serial =
        wlr_xdg_toplevel_set_size(xdg_toplevel->base, 0, 0);
}

void wayfire_xdg_view::close()
{
    if (xdg_toplevel)
    {
        wlr_xdg_toplevel_send_close(xdg_toplevel->base);
        wf::wlr_view_t::close();
    }
}

void wayfire_xdg_view::ping()
{
    if (xdg_toplevel)
    {
        wlr_xdg_surface_ping(xdg_toplevel->base);
    }
}

wlr_surface*wayfire_xdg_view::get_wlr_surface()
{
    if (xdg_toplevel)
    {
        return xdg_toplevel->base->surface;
    }

    return nullptr;
}

void wayfire_xdg_view::destroy()
{
    on_map.disconnect();
    on_unmap.disconnect();
    on_destroy.disconnect();
    on_precommit.disconnect();
    on_new_popup.disconnect();
    on_set_title.disconnect();
    on_set_app_id.disconnect();
    on_set_parent.disconnect();
    on_ping_timeout.disconnect();
    on_request_move.disconnect();
    on_request_resize.disconnect();
    on_request_maximize.disconnect();
    on_request_minimize.disconnect();
    on_show_window_menu.disconnect();
    on_request_fullscreen.disconnect();

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
