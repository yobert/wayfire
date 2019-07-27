#include "cursor.hpp"
#include "touch.hpp"
#include "../core-impl.hpp"
#include "input-manager.hpp"
#include "workspace-manager.hpp"
#include "debug.hpp"
#include "compositor-surface.hpp"
#include "output-layout.hpp"
#include "tablet.hpp"

extern "C" {
#include <wlr/util/region.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
}

bool input_manager::handle_pointer_button(wlr_event_pointer_button *ev)
{
    mod_binding_key = 0;

    std::vector<std::function<void()>> callbacks;
    if (ev->state == WLR_BUTTON_PRESSED)
    {
        cursor->count_pressed_buttons++;
        if (cursor->count_pressed_buttons == 1)
        {
            auto gc = wf::get_core().get_cursor_position();
            auto output =
                wf::get_core().output_layout->get_output_at(gc.x, gc.y);
            wf::get_core().focus_output(output);
        }

        auto oc = wf::get_core().get_active_output()->get_cursor_position();
        auto mod_state = get_modifiers();
        for (auto& binding : bindings[WF_BINDING_BUTTON])
        {
            if (binding->output == wf::get_core().get_active_output() &&
                binding->value->as_cached_button().matches(
                    {mod_state, ev->button}))
            {
                /* We must be careful because the callback might be erased,
                 * so force copy the callback into the lambda */
                auto callback = binding->call.button;
                callbacks.push_back([=] () {
                    (*callback) (ev->button, oc.x, oc.y);
                });
            }
        }

        for (auto& binding : bindings[WF_BINDING_ACTIVATOR])
        {
            if (binding->output == wf::get_core().get_active_output() &&
                binding->value->matches_button({mod_state, ev->button}))
            {
                /* We must be careful because the callback might be erased,
                 * so force copy the callback into the lambda */
                auto callback = binding->call.activator;
                callbacks.push_back([=] () {
                    (*callback) (ACTIVATOR_SOURCE_BUTTONBINDING, ev->button);
                });
            }
        }

        for (auto call : callbacks)
            call();
    }
    else
    {
        cursor->count_pressed_buttons--;
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

    /* Do not send event to clients if any button binding used it. However, if
     * the binding was just a single button (no modifiers), forward it to the client.
     * This allows some use-cases like click-to-focus plugin. */
    return (!callbacks.empty() && get_modifiers());
}

void input_manager::update_cursor_focus(wf::surface_interface_t *focus, wf_pointf local)
{
    if (focus && !can_focus_surface(focus))
        return;

    wf::compositor_surface_t *compositor_surface =
        wf_compositor_surface_from_surface(cursor_focus);
    if (compositor_surface)
        compositor_surface->on_pointer_leave();

    bool focus_change = (cursor_focus != focus);
    if (focus_change)
        log_debug("change cursor focus %p -> %p", cursor_focus, focus);

    cursor_focus = focus;
    wlr_surface *next_focus_wlr_surface = nullptr;
    if (focus && !wf_compositor_surface_from_surface(focus))
    {
        next_focus_wlr_surface = focus->priv->wsurface;
        wlr_seat_pointer_notify_enter(seat, focus->priv->wsurface,
            local.x, local.y);
    } else
    {
        wlr_seat_pointer_clear_focus(seat);
    }

    if ((compositor_surface = wf_compositor_surface_from_surface(focus)))
        compositor_surface->on_pointer_enter(local.x, local.y);

    if (focus_change)
    {
        wlr_pointer_constraint_v1 *constraint = NULL;;
        if (next_focus_wlr_surface)
        {
            constraint = wlr_pointer_constraints_v1_constraint_for_surface(
                wf::get_core().protocols.pointer_constraints,
                next_focus_wlr_surface, seat);
        }
        set_pointer_constraint(constraint);
    }
}

void input_manager::update_cursor_position(uint32_t time_msec, bool real_update)
{
    if (input_grabbed())
    {
        auto oc = wf::get_core().get_active_output()->get_cursor_position();
        if (active_grab->callbacks.pointer.motion && real_update)
            active_grab->callbacks.pointer.motion(oc.x, oc.y);

        return;
    }

    wf_pointf gc = wf::get_core().get_cursor_position();

    wf_pointf local;
    wf::surface_interface_t *new_focus = nullptr;
    /* If we have a grabbed surface, but no drag, we want to continue sending
     * events to the grabbed surface, even if the pointer goes outside of it.
     * This enables Xwayland DnD to work correctly, and also lets the user for
     * ex. grab a scrollbar and move their mouse freely.
     *
     * Notice in case of active wayland DnD we need to send events to the surfaces
     * which are actually under the mouse */
    if (cursor->grabbed_surface && !this->drag_icon)
    {
        new_focus = cursor->grabbed_surface;
        local = get_surface_relative_coords(new_focus, gc);
    } else
    {
        new_focus = input_surface_at(gc, local);
        update_cursor_focus(new_focus, local);
    }

    auto compositor_surface = wf_compositor_surface_from_surface(new_focus);
    if (compositor_surface)
    {
        compositor_surface->on_pointer_motion(local.x, local.y);
    }
    else if (real_update)
    {
        wlr_seat_pointer_notify_motion(wf::get_core_impl().input->seat,
            time_msec, local.x, local.y);
    }

    update_drag_icon();
}
wf_pointf input_manager::get_cursor_position_relative_to_cursor_focus()
{
    return get_cursor_position_relative_to_cursor_focus(
        wf::get_core().get_cursor_position());
}

wf_pointf input_manager::get_cursor_position_relative_to_cursor_focus(
    wf_pointf gc)
{
    auto view =
        (wf::view_interface_t*) (this->cursor_focus->get_main_surface());

    auto output = view->get_output()->get_layout_geometry();
    gc.x -= output.x;
    gc.y -= output.y;

    return view->global_to_local_point(gc, this->cursor_focus);
}

wf_pointf input_manager::get_absolute_cursor_from_relative(wf_pointf relative)
{
    auto view =
        (wf::view_interface_t*) (this->cursor_focus->get_main_surface());

    auto output_geometry = view->get_output_geometry();
    wf_point origin = {output_geometry.x, output_geometry.y};

    for (auto& surf : view->enumerate_surfaces(origin))
    {
        if (surf.surface == this->cursor_focus)
        {
            relative.x += surf.position.x;
            relative.y += surf.position.y;
        }
    }

    relative = view->transform_point(relative);
    auto output = view->get_output()->get_layout_geometry();
    return {relative.x + output.x, relative.y + output.y};
}

static double distance_between_points(const wf_pointf& a, const wf_pointf& b)
{
    return std::sqrt(1.0 * (a.x - b.x) * (a.x - b.x) +
        1.0 * (a.y - b.y) * (a.y - b.y));
}

static wf_pointf region_closest_point(const wf_region& region,
    const wf_pointf& ref)
{
    if (region.empty())
        return ref;

    auto extents = region.get_extents();
    wf_pointf result = {1.0 * extents.x1, 1.0 * extents.y1};

    for (const auto& box : region)
    {
        auto wlr_box = wlr_box_from_pixman_box(box);

        double x, y;
        wlr_box_closest_point(&wlr_box, ref.x, ref.y, &x, &y);
        wf_pointf closest = {x, y};

        if (distance_between_points(ref, result) >
                distance_between_points(ref,closest))
        {
            result = closest;
        }
    }

    return result;
}

wlr_pointer_constraint_v1 *input_manager::get_active_pointer_constraint()
{
    return this->active_pointer_constraint;
}

void input_manager::set_pointer_constraint(
    wlr_pointer_constraint_v1 *constraint, bool last_destroyed)
{
    if (constraint == this->active_pointer_constraint)
        return;

    /* First set the constraint to the new constraint.
     * send_deactivated might cause destruction of the active constraint,
     * and then before we've finished this request we'd get another to reset
     * the constraint to NULL.
     *
     * XXX: a race is still possible if we directly switch from one constraint
     * to another, and the first one gets destroyed. This is however almost
     * impossible, since a constraint keeps the cursor inside its surface, so
     * the only way to cancel this would be to either cancel the constraint by
     * activating a plugin or when the constraint itself gets destroyed. In both
     * cases, we first get a set_pointer_constraint(NULL) request */
    auto last_constraint = this->active_pointer_constraint;
    this->active_pointer_constraint = constraint;

    if (last_constraint && !last_destroyed)
    {
        wlr_pointer_constraint_v1_send_deactivated(last_constraint);
        // TODO: restore cursor position from the constraint hint
    }

    this->constraint_region.clear();
    if (!constraint)
        return;

    wlr_pointer_constraint_v1_send_activated(constraint);
    if (constraint->type == WLR_POINTER_CONSTRAINT_V1_CONFINED)
        this->constraint_region = wf_region{&constraint->region};

    if (this->cursor_focus)
    {
        auto current = get_cursor_position_relative_to_cursor_focus();
        bool is_inside_constraint = pixman_region32_contains_point(
            this->constraint_region.to_pixman(), current.x, current.y, NULL);

        if (!is_inside_constraint)
        {
            auto closest =
                region_closest_point(this->constraint_region, current);
            closest = get_absolute_cursor_from_relative(closest);

            wlr_cursor_warp_closest(cursor->cursor, NULL, closest.x, closest.y);
        }
    }
}

void input_manager::handle_pointer_motion(wlr_event_pointer_motion *ev)
{
    if (input_grabbed() && active_grab->callbacks.pointer.relative_motion)
        active_grab->callbacks.pointer.relative_motion(ev);

    // send relative motion
    wlr_relative_pointer_manager_v1_send_relative_motion(
        wf::get_core().protocols.relative_pointer, seat,
        (uint64_t)ev->time_msec * 1000, ev->delta_x, ev->delta_y,
        ev->unaccel_dx, ev->unaccel_dy);

    double dx = ev->delta_x;
    double dy = ev->delta_y;

    // confine inside constraints
    if (this->active_pointer_constraint && this->cursor_focus)
    {
        if (constraint_region.empty())
        {
            /* Empty region */
            dx = dy = 0;
        } else
        {
            // next coordinates
            double cx = cursor->cursor->x + dx;
            double cy = cursor->cursor->y + dy;
            auto local = get_cursor_position_relative_to_cursor_focus({cx, cy});

            if (!constraint_region.contains_point({(int)local.x, (int)local.y}))
            {
                // get as close as possible
                local = region_closest_point(constraint_region, local);
                wf_pointf target = get_absolute_cursor_from_relative(local);

                dx = target.x - cursor->cursor->x;
                dy = target.y - cursor->cursor->y;
            }
        }
    }

    wlr_cursor_move(cursor->cursor, ev->device, dx, dy);
    update_cursor_position(ev->time_msec);
}

void input_manager::handle_pointer_motion_absolute(wlr_event_pointer_motion_absolute *ev)
{
    // next coordinates
    double cx, cy;
    wlr_cursor_absolute_to_layout_coords(cursor->cursor, ev->device,
        ev->x,ev->y, &cx, &cy);

    // send relative motion
    double dx = cx - cursor->cursor->x;
    double dy = cy - cursor->cursor->y;
    wlr_relative_pointer_manager_v1_send_relative_motion(
        wf::get_core().protocols.relative_pointer, seat,
        (uint64_t)ev->time_msec * 1000, dx, dy, dx, dy);

    // check constraints
    if (this->active_pointer_constraint && this->cursor_focus)
    {
        auto local = get_cursor_position_relative_to_cursor_focus({cx, cy});
        if (constraint_region.contains_point({(int)local.x, (int)local.y}))
            return;
    }

    wlr_cursor_warp_absolute(cursor->cursor, ev->device, ev->x, ev->y);
    update_cursor_position(ev->time_msec);
}

void input_manager::handle_pointer_axis(wlr_event_pointer_axis *ev)
{
    std::vector<axis_callback*> callbacks;

    auto mod_state = get_modifiers();

    for (auto& binding : bindings[WF_BINDING_AXIS])
    {
        if (binding->output == wf::get_core().get_active_output() &&
            binding->value->as_cached_key().matches({mod_state, 0}))
            callbacks.push_back(binding->call.axis);
    }

    for (auto call : callbacks)
        (*call) (ev);

    /* reset modifier bindings */
    mod_binding_key = 0;
    if (active_grab)
    {
        if (active_grab->callbacks.pointer.axis)
            active_grab->callbacks.pointer.axis(ev);

        return;
    }

    /* Do not send scroll events to clients if an axis binding has used up the event */
    if (callbacks.size())
        return;

    double mult = ev->source == WLR_AXIS_SOURCE_FINGER ?
        cursor->touchpad_scroll_speed->as_cached_double() :
        cursor->mouse_scroll_speed->as_cached_double();
    wlr_seat_pointer_notify_axis(seat, ev->time_msec, ev->orientation,
        mult * ev->delta, mult * ev->delta_discrete, ev->source);
}

void input_manager::handle_pointer_swipe_begin(wlr_event_pointer_swipe_begin *ev)
{
    wlr_pointer_gestures_v1_send_swipe_begin(
        wf::get_core().protocols.pointer_gestures, seat,
        ev->time_msec, ev->fingers);
}

void input_manager::handle_pointer_swipe_update(wlr_event_pointer_swipe_update *ev)
{
    wlr_pointer_gestures_v1_send_swipe_update(
        wf::get_core().protocols.pointer_gestures, seat,
        ev->time_msec, ev->dx, ev->dy);
}

void input_manager::handle_pointer_swipe_end(wlr_event_pointer_swipe_end *ev)
{
    wlr_pointer_gestures_v1_send_swipe_end(
        wf::get_core().protocols.pointer_gestures, seat,
        ev->time_msec, ev->cancelled);
}

void input_manager::handle_pointer_pinch_begin(wlr_event_pointer_pinch_begin *ev)
{
    wlr_pointer_gestures_v1_send_pinch_begin(
        wf::get_core().protocols.pointer_gestures, seat,
        ev->time_msec, ev->fingers);
}

void input_manager::handle_pointer_pinch_update(wlr_event_pointer_pinch_update *ev)
{
    wlr_pointer_gestures_v1_send_pinch_update(
        wf::get_core().protocols.pointer_gestures, seat,
        ev->time_msec, ev->dx, ev->dy, ev->scale, ev->rotation);
}

void input_manager::handle_pointer_pinch_end(wlr_event_pointer_pinch_end *ev)
{
    wlr_pointer_gestures_v1_send_pinch_end(
        wf::get_core().protocols.pointer_gestures, seat,
        ev->time_msec, ev->cancelled);
}

void input_manager::handle_pointer_frame()
{
    wlr_seat_pointer_notify_frame(seat);
}

wf_cursor::wf_cursor()
{
    cursor = wlr_cursor_create();

    wlr_cursor_attach_output_layout(cursor,
        wf::get_core().output_layout->get_handle());

    wlr_cursor_map_to_output(cursor, NULL);
    wlr_cursor_warp(cursor, NULL, cursor->x, cursor->y);

    setup_listeners();
    init_xcursor();

    config_reloaded = [=] (wf::signal_data_t*) {
        init_xcursor();
    };

    auto section = wf::get_core().config->get_section("input");
    mouse_scroll_speed    = section->get_option("mouse_scroll_speed", "1");
    touchpad_scroll_speed = section->get_option("touchpad_scroll_speed", "1");

    wf::get_core().connect_signal("reload-config", &config_reloaded);
}

void wf_cursor::setup_listeners()
{
    auto& core = wf::get_core_impl();

    on_button.set_callback([&] (void *data) {
        this->handle_pointer_button((wlr_event_pointer_button*) data);
        wlr_idle_notify_activity(core.protocols.idle, core.get_current_seat());
    });
    on_button.connect(&cursor->events.button);

    on_frame.set_callback([&] (void *) {
        core.input->handle_pointer_frame();

        wlr_idle_notify_activity(core.protocols.idle,
            core.get_current_seat());
    });
    on_frame.connect(&cursor->events.frame);

#define setup_passthrough_callback(evname) \
    on_##evname.set_callback([&] (void *data) { \
        auto ev = static_cast<wlr_event_pointer_##evname *> (data); \
        core.input->handle_pointer_##evname (ev); \
        wlr_idle_notify_activity(core.protocols.idle, core.get_current_seat()); \
    }); \
    on_##evname.connect(&cursor->events.evname);

    setup_passthrough_callback(motion);
    setup_passthrough_callback(motion_absolute);
    setup_passthrough_callback(axis);
    setup_passthrough_callback(swipe_begin);
    setup_passthrough_callback(swipe_update);
    setup_passthrough_callback(swipe_end);
    setup_passthrough_callback(pinch_begin);
    setup_passthrough_callback(pinch_update);
    setup_passthrough_callback(pinch_end);
#undef setup_passthrough_callback

    /**
     * All tablet events are directly sent to the tablet device, it should
     * manage them
     */
#define setup_tablet_callback(evname) \
    on_tablet_##evname.set_callback([=] (void *data) { \
        auto ev = static_cast<wlr_event_tablet_tool_##evname *> (data); \
        if (ev->device->tablet->data) { \
            auto tablet = \
                static_cast<wf::tablet_t*> (ev->device->tablet->data); \
            tablet->handle_##evname (ev); \
        } \
    }); \
    on_tablet_##evname.connect(&cursor->events.tablet_tool_##evname);

    setup_tablet_callback(tip);
    setup_tablet_callback(axis);
    setup_tablet_callback(button);
    setup_tablet_callback(proximity);
#undef setup_tablet_callback
}

void wf_cursor::handle_pointer_button(wlr_event_pointer_button *ev)
{
    auto& core = wf::get_core_impl();
    if (!core.input->handle_pointer_button(ev))
    {
        /* start a button held grab, so that the window will receive all the
         * subsequent events, no matter what happens */
        if (count_pressed_buttons == 1 && core.get_cursor_focus())
            start_held_grab(core.get_cursor_focus());

        wlr_seat_pointer_notify_button(core.input->seat, ev->time_msec,
            ev->button, ev->state);

        /* end the button held grab. We need to to this here after we have send
         * the last button release event, so that buttons don't get stuck in clients */
        if (count_pressed_buttons == 0)
            end_held_grab();
    }
}

void wf_cursor::init_xcursor()
{
    auto section = wf::get_core().config->get_section("input");

    auto theme = section->get_option("cursor_theme", "default")->as_string();
    auto size = section->get_option("cursor_size", "24");

    auto theme_ptr = (theme == "default") ? NULL : theme.c_str();

    if (xcursor)
        wlr_xcursor_manager_destroy(xcursor);

    xcursor = wlr_xcursor_manager_create(theme_ptr, size->as_int());
    wlr_xcursor_manager_load(xcursor, 1);

    set_cursor("default");
}

void wf_cursor::attach_device(wlr_input_device *device)
{
    wlr_cursor_attach_input_device(cursor, device);
}

void wf_cursor::detach_device(wlr_input_device *device)
{
    wlr_cursor_detach_input_device(cursor, device);
}

void wf_cursor::set_cursor(std::string name)
{
    if (name == "default")
        name = "left_ptr";

    wlr_xcursor_manager_set_cursor_image(xcursor, name.c_str(), cursor);
}

void wf_cursor::hide_cursor()
{
    wlr_cursor_set_surface(cursor, NULL, 0, 0);
}

void wf_cursor::warp_cursor(int x, int y)
{
    wlr_cursor_warp(cursor, NULL, x, y);
    wf::get_core_impl().input->update_cursor_position(get_current_time());
}

void wf_cursor::set_cursor(wlr_seat_pointer_request_set_cursor_event *ev)
{
    auto focused_surface = ev->seat_client->seat->pointer_state.focused_surface;
    auto client =
        focused_surface ? wl_resource_get_client(focused_surface->resource) : NULL;

    if (client == ev->seat_client->client &&
        !wf::get_core_impl().input->input_grabbed())
    {
        wlr_cursor_set_surface(cursor, ev->surface, ev->hotspot_x, ev->hotspot_y);
    }
}

void wf_cursor::start_held_grab(wf::surface_interface_t *surface)
{
    grabbed_surface = surface;
}

void wf_cursor::end_held_grab()
{
    if (grabbed_surface)
    {
        grabbed_surface = nullptr;

        wf::get_core_impl().input->update_cursor_position(
            get_current_time(), false);
    }
}

wf_cursor::~wf_cursor()
{
    wf::get_core().disconnect_signal("reload-config", &config_reloaded);
}
