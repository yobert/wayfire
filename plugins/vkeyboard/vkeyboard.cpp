#include <core.hpp>
#include <debug.hpp>
#include <config.hpp>
#include <view.hpp>
#include <output.hpp>
#include <plugin.hpp>
#include <signal-definitions.hpp>
#include <workspace-manager.hpp>
#include <linux/input-event-codes.h>
#include "proto/wayfire-shell-server.h"

class vkeyboard : public wayfire_plugin_t
{
    weston_layer input_layer;
    touch_gesture_callback swipe;

    weston_seat *vseat = nullptr;
    weston_keyboard *vkbd;

    std::string keyboard_exec_path = INSTALL_PREFIX "/lib/wayfire/wayfire-virtual-keyboard";

    public:
        wl_resource *resource = nullptr;
        wayfire_view view;

        void init(wayfire_config *config);
        void bind(wl_resource *resource);

        void show_keyboard();
        void set_keyboard(wayfire_view view);
        void unset_keyboard();
        void configure_keyboard(wayfire_view view, int x, int y);

        void send_key_up(uint32_t key);
        void send_key_down(uint32_t key);
};

void send_key_pressed(struct wl_client *client,
                      struct wl_resource *resource,
                      uint32_t key)
{
    auto vk = (vkeyboard*) wl_resource_get_user_data(resource);
    vk->send_key_down(key);
}

void send_key_released(struct wl_client *client,
                       struct wl_resource *resource,
                       uint32_t key)
{
    auto vk = (vkeyboard*) wl_resource_get_user_data(resource);
    vk->send_key_up(key);
}

void set_virtual_keyboard(struct wl_client *client,
                          struct wl_resource *resource,
                          struct wl_resource *surface)
{
    auto vk = (vkeyboard*) wl_resource_get_user_data(resource);

    auto wsurf = (weston_surface*) wl_resource_get_user_data(surface);
    auto view = core->find_view(wsurf);

    vk->resource = resource;
    vk->set_keyboard(view);
}

void configure_keyboard(struct wl_client *client,
                        struct wl_resource *resource,
                        struct wl_resource *surface,
                        int32_t x,
                        int32_t y)
{
    auto vk = (vkeyboard*) wl_resource_get_user_data(resource);

    auto wsurf = (weston_surface*) wl_resource_get_user_data(surface);
    auto view = core->find_view(wsurf);

    vk->configure_keyboard(view, x, y);
}

void start_interactive_move(struct wl_client *client,
                        struct wl_resource *resource,
                        struct wl_resource *surface)
{
    auto wsurf = (weston_surface*) wl_resource_get_user_data(surface);
    auto view = core->find_view(wsurf);

    move_request_signal data;
    data.view = view;

    auto touch = weston_seat_get_touch(core->get_current_seat());
    if (!touch)
    {
        auto ptr = weston_seat_get_pointer(core->get_current_seat());
        data.serial = ptr->grab_serial;
    } else {
        data.serial = touch->grab_serial;
    }


    view->output->emit_signal("move-request", &data);
}


static const struct wayfire_virtual_keyboard_interface vk_iface =
{
    send_key_pressed,
    send_key_released,
    set_virtual_keyboard,
    configure_keyboard,
    start_interactive_move
};

void unbind_virtual_keyboard(wl_resource* resource)
{
    vkeyboard *vk = (vkeyboard*) wl_resource_get_user_data(resource);
    vk->unset_keyboard();
}

void bind_virtual_keyboard(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    vkeyboard *vk = (vkeyboard*) data;

    auto resource = wl_resource_create(client, &wayfire_virtual_keyboard_interface, 1, id);

    wl_resource_set_implementation(resource, &vk_iface,
                                   vk, unbind_virtual_keyboard);

    vk->bind(resource);
}

void vkeyboard::init(wayfire_config *config)
{
    grab_interface->name = "vkeyboard";
    grab_interface->abilities_mask = 0;

    wl_global *kbd;
    if ((kbd = wl_global_create(core->ec->wl_display, &wayfire_virtual_keyboard_interface,
                                1, this, bind_virtual_keyboard)) == NULL)
    {
        errio << "Failed to create wayfire_shell interface" << std::endl;
    }

    weston_layer_init(&input_layer, core->ec);
    weston_layer_set_position(&input_layer, WESTON_LAYER_POSITION_TOP_UI);

    swipe = [=] (wayfire_touch_gesture *gesture)
    {
        if (gesture->direction == GESTURE_DIRECTION_RIGHT)
            show_keyboard();
    };

    wayfire_touch_gesture show_gesture;
    show_gesture.type = GESTURE_EDGE_SWIPE;
    show_gesture.finger_count = 3;

    output->add_gesture(show_gesture, &swipe);

    keyboard_exec_path = config->get_section("vkeyboard")->get_string("path", keyboard_exec_path);
}

void vkeyboard::bind(wl_resource *res)
{
    GetTuple(sw, sh, output->get_screen_size());
    wayfire_virtual_keyboard_send_match_output_size(res, sw, sh);

    if (!vseat)
    {
        vseat = new weston_seat;
        weston_seat_init(vseat, core->ec, "virtual-input");
        weston_seat_init_keyboard(vseat, NULL);

        vkbd = weston_seat_get_keyboard(vseat);
    }
}

void vkeyboard::send_key_down(uint32_t key)
{
    auto kbd = weston_seat_get_keyboard(core->get_current_seat());
    weston_seat_set_keyboard_focus(vseat, kbd->focus);

    auto t = time(NULL);
    notify_key(vseat, t, key, WL_KEYBOARD_KEY_STATE_PRESSED, STATE_UPDATE_AUTOMATIC);
}

void vkeyboard::send_key_up(uint32_t key)
{
    auto t = time(NULL);
    notify_key(vseat, t, key, WL_KEYBOARD_KEY_STATE_RELEASED, STATE_UPDATE_AUTOMATIC);
}

void vkeyboard::set_keyboard(wayfire_view view)
{
    this->view = view;
    view->is_special = true;
    auto output = view->output;

    output->detach_view(view);
    view->output = output;
    weston_layer_entry_insert(&input_layer.view_list, &view->handle->layer_link);

    output->workspace->add_renderable_view(view);
}

void vkeyboard::unset_keyboard()
{
    if (resource)
    {
        resource = NULL;
        view->output->workspace->rem_renderable_view(view);
    }
}

void vkeyboard::configure_keyboard(wayfire_view view, int x, int y)
{
    view->move(x, y);
}

void vkeyboard::show_keyboard()
{
    if (!resource)
    {
        core->run(keyboard_exec_path.c_str());
    } else
    {
        wayfire_virtual_keyboard_send_show_virtual_keyboard(resource);
    }
}

extern "C" {
    wayfire_plugin_t *newInstance()
    {
        return new vkeyboard;
    }
}
