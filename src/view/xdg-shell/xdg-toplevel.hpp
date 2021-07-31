#pragma once

#include <wayfire/transaction/instruction.hpp>
#include <wayfire/debug.hpp>
#include "xdg-shell.hpp"

/**
 * A common class for all xdg-toplevel instructions.
 */
class xdg_instruction_t : public wf::txn::instruction_t
{
  protected:
    wayfire_xdg_view *view;
    uint32_t lock_id = 0;
    wf::wl_listener_wrapper on_cache;

    wf::signal_connection_t on_kill = [=] (wf::signal_data_t*)
    {
        on_cache.disconnect();
        wf::txn::emit_instruction_signal(this, "cancel");
    };

    virtual ~xdg_instruction_t()
    {
        if (lock_id > 0)
        {
            view->lockmgr->unlock_all(lock_id);
        }

        view->unref();
    }

    std::string get_object() override
    {
        return view->to_string();
    }

    /**
     * Go through the cached state of the toplevel and decide whether the desired
     * serial has been reached.
     *
     * If it has been, return true, emit the ready signal on @self and disconnect
     * @listener.
     *
     * Otherwise, return false.
     */
    bool check_ready(uint32_t serial)
    {
        wlr_xdg_surface_state *state;
        wl_list_for_each(state, &view->xdg_toplevel->base->cached, cached_state_link)
        {
            // TODO: we may have skipped the serial as a whole
            if (state->configure_serial == serial)
            {
                on_cache.disconnect();

                if (lock_id > 0)
                {
                    view->lockmgr->checkpoint(lock_id);
                }

                wf::txn::emit_instruction_signal(this, "ready");
                return true;
            }
        }

        // The surface is not ready yet.
        // We give it additional frame events, so that it can redraw to the
        // correct state as soon as possible.
        if (view->get_wlr_surface())
        {
            wf::surface_send_frame(view->get_wlr_surface());
        }

        return false;
    }

  public:
    xdg_instruction_t(wayfire_xdg_view *view)
    {
        this->view = view;
        view->take_ref();
        view->connect_signal(KILL_TX, &on_kill);
    }
};

class xdg_view_state_t : public xdg_instruction_t
{
    uint32_t desired_edges;

  public:
    xdg_view_state_t(wayfire_xdg_view *view, uint32_t tiled_edges) :
        xdg_instruction_t(view)
    {
        this->desired_edges = tiled_edges;
    }

    void set_pending() override
    {
        LOGC(TXNV, "Pending: set state of ", wayfire_view{view},
            " to tiled=", desired_edges);

        view->view_impl->pending.tiled_edges = desired_edges;
    }

    void commit() override
    {
        if (!view->xdg_toplevel)
        {
            wf::txn::emit_instruction_signal(this, "ready");
            return;
        }

        auto& sp = view->xdg_toplevel->server_pending;
        if (sp.tiled == desired_edges)
        {
            wf::txn::emit_instruction_signal(this, "ready");
            return;
        }

        lock_id = view->lockmgr->lock();
        wlr_xdg_toplevel_set_maximized(view->xdg_toplevel->base,
            desired_edges == wf::TILED_EDGES_ALL);
        auto serial = wlr_xdg_toplevel_set_tiled(view->xdg_toplevel->base,
            desired_edges);
        wf::surface_send_frame(view->xdg_toplevel->base->surface);

        on_cache.set_callback([this, serial] (void*)
        {
            check_ready(serial);
        });
        on_cache.connect(&view->xdg_toplevel->base->surface->events.cache);
    }

    void apply() override
    {
        view->lockmgr->unlock(lock_id);
        auto old_edges = view->view_impl->state.tiled_edges;
        view->view_impl->state.tiled_edges = desired_edges;
        view->update_tiled_edges(old_edges);
    }
};

class xdg_view_geometry_t : public xdg_instruction_t
{
    wf::geometry_t target;
    wf::gravity_t current_gravity;
    bool client_initiated = false;

  public:
    xdg_view_geometry_t(wayfire_xdg_view *view, const wf::geometry_t& g,
        bool client_initiated = false) :
        xdg_instruction_t(view)
    {
        this->target = g;
        this->client_initiated = client_initiated;
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

        lock_id = view->lockmgr->lock();
        if (client_initiated)
        {
            // Just grab a lock, but don't do anything, the surface already
            // has the correct geometry.
            wf::txn::emit_instruction_signal(this, "ready");
            return;
        }

        auto cfg_geometry = target;
        if (view->view_impl->frame)
        {
            cfg_geometry = wf::shrink_by_margins(cfg_geometry,
                view->view_impl->frame->get_margins());
        }

        auto serial = wlr_xdg_toplevel_set_size(view->xdg_toplevel->base,
            cfg_geometry.width, cfg_geometry.height);
        wf::surface_send_frame(view->xdg_toplevel->base->surface);

        on_cache.set_callback([this, serial] (void*)
        {
            check_ready(serial);
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
        target = wf::align_with_gravity(target, box, current_gravity);
        view->view_impl->state.geometry = target;

        // Adjust output geometry for shadows and other parts of the surface
        target.x    -= box.x;
        target.y    -= box.y;
        target.width = view->get_wlr_surface()->current.width;
        target.height  = view->get_wlr_surface()->current.height;
        view->geometry = target;
        view->damage();
    }
};

class xdg_view_gravity_t : public xdg_instruction_t
{
    wf::gravity_t g;

  public:
    xdg_view_gravity_t(wayfire_xdg_view *view, wf::gravity_t g) :
        xdg_instruction_t(view)
    {
        this->g = g;
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

class xdg_view_map_t : public xdg_instruction_t
{
  public:
    using xdg_instruction_t::xdg_instruction_t;

    void set_pending() override
    {
        LOGC(TXNV, "Pending: map ", wayfire_view{view});
        view->view_impl->pending.mapped = true;
    }

    void commit() override
    {
        lock_id = view->lockmgr->lock();
        view->lockmgr->checkpoint(lock_id);

        wf::txn::instruction_ready_signal data;
        data.instruction = {this};
        this->emit_signal("ready", &data);
    }

    void apply() override
    {
        view->view_impl->state.mapped = true;
        view->lockmgr->unlock(lock_id);
        view->map(view->get_wlr_surface());
    }
};

class xdg_view_unmap_t : public xdg_instruction_t
{
  public:
    using xdg_instruction_t::xdg_instruction_t;

    void set_pending() override
    {
        LOGC(TXNV, "Pending: unmap ", wayfire_view{view});
        view->view_impl->pending.mapped = false;

        // Typically locking happens in the commit() handler. We cannot afford
        // to wait, though. The surface is about to be unmapped so we need to
        // take a lock immediately.
        lock_id = view->lockmgr->lock();
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
        view->lockmgr->unlock(lock_id);
        view->unmap();
    }
};
