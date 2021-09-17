#pragma once

#include <map>
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
    wf::wl_listener_wrapper on_commit;
    std::map<wlr_surface*, uint32_t> held_locks;

    wf::signal_connection_t on_kill = [=] (wf::signal_data_t*)
    {
        on_commit.disconnect();
        wf::txn::emit_instruction_signal(this, "cancel");
    };

    xdg_instruction_t(const xdg_instruction_t&) = delete;
    xdg_instruction_t(xdg_instruction_t&&) = delete;
    xdg_instruction_t& operator =(const xdg_instruction_t&) = delete;
    xdg_instruction_t& operator =(xdg_instruction_t&&) = delete;

    virtual ~xdg_instruction_t()
    {
        unlock_tree_wlr();
        view->unref();
    }

    std::string get_object() override
    {
        return view->to_string();
    }

    /**
     * Decide whether the toplevel has reached the desired configure serial.
     * If it has, return true, emit the ready signal on @self and disconnect
     * @listener.
     *
     * Otherwise, return false.
     */
    bool check_ready(uint32_t serial)
    {
        // TODO: we may have skipped the serial as a whole
        if (view->xdg_toplevel->base->configure_serial == serial)
        {
            on_commit.disconnect();
            lock_tree_wlr();

            wf::txn::emit_instruction_signal(this, "ready");
            return true;
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

    void lock_tree()
    {
        wf::for_each_wlr_surface(view, [] (auto base) { base->lock(); });
    }

    void unlock_tree()
    {
        wf::for_each_wlr_surface(view, [] (auto base) { base->unlock(); });
    }

    void lock_tree_wlr()
    {
        wf::for_each_wlr_surface(view, [=] (auto base)
        {
            auto surf = dynamic_cast<wf::surface_interface_t*>(base)->get_wlr_surface();
            held_locks[surf] = wlr_surface_lock_pending(surf);
        });
    }

    void unlock_tree_wlr()
    {
        for (auto [surf, id] : held_locks)
        {
            wlr_surface_unlock_cached(surf, id);
        }

        held_locks.clear();
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

        lock_tree();
        wlr_xdg_toplevel_set_maximized(view->xdg_toplevel->base,
            desired_edges == wf::TILED_EDGES_ALL);
        auto serial = wlr_xdg_toplevel_set_tiled(view->xdg_toplevel->base,
            desired_edges);
        wf::surface_send_frame(view->xdg_toplevel->base->surface);

        on_commit.set_callback([this, serial] (void*)
        {
            check_ready(serial);
        });
        on_commit.connect(&view->xdg_toplevel->base->surface->events.commit);
    }

    void apply() override
    {
        unlock_tree();
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

        if (client_initiated)
        {
            // Grab a lock now, otherwise, wlroots will do the commit
            lock_tree();
        }
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

        if (client_initiated)
        {
            // We have already grabbed a lock in the constructor. Just signal
            // that we're ready and don't do anything else.
            wf::txn::emit_instruction_signal(this, "ready");
            return;
        }

        lock_tree();

        auto cfg_geometry = target;
        if (view->view_impl->frame)
        {
            cfg_geometry = wf::shrink_by_margins(cfg_geometry,
                view->view_impl->frame->get_margins());
        }

        auto serial = wlr_xdg_toplevel_set_size(view->xdg_toplevel->base,
            cfg_geometry.width, cfg_geometry.height);
        wf::surface_send_frame(view->xdg_toplevel->base->surface);

        on_commit.set_callback([this, serial] (void*)
        {
            check_ready(serial);
        });
        on_commit.connect(&view->xdg_toplevel->base->surface->events.commit);
    }

    void apply() override
    {
        view->damage();
        unlock_tree();

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
        lock_tree();
        wf::txn::instruction_ready_signal data;
        data.instruction = {this};
        this->emit_signal("ready", &data);
    }

    void apply() override
    {
        view->view_impl->state.mapped = true;
        unlock_tree();
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
        lock_tree_wlr();
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
        unlock_tree_wlr();
        view->unmap();
    }
};
