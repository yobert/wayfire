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

    void set_geometry(const wf::geometry_t& new_g) final
    {
        if (view->pending().geometry != new_g)
        {
            pending.push_back(std::make_unique<Geometry>(view, new_g));
        }
    }

    void set_gravity(wf::gravity_t gravity) final
    {
        if (view->pending().gravity != gravity)
        {
            pending.push_back(std::make_unique<Gravity>(view, gravity));
        }
    }

    void set_tiled(uint32_t edges) final
    {
        if (view->pending().tiled_edges != edges)
        {
            pending.push_back(std::make_unique<State>(view, edges));
        }
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
}
