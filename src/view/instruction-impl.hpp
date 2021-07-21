#pragma once

#include <wayfire/transaction/transaction-view.hpp>
#include <wayfire/transaction/transaction.hpp>

namespace wf
{
template<class View, class Geometry>
class view_impl_transaction_t : public txn::view_transaction_t
{
  public:
    view_impl_transaction_t(View *view)
    {
        this->view = view;
    }

    void set_geometry(const wf::geometry_t& new_g) final
    {
        pending.push_back(std::make_unique<Geometry>(view, new_g));
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

inline void emit_instruction_signal(txn::instruction_t *self,
    std::string_view name)
{
    txn::_instruction_signal data;
    data.instruction = {self};
    self->emit_signal(std::string(name), &data);
}
}
