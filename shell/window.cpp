#include "window.hpp"
#include <cstring>
#include <algorithm>
#include <wayland-cursor.h>

wayfire_display display;

wayfire_window *current_pointer_window = nullptr,
               *current_touch_window   = nullptr;
size_t current_window_touch_points = 0;

int pointer_x, pointer_y;
void pointer_enter(void *data, struct wl_pointer *wl_pointer,
    uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    /* possibly an event for a surface we just destroyed */
    if (!surface)
        return;

    pointer_x = wl_fixed_to_int(surface_x);
    pointer_y = wl_fixed_to_int(surface_y);

    auto window = (wayfire_window*) wl_surface_get_user_data(surface);
    if (window && window->pointer_enter)
        window->pointer_enter(wl_pointer, serial,
                pointer_x, pointer_y);
    if (window)
    {
        current_pointer_window = window;
        window->has_pointer_focus = true;
    }
}

void pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
    struct wl_surface *surface)
{
    /* possibly an event for a surface we just destroyed */
    if (!surface)
        return;

    auto window = (wayfire_window*) wl_surface_get_user_data(surface);
    if (window && window->pointer_leave)
    {
        window->pointer_leave();
        window->has_pointer_focus = false;
    }

    current_pointer_window = nullptr;
}

void pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time,
    wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    pointer_x = wl_fixed_to_int(surface_x);
    pointer_y = wl_fixed_to_int(surface_y);
    if (current_pointer_window && current_pointer_window->pointer_move)
        current_pointer_window->pointer_move(pointer_x, pointer_y);
}

void pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
    uint32_t time, uint32_t button, uint32_t state)
{
    if (current_pointer_window && current_pointer_window->pointer_button)
        current_pointer_window->pointer_button(button, state, pointer_x, pointer_y);
}

void pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time,
    uint32_t axis, wl_fixed_t value)
{
}
void pointer_frame(void *data, struct wl_pointer *wl_pointer) {}
void pointer_axis_source(void *data, struct wl_pointer *wl_pointer,
        uint32_t axis_source) {}
void pointer_axis_stop(void *data, struct wl_pointer *wl_pointer, uint32_t time,
        uint32_t axis) {}
void pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer, uint32_t axis,
        int32_t discrete) {}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete
};

static void touch_down(void *data, struct wl_touch *wl_touch,
                       uint32_t serial, uint32_t time, struct wl_surface *surface,
                       int32_t id, wl_fixed_t x, wl_fixed_t y)
{
    auto window = (wayfire_window*) wl_surface_get_user_data(surface);

    if (current_touch_window != window)
        current_window_touch_points = 0;

    current_touch_window = window;
    current_window_touch_points++;

    if (window->touch_down)
        window->touch_down(id, wl_fixed_to_int(x), wl_fixed_to_int(y));
}

static void
touch_up(void *data, struct wl_touch *wl_touch,
         uint32_t serial, uint32_t time, int32_t id)
{
    if (current_touch_window && current_touch_window->touch_down)
        current_touch_window->touch_up(id);

    current_window_touch_points--;
    if (!current_window_touch_points)
        current_touch_window = nullptr;
}

static void
touch_motion(void *data, struct wl_touch *wl_touch,
             uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
    if (current_touch_window->touch_motion)
        current_touch_window->touch_motion(id, wl_fixed_to_int(x), wl_fixed_to_int(y));
}

static const struct wl_touch_listener touch_listener = {
    touch_down,
    touch_up,
    touch_motion,
    NULL,
    NULL,
    NULL,
    NULL
};

void delete_window(wayfire_window *window)
{
    if (current_pointer_window == window)
        current_pointer_window = nullptr;

    wl_shell_surface_destroy (window->shell_surface);
    wl_surface_destroy (window->surface);

    cairo_surface_destroy(window->cairo_surface);

    backend_delete_window(window);
}

// listeners
void registry_add_object(void *data, struct wl_registry *registry, uint32_t name,
        const char *interface, uint32_t version)
{
    if (!strcmp(interface,"wl_compositor")) {
        display.compositor = (wl_compositor*) wl_registry_bind (registry, name, &wl_compositor_interface, std::min(version, 2u));
    } else if (!strcmp(interface,"wl_shell")) {
        display.shell = (wl_shell*) wl_registry_bind(registry, name, &wl_shell_interface, std::min(version, 2u));
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        display.seat = (wl_seat*) wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 2u));
        display.pointer = wl_seat_get_pointer(display.seat);
        auto touch = wl_seat_get_touch(display.seat);

        wl_pointer_add_listener(display.pointer, &pointer_listener, NULL);
        if (touch)
            wl_touch_add_listener(touch, &touch_listener, NULL);

    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        display.shm = (wl_shm*) wl_registry_bind(registry, name, &wl_shm_interface, std::min(version, 1u));
    } else if (strcmp(interface, wayfire_shell_interface.name) == 0) {
        display.wfshell = (wayfire_shell*) wl_registry_bind(registry, name, &wayfire_shell_interface, std::min(version, 1u));
    } else if (strcmp(interface, wayfire_virtual_keyboard_interface.name) == 0) {
        display.vkbd = (wayfire_virtual_keyboard*) wl_registry_bind(registry, name, &wayfire_virtual_keyboard_interface, std::min(version, 1u));
    }
}

void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name)
{
}
static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

void shell_surface_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void shell_surface_configure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges,
        int32_t width, int32_t height)
{
    ((wayfire_window*) data)->configured = true;
}

static void shell_surface_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

const struct wl_shell_surface_listener shell_surface_listener =
{
    &shell_surface_ping,
    &shell_surface_configure,
    &shell_surface_popup_done
};

wl_cursor *cursor;
wl_surface *cursor_surface;

bool load_cursor()
{
    auto cursor_theme = wl_cursor_theme_load(NULL, 16, display.shm);
    if (!cursor_theme) {
        std::cout << "failed to load cursor theme" << std::endl;
        return false;
    }

    const char* alternatives[] = {
        "left_ptr", "default",
        "top_left_arrow", "left-arrow"
    };

    cursor = NULL;
    for (int i = 0; i < 4 && !cursor; i++)
        cursor = wl_cursor_theme_get_cursor(cursor_theme, alternatives[i]);

    cursor_surface = wl_compositor_create_surface(display.compositor);
    if (!cursor || !cursor_surface) {
        std::cout << "failed to load cursor" << std::endl;
        return false;
    }

    return true;
}

void show_default_cursor(uint32_t serial)
{
    auto image = cursor->images[0];
    auto buffer = wl_cursor_image_get_buffer(image);

    wl_surface_attach(cursor_surface, buffer, 0, 0);
    wl_surface_damage(cursor_surface, 0, 0, image->width, image->height);
    wl_surface_commit(cursor_surface);

    wl_pointer_set_cursor(display.pointer, serial, cursor_surface,
            image->hotspot_x, image->hotspot_y);
}

bool setup_wayland_connection()
{
    display.wl_disp = wl_display_connect(NULL);

    if (!display.wl_disp) {
        std::cerr << "Failed to connect to display!" << std::endl;
        std::exit(-1);
    }

    wl_registry *registry = wl_display_get_registry(display.wl_disp);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display.wl_disp);
    wl_registry_destroy(registry);

    if (!setup_backend())
        return false;

    if (!load_cursor())
        return false;

    return true;
}

void finish_wayland_connection()
{
    finish_backend();
    wl_display_disconnect(display.wl_disp);
}
