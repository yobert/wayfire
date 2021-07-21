#pragma once

#include <wayfire/transaction/transaction-view.hpp>

#if WF_HAS_XWAYLAND
class wayfire_xwayland_view_base;

namespace wf
{
class xwayland_geometry_t : public txn::instruction_t
{
  public:
    xwayland_geometry_t(wayfire_xwayland_view_base *view, const wf::geometry_t& g);
    ~xwayland_geometry_t();

    std::string get_object() final;
    void set_pending() final;
    void commit() final;
    void apply() final;

  private:
    wayfire_xwayland_view_base *view;
};
}

#endif
