#ifndef SEAT_HPP
#define SEAT_HPP

extern "C"
{
#include <wlr/types/wlr_data_device.h>
#include <wlr/backend/libinput.h>
}

#include "output.hpp"

struct wf_callback
{
    int id;
    wayfire_output *output;
};

struct wf_drag_icon : public wayfire_surface_t
{
    wlr_drag_icon *icon;
    wl_listener map_ev, unmap_ev, destroy;

    wf_drag_icon(wlr_drag_icon *icon);

    bool is_subsurface() { return true ;}
    wf_point get_output_position();
    void damage(const wlr_box& rect);
};

namespace device_config
{
    void load(wayfire_config *conf);
};

void configure_input_device(libinput_device *device);

#endif /* end of include guard: SEAT_HPP */
