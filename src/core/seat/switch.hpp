#ifndef WF_SEAT_SWITCH_HPP
#define WF_SEAT_SWITCH_HPP

#include "seat.hpp"

namespace wf
{
struct switch_device_t : public wf_input_device_internal
{
    wf::wl_listener_wrapper on_switch;
    void handle_switched(wlr_event_switch_toggle *ev);

    switch_device_t(wlr_input_device *dev);
    virtual ~switch_device_t() = default;
};
}

#endif /* end of include guard: WF_SEAT_SWITCH_HPP */
