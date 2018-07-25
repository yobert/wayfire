#include "cursor.hpp"
#include "core.hpp"
#include "input-manager.hpp"
#include "workspace-manager.hpp"
#include "debug.hpp"
#include "compositor-surface.hpp"

static void handle_pointer_button_cb(wl_listener*, void *data)
{
    auto ev = static_cast<wlr_event_pointer_button*> (data);
    core->input->handle_pointer_button(ev);
    wlr_seat_pointer_notify_button(core->input->seat, ev->time_msec, ev->button, ev->state);
}

static void handle_pointer_motion_cb(wl_listener*, void *data)
{
    auto ev = static_cast<wlr_event_pointer_motion*> (data);
    core->input->handle_pointer_motion(ev);
}

static void handle_pointer_motion_absolute_cb(wl_listener*, void *data)
{
    auto ev = static_cast<wlr_event_pointer_motion_absolute*> (data);
    core->input->handle_pointer_motion_absolute(ev);
}

static void handle_pointer_axis_cb(wl_listener*, void *data)
{
    auto ev = static_cast<wlr_event_pointer_axis*> (data);
    core->input->handle_pointer_axis(ev);
}


void input_manager::handle_pointer_button(wlr_event_pointer_button *ev)
{
    core->input->last_cursor_event_msec = ev->time_msec;
    in_mod_binding = false;

    if (ev->state == WLR_BUTTON_PRESSED)
    {
        count_other_inputs++;

        GetTuple(gx, gy, core->get_cursor_position());
        auto output = core->get_output_at(gx, gy);
        core->focus_output(output);

        std::vector<button_callback*> callbacks;

        auto mod_state = get_modifiers();
        for (auto& pair : button_bindings)
        {
            auto& binding = pair.second;
            const auto button = binding->button->as_cached_button();
            if (binding->output == core->get_active_output() &&
                mod_state == button.mod && ev->button == button.button)
                callbacks.push_back(binding->call);
        }

        GetTuple(ox, oy, core->get_active_output()->get_cursor_position());
        for (auto call : callbacks)
            (*call) (ev->button, ox, oy);
    } else
    {
        count_other_inputs--;
        update_cursor_position(ev->time_msec, false);
    }

    if (active_grab)
    {
        if (active_grab->callbacks.pointer.button)
            active_grab->callbacks.pointer.button(ev->button, ev->state);
    } else if (cursor_focus)
    {
        auto custom = wf_compositor_surface_from_surface(cursor_focus);
        if (custom)
            custom->on_pointer_button(ev->button, ev->state);
    }
}

void input_manager::update_cursor_focus(wayfire_surface_t *focus, int x, int y)
{
    wayfire_compositor_surface_t *compositor_surface = wf_compositor_surface_from_surface(cursor_focus);
    if (compositor_surface)
        compositor_surface->on_pointer_leave();

    if (cursor_focus != focus)
        log_info("change cursor focus %p -> %p", cursor_focus, focus);

    cursor_focus = focus;
    if (focus && !wf_compositor_surface_from_surface(focus))
    {
        wlr_seat_pointer_notify_enter(seat, focus->surface, x, y);
    } else
    {
        wlr_seat_pointer_clear_focus(seat);
    }

    if ((compositor_surface = wf_compositor_surface_from_surface(focus)))
        compositor_surface->on_pointer_enter(x, y);
}

void input_manager::update_cursor_position(uint32_t time_msec, bool real_update)
{
    auto output = core->get_output_at(cursor->x, cursor->y);
    assert(output);

    if (input_grabbed() && real_update)
    {
        GetTuple(sx, sy, core->get_active_output()->get_cursor_position());
        if (active_grab->callbacks.pointer.motion)
            active_grab->callbacks.pointer.motion(sx, sy);
        return;
    }

    GetTuple(px, py, output->get_cursor_position());
    int sx, sy;
    wayfire_surface_t *new_focus = NULL;

    output->workspace->for_each_view(
        [&] (wayfire_view view)
        {
            if (new_focus) return;
            new_focus = view->map_input_coordinates(px, py, sx, sy);
        }, WF_ALL_LAYERS);

    update_cursor_focus(new_focus, sx, sy);

    auto compositor_surface = wf_compositor_surface_from_surface(new_focus);
    if (compositor_surface)
    {
        compositor_surface->on_pointer_motion(sx, sy);
    } else
    {
        wlr_seat_pointer_notify_motion(core->input->seat, time_msec, sx, sy);
    }

    for (auto& icon : drag_icons)
    {
        if (icon->is_mapped())
            icon->update_output_position();
    }
}

void input_manager::handle_pointer_motion(wlr_event_pointer_motion *ev)
{
    core->input->last_cursor_event_msec = ev->time_msec;
    wlr_cursor_move(cursor, ev->device, ev->delta_x, ev->delta_y);
    update_cursor_position(ev->time_msec);
}

void input_manager::handle_pointer_motion_absolute(wlr_event_pointer_motion_absolute *ev)
{
    core->input->last_cursor_event_msec = ev->time_msec;
    wlr_cursor_warp_absolute(cursor, ev->device, ev->x, ev->y);
    update_cursor_position(ev->time_msec);;
}

void input_manager::handle_pointer_axis(wlr_event_pointer_axis *ev)
{
    core->input->last_cursor_event_msec = ev->time_msec;
    std::vector<axis_callback*> callbacks;

    auto mod_state = get_modifiers();

    for (auto& pair : axis_bindings)
    {
        auto& binding = pair.second;
        const auto mod = binding->modifier->as_cached_key().mod;

        if (binding->output == core->get_active_output() &&
            mod_state == mod)
            callbacks.push_back(binding->call);
    }

    for (auto call : callbacks)
        (*call) (ev);

    /* reset modifier bindings */
    in_mod_binding = false;
    if (active_grab)
    {
        if (active_grab->callbacks.pointer.axis)
            active_grab->callbacks.pointer.axis(ev);

        return;
    }

    wlr_seat_pointer_notify_axis(seat, ev->time_msec, ev->orientation,
                                 ev->delta, ev->delta_discrete, ev->source);
}

void input_manager::create_cursor()
{
    cursor = wlr_cursor_create();

    wlr_cursor_attach_output_layout(cursor, core->output_layout);
    wlr_cursor_map_to_output(cursor, NULL);
    wlr_cursor_warp(cursor, NULL, cursor->x, cursor->y);

    xcursor = wlr_xcursor_manager_create(NULL, 32);
    wlr_xcursor_manager_load(xcursor, 1);

    core->set_default_cursor();

    button.notify             = handle_pointer_button_cb;
    motion.notify             = handle_pointer_motion_cb;
    motion_absolute.notify    = handle_pointer_motion_absolute_cb;
    axis.notify               = handle_pointer_axis_cb;

    wl_signal_add(&cursor->events.button, &button);
    wl_signal_add(&cursor->events.motion, &motion);
    wl_signal_add(&cursor->events.motion_absolute, &motion_absolute);
    wl_signal_add(&cursor->events.axis, &axis);
}

