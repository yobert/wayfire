#pragma once

#include <string>
#include <wayfire/object.hpp>
#include <wayfire/nonstd/wlroots.hpp>

namespace wf
{
/**
 * An interface for desktop surfaces to interact with keyboard input.
 */
class keyboard_surface_t
{
  public:
    keyboard_surface_t(const keyboard_surface_t&) = delete;
    keyboard_surface_t(keyboard_surface_t&&) = delete;
    keyboard_surface_t& operator =(const keyboard_surface_t&) = delete;
    keyboard_surface_t& operator =(keyboard_surface_t&&) = delete;

    /**
     * Check whether the desktop surface wants to accept focus at all.
     *
     * The returned value does not have to remain constant for the whole
     * lifetime of the desktop surface.
     */
    virtual bool accepts_focus() const = 0;

    /**
     * Handle a keyboard enter event.
     * This means that the desktop surface is now focused.
     */
    virtual void handle_keyboard_enter() = 0;

    /**
     * Handle a keyboard leave event.
     * The desktop surface is no longer focused.
     */
    virtual void handle_keyboard_leave() = 0;

    /**
     * Handle a keyboard key event.
     *
     * These are received only after the desktop surface has received keyboard focus
     * and
     * before it loses it.
     */
    virtual void handle_keyboard_key(wlr_event_keyboard_key event) = 0;

  protected:
    keyboard_surface_t() = default;
    virtual ~keyboard_surface_t() = default;
};

/**
 * A desktop surface represents a whole tree of surfaces which have been
 * assigned a particular role. In other words, a desktop surface represents
 * a whole application toplevel window, a panel created with wlr-layer-shell,
 * etc.
 *
 * Desktop surfaces are therefore the most generic type of entities which can
 * be rendered on an output, added in an output layer, etc. The most important
 * subclass are toplevels, which only represent application windows.
 */
class desktop_surface_t : public wf::object_base_t
{
  public:
    /**
     * The role of a desktop surface gives more information about its purpose.
     */
    enum class role
    {
        /** The desktop surface is a toplevel application window. */
        TOPLEVEL,
        /**
         * The desktop surface is part of the desktop enviroment, for ex.
         * a background, a panel, a dock, etc.
         */
        DESKTOP_ENVIRONMENT,
        /**
         * The desktop surface is part of an application, but should not be
         * managed by the compositor. Typical examples are menus, tooltips.
         */
        UNMANAGED,
    };

  public:
    desktop_surface_t(const desktop_surface_t&) = delete;
    desktop_surface_t(desktop_surface_t&&) = delete;
    desktop_surface_t& operator =(const desktop_surface_t&) = delete;
    desktop_surface_t& operator =(desktop_surface_t&&) = delete;
    virtual ~desktop_surface_t() = default;

    /**
     * Get the app-id of the desktop surface.
     * The app-id is used to identify which application or class of
     * applications (for layer-shell) created the desktop surface.
     */
    virtual std::string get_app_id() = 0;

    /**
     * Get the title of the desktop surface.
     *
     * The title usually conveys additional information about the particular
     * desktop surface, especially if multiple desktop surfaces have the same
     * app-id.
     */
    virtual std::string get_title() = 0;

    /**
     * Get the role of the desktop surface.
     * The role should not change for the lifetime of the desktop surface.
     */
    virtual role get_role() const = 0;

    /**
     * Get the keyboard focus interface of this view.
     */
    virtual keyboard_surface_t& get_keyboard_focus() = 0;

    /**
     * Check whether the surface is focuseable. Note the actual ability to give
     * keyboard focus while the surface is mapped is determined by the keyboard
     * focus surface or the compositor_view implementation.
     *
     * This is meant for plugins like matcher, which need to check whether the
     * view is focuseable at any point of the view life-cycle.
     */
    virtual bool is_focuseable() const = 0;

    /**
     * Request that the client closes its desktop surface.
     *
     * The client may fulfill the request as soon as it receives the request,
     * delay the response (showing a closing dialog for example), or not
     * respond at all, if for example the client is stuck.
     */
    virtual void close() = 0;

    /**
     * Ping the desktop surface's client.
     * If the ping request times out, `ping-timeout` event will be emitted.
     */
    virtual void ping() = 0;

  protected:
    desktop_surface_t() = default;
};
}
