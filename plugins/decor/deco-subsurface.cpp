extern "C"
{
#define static
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_matrix.h>
#undef static
}

#include <linux/input-event-codes.h>

#include <compositor-surface.hpp>
#include <output.hpp>
#include <core.hpp>
#include <debug.hpp>
#include <decorator.hpp>
#include <signal-definitions.hpp>
#include "deco-subsurface.hpp"

#include <cairo.h>

GLuint get_text_texture(std::string text)
{
}

class simple_decoration_surface : public wayfire_compositor_surface_t, public wf_decorator_frame_t
{
    const int normal_thickness = 15;
    int thickness = normal_thickness;
    wayfire_view view;

    protected:
        virtual void damage(const wlr_box& box)
        {
            assert(false);
        }

        virtual void get_child_position(int &x, int &y)
        {
            view->get_child_offset(x, y);
            x -= thickness;
            y -= thickness;
        }

    public:
        simple_decoration_surface(wayfire_view view)
        {
            this->view = view;
        }

        virtual ~simple_decoration_surface()
        {
        }

        virtual bool is_mapped()
        {
            return true;
        }

        int width = 100, height = 100;
        virtual wf_geometry get_output_geometry()
        {
            auto pos = get_output_position();
            return {pos.x, pos.y, width, height};
        }

        bool active = true; // when views are mapped, they are usually activated
        float border_color[4] = {0.15f, 0.15f, 0.15f, 0.8f};
        float border_color_inactive[4] = {0.25f, 0.25f, 0.25f, 0.95f};

        virtual void _wlr_render_box(const wlr_fb_attribs& fb, int x, int y, const wlr_box& scissor)
        {
            wlr_box geometry {x, y, width, height};
            geometry = get_output_box_from_box(geometry, output->handle->scale);

            float projection[9];
            wlr_matrix_projection(projection, fb.width, fb.height, fb.transform);

            float matrix[9];
            wlr_matrix_project_box(matrix, &geometry, WL_OUTPUT_TRANSFORM_NORMAL, 0, projection);

            wlr_renderer_begin(core->renderer, fb.width, fb.height);
            auto sbox = scissor; wlr_renderer_scissor(core->renderer, &sbox);

            wlr_render_quad_with_matrix(core->renderer, active ? border_color : border_color_inactive, matrix);
            wlr_renderer_end(core->renderer);
        }

        virtual void _render_pixman(const wlr_fb_attribs& fb, int x, int y, pixman_region32_t* damage)
        {
            const float scale = output->handle->scale;

            pixman_region32_t frame_region;
            pixman_region32_init(&frame_region);

            /* top */
            pixman_region32_union_rect(&frame_region, &frame_region, x * scale, y * scale, width * scale,     thickness * scale);
            /* left */
            pixman_region32_union_rect(&frame_region, &frame_region, x * scale, y * scale, thickness * scale, height * scale);
            /* right */
            pixman_region32_union_rect(&frame_region, &frame_region, x * scale + (width - thickness) * scale, y * scale,
                                       thickness * scale, height * scale);
            /* bottom */
            pixman_region32_union_rect(&frame_region, &frame_region, x * scale, y * scale + (height - thickness) * scale,
                                       width * scale, thickness * scale);

            pixman_region32_intersect(&frame_region, &frame_region, damage);
            wayfire_surface_t::_render_pixman(fb, x, y, &frame_region);
            pixman_region32_fini(&frame_region);
        }

        virtual void render_fb(pixman_region32_t* damage, wf_framebuffer fb)
        {
            GL_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb.fb));
            auto obox = get_output_geometry();

            wlr_fb_attribs attribs;
            attribs.width = output->handle->width;
            attribs.height = output->handle->height;
            attribs.transform = output->handle->transform;

            render_pixman(attribs, obox.x - fb.geometry.x, obox.y - fb.geometry.y, damage);
        }

        /* all input events coordinates are surface-local */

        /* override this if you want to get pointer events or to stop input passthrough */
        virtual bool accepts_input(int32_t sx, int32_t sy)
        {
            return (0 <= sx && sx < width) && (0 <= sy && sy < height);
        }

        virtual void on_pointer_enter(int x, int y)
        {
            core->set_default_cursor();
        }

        virtual void on_pointer_leave()
        { }

        virtual void on_pointer_motion(int x, int y)
        {
            /* TODO: handle this when buttons are added */
        }

        virtual void on_pointer_button(uint32_t button, uint32_t state)
        {
            if (button == BTN_LEFT && state == WLR_BUTTON_PRESSED)
            {
                resize_request_signal resize_request;
                resize_request.view = view;
                output->emit_signal("resize-request", &resize_request);
            }
        }

        /* TODO: add touch events */


        /* frame implementation */
        virtual wf_geometry expand_wm_geometry(wf_geometry contained_wm_geometry)
        {
            contained_wm_geometry.x -= thickness;
            contained_wm_geometry.y -= thickness;
            contained_wm_geometry.width += 2 * thickness;
            contained_wm_geometry.height += 2 * thickness;

            return contained_wm_geometry;
        }

        virtual void calculate_resize_size(int& target_width, int& target_height)
        {
            target_width -= 2 * thickness;
            target_height -= 2 * thickness;
        }

        virtual void notify_view_activated(bool active)
        {
            if (this->active != active)
                view->damage();

            this->active = active;
        }

        virtual void notify_view_resized(wf_geometry view_geometry)
        {
            width = view_geometry.width;
            height = view_geometry.height;

            view->damage();
        };

        virtual void notify_view_maximized()
        {

        }

        virtual void notify_view_fullscreened()
        {
            if (view->fullscreen)
            {
                thickness = 0;
                view->resize(width, height);
            } else
            {
                thickness = normal_thickness;
                view->resize(width, height);
            }
        };
};

void init_view(wayfire_view view)
{
    auto surf = new simple_decoration_surface(view);
    view->set_decoration(surf);
    view->damage();
}
