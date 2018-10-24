#ifndef KEYBOARD_HPP
#define KEYBOARD_HPP

#include "config.hpp"
#include "seat.hpp"

extern "C"
{
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_input_device.h>
}

struct wf_keyboard
{
    wlr_keyboard *handle;
    wlr_input_device *device;

    /* so that we can safely use wl_container_of */
    struct listeners
    {
        wl_listener key, modifier, destroy;
        wf_keyboard *keyboard;
    } lss;

    wf_option model, variant, layout, options, rules;
    wf_option repeat_rate, repeat_delay;
    wf_option_callback keyboard_options_updated;

    wf_keyboard(wlr_input_device *keyboard, wayfire_config *config);
    void reload_input_options();
    ~wf_keyboard();
};

#endif /* end of include guard: KEYBOARD_HPP */
