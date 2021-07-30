#include <wayfire/util/log.hpp>
#include <wayfire/debug.hpp>
#include "wayfire/core.hpp"
#include "surface-impl.hpp"
#include "wayfire/output.hpp"
#include "wayfire/decorator.hpp"
#include "xdg-shell.hpp"
#include "wayfire/output-layout.hpp"
#include <wayfire/workspace-manager.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/transaction/instruction.hpp>
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
        popup->base->geometry.x,
        popup->base->geometry.y,
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

class xdg_view_geometry_t : public wf::txn::instruction_t
{
    wayfire_xdg_view *view;
    wf::geometry_t target;
    uint32_t lock_id;
    wf::gravity_t current_gravity;

    wf::wl_listener_wrapper on_cache;

  public:
    xdg_view_geometry_t(wayfire_xdg_view *view, const wf::geometry_t& g)
    {
        this->target = g;
        this->view   = view;
        view->take_ref();
    }

    ~xdg_view_geometry_t()
    {
        view->lockmgr->unlock_all(lock_id);
        view->unref();
    }

    std::string get_object() override
    {
        return view->to_string();
    }

    void set_pending() override
    {
        LOGC(TXNV, "Pending: set geometry of ", wayfire_view{view}, " to ", target);
        current_gravity = view->view_impl->pending.gravity;
        view->view_impl->pending.geometry = target;
    }

    void commit() override
    {
        if (!view->xdg_toplevel)
        {
            wf::txn::emit_instruction_signal(this, "ready");
            return;
        }

        auto cfg_geometry = target;
        if (view->view_impl->frame)
        {
            cfg_geometry = wf::shrink_by_margins(cfg_geometry,
                view->view_impl->frame->get_margins());
        }

        auto& sp = view->xdg_toplevel->server_pending;
        if (((int)sp.width == cfg_geometry.width) &&
            ((int)sp.height == cfg_geometry.height))
        {
            wf::txn::emit_instruction_signal(this, "ready");
            return;
        }

        LOGI("Wanting ", cfg_geometry.width, cfg_geometry.height, " from ",
            view->xdg_toplevel->base->surface);

        lock_id = view->lockmgr->lock();
        auto serial = wlr_xdg_toplevel_set_size(view->xdg_toplevel->base,
            cfg_geometry.width, cfg_geometry.height);
        wf::surface_send_frame(view->xdg_toplevel->base->surface);

        LOGI("Serial is ", serial);

        on_cache.set_callback([this, serial] (void*)
        {
            wf::surface_send_frame(view->xdg_toplevel->base->surface);
            wlr_xdg_surface_state *state;
            wl_list_for_each(state, &view->xdg_toplevel->base->cached,
                cached_state_link)
            {
                LOGI("Cached is ", state->configure_serial);
                if (state->configure_serial == serial)
                {
                    wf::txn::emit_instruction_signal(this, "ready");
                    return;
                }
            }
        });
        on_cache.connect(&view->xdg_toplevel->base->surface->events.cache);
    }

    void apply() override
    {
        view->damage();

        view->lockmgr->unlock(lock_id);

        wlr_box box;
        wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &box);

        if (view->view_impl->frame)
        {
            box = wf::expand_with_margins(box,
                view->view_impl->frame->get_margins());
        }

        // Adjust for gravity
        if ((current_gravity == wf::gravity_t::TOP_RIGHT) ||
            (current_gravity == wf::gravity_t::BOTTOM_RIGHT))
        {
            target.x = (target.x + target.width) - box.width;
        }

        if ((current_gravity == wf::gravity_t::BOTTOM_LEFT) ||
            (current_gravity == wf::gravity_t::BOTTOM_RIGHT))
        {
            target.y = (target.y + target.height) - box.height;
        }

        target.width  = box.width;
        target.height = box.height;
        view->view_impl->state.geometry = target;

        LOGI("setting geometry ", target);

        // Adjust output geometry for shadows and other parts of the surface
        target.x    -= box.x;
        target.y    -= box.y;
        target.width = view->get_wlr_surface()->current.width;
        target.height  = view->get_wlr_surface()->current.height;
        view->geometry = target;

        LOGI("Setting output g", target);

        view->damage();
    }
};

class xdg_view_gravity_t : public wf::txn::instruction_t
{
    wayfire_xdg_view *view;
    wf::gravity_t g;

  public:
    xdg_view_gravity_t(wayfire_xdg_view *view, wf::gravity_t g)
    {
        this->g    = g;
        this->view = view;
        view->take_ref();
    }

    ~xdg_view_gravity_t()
    {
        view->unref();
    }

    std::string get_object() override
    {
        return view->to_string();
    }

    void set_pending() override
    {
        LOGC(TXNV, "Pending: set gravity of ", wayfire_view{view}, " to ", (int)g);
        view->view_impl->pending.gravity = g;
    }

    void commit() override
    {
        wf::txn::emit_instruction_signal(this, "ready");
    }

    void apply() override
    {
        view->view_impl->state.gravity = g;
    }
};

class xdg_view_map_t : public wf::txn::instruction_t
{
    wayfire_xdg_view *view;
    uint32_t req_id;

  public:
    xdg_view_map_t(wayfire_xdg_view *view)
    {
        this->view = view;
        view->take_ref();
    }

    ~xdg_view_map_t()
    {
        view->unref();
    }

    std::string get_object() override
    {
        return view->to_string();
    }

    void set_pending() override
    {
        LOGC(TXNV, "Pending: map ", wayfire_view{view});
        view->view_impl->pending.mapped = true;
    }

    void commit() override
    {
        wf::txn::instruction_ready_signal data;
        data.instruction = {this};
        this->emit_signal("ready", &data);
    }

    void apply() override
    {
        view->view_impl->state.mapped = true;
        view->lockmgr->unlock(req_id);
        view->map(view->get_wlr_surface());
    }
};

class xdg_view_unmap_t : public wf::txn::instruction_t
{
    wayfire_xdg_view *view;
    uint32_t req_id;

  public:
    xdg_view_unmap_t(wayfire_xdg_view *view)
    {
        this->view = view;
        view->take_ref();
    }

    ~xdg_view_unmap_t()
    {
        view->lockmgr->unlock_all(req_id);
        view->unref();
    }

    std::string get_object() override
    {
        return view->to_string();
    }

    void set_pending() override
    {
        LOGC(TXNV, "Pending: unmap ", wayfire_view{view});
        req_id = view->lockmgr->lock();
        view->view_impl->pending.mapped = false;
    }

    void commit() override
    {
        wf::txn::instruction_ready_signal data;
        data.instruction = {this};
        this->emit_signal("ready", &data);
    }

    void apply() override
    {
        view->view_impl->state.mapped = false;
        view->lockmgr->unlock(req_id);
        view->unmap();
    }
};


wayfire_xdg_view::wayfire_xdg_view(wlr_xdg_toplevel *top) :
    wf::wlr_view_t(), xdg_toplevel(top)
{}

std::unique_ptr<wf::txn::view_transaction_t> wayfire_xdg_view::next_state()
{
    using type = wf::view_impl_transaction_t<
        wayfire_xdg_view,
        xdg_view_geometry_t,
        xdg_view_gravity_t>;

    return std::make_unique<type>(this);
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
        auto tx = wf::txn::transaction_t::create();
        tx->add_instruction(std::make_unique<xdg_view_unmap_t>(this));
        wf::txn::transaction_manager_t::get().submit(std::move(tx));
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
        tile_request(xdg_toplevel->client_pending.maximized ?
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

    xdg_toplevel->base->data = dynamic_cast<view_interface_t*>(this);
    // set initial parent
    on_set_parent.emit(nullptr);

    if (xdg_toplevel->client_pending.fullscreen)
    {
        fullscreen_request(get_output(), true);
    }

    if (xdg_toplevel->client_pending.maximized)
    {
        tile_request(wf::TILED_EDGES_ALL);
    }
}

wayfire_xdg_view::~wayfire_xdg_view()
{}

wf::geometry_t get_xdg_geometry(wlr_xdg_toplevel *toplevel)
{
    wlr_box xdg_geometry;
    wlr_xdg_surface_get_geometry(toplevel->base, &xdg_geometry);

    return xdg_geometry;
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
    LOGI("Commit serial ", xdg_toplevel->base->configure_serial);
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
}

void wayfire_xdg_view::set_tiled(uint32_t edges)
{
    wlr_xdg_toplevel_set_tiled(xdg_toplevel->base, edges);
    last_configure_serial = wlr_xdg_toplevel_set_maximized(xdg_toplevel->base,
        (edges == wf::TILED_EDGES_ALL));
    wlr_view_t::set_tiled(edges);
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
