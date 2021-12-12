#include <wayfire/core.hpp>
#include <wayfire/output.hpp>
#include <wayfire/opengl.hpp>
#include <wayfire/compositor-view.hpp>
#include <wayfire/signal-definitions.hpp>
#include <cstring>

#include <glm/gtc/matrix_transform.hpp>

/* Implementation of color_rect_view_t */

wf::color_rect_view_t::color_rect_view_t() : wf::view_interface_t(nullptr)
{
    this->color_surface = std::make_shared<wf::solid_bordered_surface_t>();
    this->set_main_surface(color_surface);
    auto desktop_surface =
        std::make_shared<compositor_desktop_surface_t>("compositor", "compositor");
    this->set_desktop_surface(desktop_surface);

    this->_is_mapped = true;
}

static void render_colored_rect(const wf::framebuffer_t& fb,
    int x, int y, int w, int h, const wf::color_t& color)
{
    wf::color_t premultiply{
        color.r * color.a,
        color.g * color.a,
        color.b * color.a,
        color.a};

    OpenGL::render_rectangle({x, y, w, h}, premultiply,
        fb.get_orthographic_projection());
}

void wf::color_rect_view_t::move(int x, int y)
{
    damage();
    view_geometry_changed_signal data;
    data.old_geometry = get_wm_geometry();
    this->position    = {x, y};

    damage();
    emit_signal("geometry-changed", &data);
}

void wf::color_rect_view_t::resize(int w, int h)
{
    view_geometry_changed_signal data;
    data.old_geometry = get_wm_geometry();

    // This will also apply the damage
    color_surface->set_size({w, h});

    emit_signal("geometry-changed", &data);
}

wf::geometry_t wf::color_rect_view_t::get_output_geometry()
{
    return wf::construct_box(position, color_surface->output().get_size());
}

bool wf::color_rect_view_t::should_be_decorated()
{
    return false;
}

void wf::no_input_surface_t::handle_pointer_axis(uint32_t time_ms,
    wlr_axis_orientation orientation, double delta, int32_t delta_discrete,
    wlr_axis_source source)
{}

void wf::no_input_surface_t::handle_pointer_motion(
    uint32_t time_ms, wf::pointf_t at)
{}

void wf::no_input_surface_t::handle_pointer_button(
    uint32_t time_ms, uint32_t button, wlr_button_state state)
{}

void wf::no_input_surface_t::handle_pointer_leave()
{}

std::optional<wf::region_t> wf::no_input_surface_t::handle_pointer_enter(
    wf::pointf_t at, bool refocus)
{
    return {};
}

void wf::no_input_surface_t::handle_touch_down(
    uint32_t time_ms, int32_t id, wf::pointf_t at)
{}

void wf::no_input_surface_t::handle_touch_up(
    uint32_t time_ms, int32_t id, bool finger_lifted)
{}

void wf::no_input_surface_t::handle_touch_motion(
    uint32_t time_ms, int32_t id, wf::pointf_t at)
{}

bool wf::no_input_surface_t::accepts_input(wf::pointf_t at)
{
    return false;
}

void wf::solid_bordered_surface_t::set_border(int width)
{
    this->border = width;
    emit_damage(wf::construct_box({0, 0}, get_size()));
}

void wf::solid_bordered_surface_t::set_border_color(wf::color_t border)
{
    this->_border_color = border;
    emit_damage(wf::construct_box({0, 0}, get_size()));
}

void wf::solid_bordered_surface_t::set_color(wf::color_t color)
{
    this->_color = color;
    emit_damage(wf::construct_box({0, 0}, get_size()));
}

void wf::solid_bordered_surface_t::set_size(wf::dimensions_t new_size)
{
    emit_damage(wf::construct_box({0, 0}, get_size()));
    this->size = new_size;
    emit_damage(wf::construct_box({0, 0}, get_size()));
}

wf::point_t wf::solid_bordered_surface_t::get_offset()
{
    return {0, 0};
}

wf::dimensions_t wf::solid_bordered_surface_t::get_size() const
{
    return size;
}

void wf::solid_bordered_surface_t::schedule_redraw(const timespec& frame_end)
{}

void wf::solid_bordered_surface_t::set_visible_on_output(
    wf::output_t *output, bool is_visible)
{
    /* no-op */ }

wf::region_t wf::solid_bordered_surface_t::get_opaque_region()
{
    return {};
}

void wf::solid_bordered_surface_t::simple_render(
    const wf::framebuffer_t& fb, wf::point_t pos, const wf::region_t& damage)
{
    OpenGL::render_begin(fb);
    for (const auto& box : damage)
    {
        fb.logic_scissor(wlr_box_from_pixman_box(box));

        /* Draw the border, making sure border parts don't overlap, otherwise
         * we will get wrong corners if border has alpha != 1.0 */
        // top
        render_colored_rect(fb, pos.x, pos.y, size.width, border,
            _border_color);
        // bottom
        render_colored_rect(fb, pos.x, pos.y + size.height - border,
            size.width, border, _border_color);
        // left
        render_colored_rect(fb, pos.x, pos.y + border, border,
            size.height - 2 * border, _border_color);
        // right
        render_colored_rect(fb, pos.x + size.width - border,
            pos.y + border, border, size.height - 2 * border, _border_color);

        /* Draw the inside of the rect */
        render_colored_rect(fb, pos.x + border, pos.y + border,
            size.width - 2 * border, size.height - 2 * border, _color);
    }

    OpenGL::render_end();
}

bool wf::no_keyboard_input_surface_t::accepts_focus() const
{
    return false;
}

void wf::no_keyboard_input_surface_t::handle_keyboard_enter()
{}

void wf::no_keyboard_input_surface_t::handle_keyboard_leave()
{}

void wf::no_keyboard_input_surface_t::handle_keyboard_key(
    wlr_event_keyboard_key event)
{}

bool wf::solid_bordered_surface_t::is_mapped() const
{
    return mapped;
}

wf::input_surface_t& wf::solid_bordered_surface_t::input()
{
    return *this;
}

wf::output_surface_t& wf::solid_bordered_surface_t::output()
{
    return *this;
}

nonstd::observer_ptr<wf::solid_bordered_surface_t> wf::color_rect_view_t::
get_color_surface() const
{
    return {this->color_surface.get()};
}

void wf::solid_bordered_surface_t::unmap()
{
    this->mapped = false;
    emit_map_state_change(this);
}

wf::compositor_desktop_surface_t::compositor_desktop_surface_t(
    std::string_view title, std::string_view app_id)
{
    this->title  = title;
    this->app_id = app_id;
}

std::string wf::compositor_desktop_surface_t::get_app_id()
{
    return app_id;
}

std::string wf::compositor_desktop_surface_t::get_title()
{
    return title;
}

wf::desktop_surface_t::role wf::compositor_desktop_surface_t::get_role() const
{
    return role::UNMANAGED;
}

wf::keyboard_surface_t& wf::compositor_desktop_surface_t::get_keyboard_focus()
{
    static no_keyboard_input_surface_t no_focus;
    return no_focus;
}

bool wf::compositor_desktop_surface_t::is_focuseable() const
{
    return false;
}

void wf::compositor_desktop_surface_t::ping()
{
    // Nothing, we don't need to emit ping-timeout ever for compositor surfaces
}

void wf::compositor_desktop_surface_t::close()
{
    auto views = wf::get_core().find_views_with_dsurface(this);
    for (auto v : views)
    {
        auto cview = dynamic_cast<color_rect_view_t*>(v.get());
        emit_view_unmap(v);

        cview->get_color_surface()->unmap();
        cview->unref();
    }
}

