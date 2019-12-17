#include "pointing-device.hpp"
#include <debug.hpp>

wf::pointing_device_t::pointing_device_t(wlr_input_device *dev)
    : wf_input_device_internal(dev)
{
    update_options();
}

wf::pointing_device_t::config_t wf::pointing_device_t::config;
void wf::pointing_device_t::config_t::load(wayfire_config *config)
{
    auto section = (*config)["input"];
    mouse_cursor_speed              = section->get_option("mouse_cursor_speed", "0");
    mouse_accel_profile             = section->get_option("mouse_accel_profile", "default");
    touchpad_cursor_speed           = section->get_option("touchpad_cursor_speed", "0");
    touchpad_accel_profile          = section->get_option("touchpad_accel_profile", "default");
    touchpad_tap_enabled            = section->get_option("tap_to_click", "1");
    touchpad_click_method           = section->get_option("click_method", "default");
    touchpad_scroll_method          = section->get_option("scroll_method", "default");
    touchpad_dwt_enabled            = section->get_option("disable_while_typing", "0");
    touchpad_dwmouse_enabled        = section->get_option("disable_touchpad_while_mouse", "0");
    touchpad_natural_scroll_enabled = section->get_option("natural_scroll", "0");
}

void wf::pointing_device_t::update_options()
{
    /* We currently support options only for libinput devices */
    if (!wlr_input_device_is_libinput(get_wlr_handle()))
        return;

    auto dev = wlr_libinput_get_device_handle(get_wlr_handle());
    assert(dev);

    /* we are configuring a touchpad */
    if (libinput_device_config_tap_get_finger_count(dev) > 0)
    {
        libinput_device_config_accel_set_speed(dev,
            config.touchpad_cursor_speed->as_cached_double());

        if (config.touchpad_accel_profile->as_string() == "default") {
            libinput_device_config_accel_set_profile(dev,
                libinput_device_config_accel_get_default_profile(dev));
        } else if (config.touchpad_accel_profile->as_string() == "none") {
            libinput_device_config_accel_set_profile(dev,
                LIBINPUT_CONFIG_ACCEL_PROFILE_NONE);
        } else if (config.touchpad_accel_profile->as_string() == "adaptive") {
            libinput_device_config_accel_set_profile(dev,
                LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
        } else if (config.touchpad_accel_profile->as_string() == "flat") {
            libinput_device_config_accel_set_profile(dev,
                LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
        }

        libinput_device_config_tap_set_enabled(dev,
            config.touchpad_tap_enabled->as_cached_int() ?
            LIBINPUT_CONFIG_TAP_ENABLED : LIBINPUT_CONFIG_TAP_DISABLED);

        if (config.touchpad_click_method->as_string() == "default") {
            libinput_device_config_click_set_method(dev,
                libinput_device_config_click_get_default_method(dev));
        } else if (config.touchpad_click_method->as_string() == "none") {
            libinput_device_config_click_set_method(dev,
                LIBINPUT_CONFIG_CLICK_METHOD_NONE);
        } else if (config.touchpad_click_method->as_string() == "button-areas") {
            libinput_device_config_click_set_method(dev,
                LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS);
        } else if (config.touchpad_click_method->as_string() == "clickfinger") {
            libinput_device_config_click_set_method(dev,
                LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER);
        }

        if (config.touchpad_scroll_method->as_string() == "default") {
            libinput_device_config_scroll_set_method(dev,
                libinput_device_config_scroll_get_default_method(dev));
        } else if (config.touchpad_scroll_method->as_string() == "none") {
            libinput_device_config_scroll_set_method(dev,
                LIBINPUT_CONFIG_SCROLL_NO_SCROLL);
        } else if (config.touchpad_scroll_method->as_string() == "two-finger") {
            libinput_device_config_scroll_set_method(dev,
                LIBINPUT_CONFIG_SCROLL_2FG);
        } else if (config.touchpad_scroll_method->as_string() == "edge") {
            libinput_device_config_scroll_set_method(dev,
                LIBINPUT_CONFIG_SCROLL_EDGE);
        } else if (config.touchpad_scroll_method->as_string() == "on-button-down") {
            libinput_device_config_scroll_set_method(dev,
                LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN);
        }

        libinput_device_config_dwt_set_enabled(dev,
            config.touchpad_dwt_enabled->as_cached_int() ?
            LIBINPUT_CONFIG_DWT_ENABLED : LIBINPUT_CONFIG_DWT_DISABLED);

        libinput_device_config_send_events_set_mode(dev,
            config.touchpad_dwmouse_enabled->as_cached_int() ?
            LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE
                : LIBINPUT_CONFIG_SEND_EVENTS_ENABLED);

        if (libinput_device_config_scroll_has_natural_scroll(dev) > 0)
        {
            libinput_device_config_scroll_set_natural_scroll_enabled(dev,
                    (bool)config.touchpad_natural_scroll_enabled->as_cached_int());
        }
    } else {
        libinput_device_config_accel_set_speed(dev,
            config.mouse_cursor_speed->as_cached_double());

        if (config.mouse_accel_profile->as_string() == "default") {
            libinput_device_config_accel_set_profile(dev,
                libinput_device_config_accel_get_default_profile(dev));
        } else if (config.mouse_accel_profile->as_string() == "none") {
            libinput_device_config_accel_set_profile(dev,
                LIBINPUT_CONFIG_ACCEL_PROFILE_NONE);
        } else if (config.mouse_accel_profile->as_string() == "adaptive") {
            libinput_device_config_accel_set_profile(dev,
                LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE);
        } else if (config.mouse_accel_profile->as_string() == "flat") {
            libinput_device_config_accel_set_profile(dev,
                LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
        }
    }
}
