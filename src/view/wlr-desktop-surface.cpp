#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/core.hpp>
#include "wlr-desktop-surface.hpp"

wlr_desktop_surface_t::wlr_desktop_surface_t(wlr_surface *surface)
{
    this->main_surface = surface;
    this->on_surface_destroy.connect(&surface->events.destroy);
    this->on_surface_destroy.set_callback([&] (void*)
    {
        this->main_surface = nullptr;
    });
}

bool wlr_desktop_surface_t::accepts_focus() const
{
    return keyboard_focus_enabled && this->main_surface;
}

void wlr_desktop_surface_t::handle_keyboard_enter()
{
    auto seat = wf::get_core().get_current_seat();
    auto kbd  = wlr_seat_get_keyboard(seat);
    wlr_seat_keyboard_notify_enter(seat,
        this->main_surface,
        kbd ? kbd->keycodes : NULL,
        kbd ? kbd->num_keycodes : 0,
        kbd ? &kbd->modifiers : NULL);
}

void wlr_desktop_surface_t::handle_keyboard_leave()
{
    auto seat = wf::get_core().get_current_seat();
    wlr_seat_keyboard_clear_focus(seat);
}

void wlr_desktop_surface_t::handle_keyboard_key(wlr_event_keyboard_key event)
{
    auto seat = wf::get_core().get_current_seat();
    wlr_seat_keyboard_notify_key(seat,
        event.time_msec, event.keycode, event.state);
}

wf::keyboard_surface_t& wlr_desktop_surface_t::get_keyboard_focus()
{
    return *this;
}

wf::desktop_surface_t::role wlr_desktop_surface_t::get_role() const
{
    return current_role;
}

bool wlr_desktop_surface_t::is_focuseable() const
{
    return keyboard_focus_enabled;
}

void wlr_desktop_surface_t::set_app_id(std::string_view new_app_id)
{
    this->app_id = new_app_id;

    wf::app_id_changed_signal data;
    data.dsurf = this;
    emit_signal("app-id-changed", &data);
}

void wlr_desktop_surface_t::set_title(std::string_view new_title)
{
    this->title = new_title;

    wf::title_changed_signal data;
    data.dsurf = this;
    emit_signal("title-changed", &data);
}

std::string wlr_desktop_surface_t::get_app_id()
{
    return this->app_id;
}

std::string wlr_desktop_surface_t::get_title()
{
    return this->title;
}
