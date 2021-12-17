#include <wayfire/desktop-surface.hpp>
#include <wayfire/util.hpp>

class wlr_desktop_surface_t : public wf::desktop_surface_t, wf::keyboard_surface_t
{
  public:
    wlr_desktop_surface_t(wlr_surface *surface);

    std::string get_app_id() override final;
    std::string get_title() override final;

    // Set the app id and emit the signal
    void set_app_id(std::string_view app_id);
    // Set the title and emit the signal
    void set_title(std::string_view title);

    virtual keyboard_surface_t& get_keyboard_focus() override;

    role get_role() const final;
    bool is_focuseable() const final;

    std::string title, app_id;
    role current_role = role::TOPLEVEL;
    bool keyboard_focus_enabled = true;

    // Implementation of keyboard surface
    virtual bool accepts_focus() const override;
    virtual void handle_keyboard_enter() override;
    virtual void handle_keyboard_leave() override;
    virtual void handle_keyboard_key(wlr_event_keyboard_key event) override;

    wlr_surface *main_surface;
    wf::wl_listener_wrapper on_surface_destroy;
};
