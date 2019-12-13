#ifndef WF_SEAT_POINTING_DEVICE_HPP
#define WF_SEAT_POINTING_DEVICE_HPP

#include "seat.hpp"

namespace wf
{
struct pointing_device_t : public wf_input_device_internal
{
    pointing_device_t(wlr_input_device *dev);
    virtual ~pointing_device_t() = default;

    void update_options() override;

    static struct config_t
    {
        wf_option mouse_cursor_speed;
        wf_option mouse_scroll_speed;
        wf_option mouse_accel_profile;
        wf_option touchpad_cursor_speed;
        wf_option touchpad_scroll_speed;
        wf_option touchpad_accel_profile;
        wf_option touchpad_tap_enabled;
        wf_option touchpad_click_method;
        wf_option touchpad_scroll_method;
        wf_option touchpad_dwt_enabled;
        wf_option touchpad_dwmouse_enabled;
        wf_option touchpad_natural_scroll_enabled;

        void load(wayfire_config *config);
    } config;
};
}

#endif /* end of include guard: WF_SEAT_POINTING_DEVICE_HPP */
