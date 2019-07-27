#include "tablet.hpp"
#include "../core-impl.hpp"
#include "input-manager.hpp"
#include <signal-definitions.hpp>

extern "C"
{
#include <wlr/types/wlr_tablet_v2.h>
}

/* --------------------- Tablet tool implementation ------------------------- */
wf::tablet_tool_t::tablet_tool_t(wlr_tablet_tool *tool,
    wlr_tablet_v2_tablet *tablet)
{
    this->tablet_v2 = tablet;

    /* Initialize tool_v2 */
    this->tool = tool;
    this->tool->data = this;

    auto& core = wf::get_core_impl();
    this->tool_v2 = wlr_tablet_tool_create(core.protocols.tablet_v2,
        core.get_current_seat(), tool);

    /* Free memory when the tool is destroyed */
    this->on_destroy.set_callback([=] (void*) { delete this; });
    this->on_destroy.connect(&tool->events.destroy);

    /* Ungrab surface, and update focused surface if a surface is unmapped,
     * we don't want to end up with a reference to unfocused or a destroyed
     * surface. */
    on_surface_map_state_changed = [=] (signal_data_t *data)
    {
        auto ev = static_cast<_surface_map_state_changed_signal*> (data);
        if (!ev->surface->is_mapped() && ev->surface == this->grabbed_surface)
            this->grabbed_surface = nullptr;

        update_tool_position();
    };

    wf::get_core().connect_signal("_surface_mapped",
        &on_surface_map_state_changed);
    wf::get_core().connect_signal("_surface_unmapped",
        &on_surface_map_state_changed);
}

wf::tablet_tool_t::~tablet_tool_t()
{
    wf::get_core().disconnect_signal("_surface_mapped",
        &on_surface_map_state_changed);
    wf::get_core().disconnect_signal("_surface_unmapped",
        &on_surface_map_state_changed);

    tool->data = NULL;
}

void wf::tablet_tool_t::update_tool_position()
{
    auto& core = wf::get_core_impl();
    auto& input = core.input;
    auto gc = core.get_cursor_position();

    /* XXX: tablet input works only with programs, Wayfire itself doesn't do
     * anything useful with it */
    if (core.input->input_grabbed())
        return;

    /* Figure out what surface is under the tool */
    wf_pointf local; // local to the surface
    wf::surface_interface_t *surface = nullptr;
    if (this->grabbed_surface)
    {
        surface = this->grabbed_surface;
        local = get_surface_relative_coords(surface, gc);
    } else
    {
        surface = input->input_surface_at(gc, local);
    }

    set_focus(surface);

    /* If focus is a wlr surface, send position */
    wlr_surface *next_focus = surface ? surface->priv->wsurface : nullptr;
    if (next_focus && wlr_surface_accepts_tablet_v2(tablet_v2, next_focus))
        wlr_tablet_v2_tablet_tool_notify_motion(tool_v2, local.x, local.y);
}

void wf::tablet_tool_t::set_focus(wf::surface_interface_t *surface)
{
    /* Unfocus old surface */
    if (surface != this->proximity_surface && this->proximity_surface)
    {
        wlr_tablet_v2_tablet_tool_notify_proximity_out(tool_v2);
        this->proximity_surface = nullptr;
    }

    /* Set the new focus, if it is a wlr surface */
    wlr_surface *next_focus = surface ? surface->priv->wsurface : nullptr;
    if (next_focus)
    {
        this->proximity_surface = surface;
        wlr_tablet_v2_tablet_tool_notify_proximity_in(
            tool_v2, tablet_v2, next_focus);
    }
}

void wf::tablet_tool_t::passthrough_axis(wlr_event_tablet_tool_axis *ev)
{
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE)
		wlr_tablet_v2_tablet_tool_notify_pressure(tool_v2, ev->pressure);

	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE)
		wlr_tablet_v2_tablet_tool_notify_distance(tool_v2, ev->distance);

	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION)
		wlr_tablet_v2_tablet_tool_notify_rotation(tool_v2, ev->rotation);

	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER)
		wlr_tablet_v2_tablet_tool_notify_slider(tool_v2, ev->slider);

	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL)
		wlr_tablet_v2_tablet_tool_notify_wheel(tool_v2, ev->wheel_delta, 0);

    /* Update tilt, use old values if no new values are provided */
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_X)
        tilt_x = ev->tilt_x;

	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_Y)
        tilt_y = ev->tilt_y;

	if (ev->updated_axes &
        (WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y))
    {
		wlr_tablet_v2_tablet_tool_notify_tilt(tool_v2, tilt_x, tilt_y);
	}
}

void wf::tablet_tool_t::handle_tip(wlr_event_tablet_tool_tip *ev)
{
    /* Nothing to do without a proximity surface */
    if (!this->proximity_surface)
        return;

    if (ev->state == WLR_TABLET_TOOL_TIP_DOWN)
    {
        wlr_send_tablet_v2_tablet_tool_down(tool_v2);
        this->grabbed_surface = this->proximity_surface;
    } else
    {
        wlr_send_tablet_v2_tablet_tool_up(tool_v2);
        this->grabbed_surface = nullptr;
    }
}

void wf::tablet_tool_t::handle_button(wlr_event_tablet_tool_button *ev)
{
    wlr_tablet_v2_tablet_tool_notify_button(tool_v2,
        (zwp_tablet_pad_v2_button_state)ev->button,
        (zwp_tablet_pad_v2_button_state)ev->state);
}

/* ----------------------- Tablet implementation ---------------------------- */
wf::tablet_t::tablet_t(wlr_cursor *cursor, wlr_input_device *dev)
    : wf_input_device_internal(dev)
{
    this->handle = dev->tablet;
    this->handle->data = this;
    this->cursor = cursor;

    auto& core = wf::get_core_impl();
    tablet_v2 = wlr_tablet_create(core.protocols.tablet_v2,
        core.get_current_seat(), dev);

    wlr_cursor_attach_input_device(cursor, dev);
}

wf::tablet_t::~tablet_t()
{
    this->handle->data = NULL;
}

wf::tablet_tool_t *wf::tablet_t::ensure_tool(wlr_tablet_tool *tool)
{
    if (tool->data == NULL)
        new wf::tablet_tool_t(tool, tablet_v2);

    return (wf::tablet_tool_t*) tool->data;
}

void wf::tablet_t::handle_tip(wlr_event_tablet_tool_tip *ev)
{
    auto& input = wf::get_core_impl().input;
    if (input->input_grabbed())
        return;

    auto tool = ensure_tool(ev->tool);
    tool->handle_tip(ev);
}

void wf::tablet_t::handle_axis(wlr_event_tablet_tool_axis *ev)
{
    auto& input = wf::get_core_impl().input;

    /* Update cursor position */
    switch(ev->tool->type) {
        case WLR_TABLET_TOOL_TYPE_MOUSE:
            wlr_cursor_move(cursor, ev->device, ev->dx, ev->dy);
            break;
        default:
            double x = (ev->updated_axes & WLR_TABLET_TOOL_AXIS_X) ? ev->x : NAN;
            double y = (ev->updated_axes & WLR_TABLET_TOOL_AXIS_Y) ? ev->y : NAN;
            wlr_cursor_warp_absolute(cursor, ev->device, x, y);
    }

    if (input->input_grabbed())
        return;

    /* Update focus */
    auto tool = ensure_tool(ev->tool);
    tool->update_tool_position();
    tool->passthrough_axis(ev);
}

void wf::tablet_t::handle_button(wlr_event_tablet_tool_button *ev)
{
    /* Just pass through */
    ensure_tool(ev->tool)->handle_button(ev);
}

void wf::tablet_t::handle_proximity(wlr_event_tablet_tool_proximity *ev)
{
    if (ev->state == WLR_TABLET_TOOL_PROXIMITY_OUT)
    {
        /* Unfocus the tool and return */
        ensure_tool(ev->tool)->set_focus(nullptr);
    } else
    {
        wlr_cursor_warp_absolute(cursor, ev->device, ev->x, ev->y);
        ensure_tool(ev->tool)->update_tool_position();
        wf::get_core().set_cursor("crosshair");
    }
}

