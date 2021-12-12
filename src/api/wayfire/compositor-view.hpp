#pragma once

#include <wayfire/view.hpp>
#include <wayfire/config/types.hpp>

namespace wf
{
/**
 * Input surface implementation which denies all input.
 */
class no_input_surface_t : public wf::input_surface_t
{
  public:
    no_input_surface_t() = default;

    bool accepts_input(wf::pointf_t at) final;
    std::optional<wf::region_t> handle_pointer_enter(
        wf::pointf_t at, bool refocus) final;
    void handle_pointer_leave() final;
    void handle_pointer_button(
        uint32_t time_ms, uint32_t button, wlr_button_state state) final;
    void handle_pointer_motion(uint32_t time_ms, wf::pointf_t at) final;
    void handle_pointer_axis(uint32_t time_ms, wlr_axis_orientation orientation,
        double delta, int32_t delta_discrete, wlr_axis_source source) final;

    void handle_touch_down(uint32_t time_ms, int32_t id, wf::pointf_t at) final;
    void handle_touch_up(uint32_t time_ms, int32_t id, bool finger_lifted) final;
    void handle_touch_motion(uint32_t time_ms, int32_t id, wf::pointf_t at) final;
};

class no_keyboard_input_surface_t : public wf::keyboard_surface_t
{
  public:
    no_keyboard_input_surface_t() = default;

    bool accepts_focus() const final;
    void handle_keyboard_enter() final;
    void handle_keyboard_leave() final;
    void handle_keyboard_key(wlr_event_keyboard_key event) final;
};

/**
 * A desktop_surface_t implementation for compositor surfaces.
 * By default, compositor surfaces do not receive keyboard focus,
 * have a fixed title and app-id and an UNMANAGED role.
 */
class compositor_desktop_surface_t : public wf::desktop_surface_t
{
  public:
    compositor_desktop_surface_t(std::string_view title, std::string_view app_id);

    std::string get_app_id() override;
    std::string get_title() override;
    role get_role() const override;
    keyboard_surface_t& get_keyboard_focus() override;
    bool is_focuseable() const override;

    // Default implementation works only for color views
    void close() override;
    void ping() override;

  protected:
    std::string title;
    std::string app_id;
};

/**
 * Output surface implementation which provides a solid color rectangle with
 * a border.
 */
class solid_bordered_surface_t : public wf::output_surface_t,
    public wf::no_input_surface_t, public wf::surface_interface_t
{
  public:
    /**
     * Create a new solid bordered surface.
     */
    solid_bordered_surface_t() = default;

    wf::point_t get_offset() override;
    wf::dimensions_t get_size() const final;
    void schedule_redraw(const timespec& frame_end) final;
    void set_visible_on_output(wf::output_t *output, bool is_visible) override;
    wf::region_t get_opaque_region() final;
    void simple_render(const wf::framebuffer_t& fb, wf::point_t pos,
        const wf::region_t& damage) final;

    /** Set the rectangle size. */
    void set_size(wf::dimensions_t new_size);
    /** Set the view color. Color's alpha is not premultiplied */
    void set_color(wf::color_t color);
    /** Set the view border color. Color's alpha is not premultiplied */
    void set_border_color(wf::color_t border);
    /** Set the border width. */
    void set_border(int width);

    /** Unmap the surface. */
    void unmap();

    bool is_mapped() const final;
    input_surface_t& input() final;
    output_surface_t& output() final;

  public: // Read-only for the outside
    wf::color_t _color = {0, 0, 0, 0};
    wf::color_t _border_color = {0, 0, 0, 0};
    int border = 0;

  protected:
    bool mapped = true;
    wf::dimensions_t size = {1, 1};
};

/**
 * color_rect_view_t represents another common type of compositor view - a
 * view which is simply a colored rectangle with a border.
 */
class color_rect_view_t : public wf::view_interface_t,
    public wf::no_keyboard_input_surface_t
{
    wf::point_t position = {0, 0};
    bool _is_mapped = true;
    std::shared_ptr<wf::solid_bordered_surface_t> color_surface;

  public:
    /**
     * Create a colored rect view. The map signal is not fired by default.
     * The creator of the colored view should also add it to the desired layer.
     */
    color_rect_view_t();

    /**
     * Get the colored surface of this view.
     */
    nonstd::observer_ptr<solid_bordered_surface_t> get_color_surface() const;

    /* required for view_interface_t */
    virtual void move(int x, int y) override;
    virtual void resize(int w, int h) override;
    virtual wf::geometry_t get_output_geometry() override;
    virtual bool should_be_decorated() override;
};
}
