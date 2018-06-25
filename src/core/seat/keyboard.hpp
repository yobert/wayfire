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
    wl_listener key, modifier, destroy;

    wf_keyboard(wlr_input_device *keyboard, wayfire_config *config);
};

struct key_callback_data : wf_callback
{
    key_callback *call;
    wf_option key;
};

#endif /* end of include guard: KEYBOARD_HPP */
