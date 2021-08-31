#pragma once

#include <wayfire/transaction/transaction-view.hpp>
#include <wayfire/transaction/transaction.hpp>

namespace wf
{
template<class View, class Geometry, class Gravity, class State>
class view_impl_transaction_t : public txn::view_transaction_t
{
  public:
    view_impl_transaction_t(View *view)
    {
        this->view = view;
    }

    view_transaction_t *set_geometry(const wf::geometry_t& new_g) final
    {
        if (view->pending().geometry != new_g)
        {
            pending.push_back(std::make_unique<Geometry>(view, new_g));
        }

        return this;
    }

    view_transaction_t *set_gravity(wf::gravity_t gravity) final
    {
        if (view->pending().gravity != gravity)
        {
            pending.push_back(std::make_unique<Gravity>(view, gravity));
        }

        return this;
    }

    view_transaction_t *set_tiled(uint32_t edges) final
    {
        if (view->pending().tiled_edges != edges)
        {
            pending.push_back(std::make_unique<State>(
                view, edges, view->pending().fullscreen));
        }

        return this;
    }

    view_transaction_t *set_fullscreen(bool fs) final
    {
        if (view->pending().fullscreen != fs)
        {
            pending.push_back(std::make_unique<State>(
                view, view->pending().tiled_edges, fs));
        }

        return this;
    }

    void schedule_in(
        nonstd::observer_ptr<txn::transaction_t> transaction) final
    {
        for (auto& i : pending)
        {
            transaction->add_instruction(std::move(i));
        }

        pending.clear();
    }

  private:
    View *view;
    std::vector<txn::instruction_uptr_t> pending;
};

void surface_send_frame(wlr_surface *surface);

/**
 * When a surface is resized, the client may not obey the resize request.
 * In these cases, we consider the surface gravity to figure out where to place
 * the view.
 *
 * @param desired How the compositor would like to configure the view.
 * @param actual The window geometry of the view, as reported by the client.
 * @param gravity The window gravity.
 *
 * @return The correct new geometry for the view.
 */
wf::geometry_t align_with_gravity(
    wf::geometry_t desired, wf::geometry_t actual, wf::gravity_t gravity);
}
