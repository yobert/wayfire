extern "C"
{
#define static
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_matrix.h>
#undef static
}

#include <compositor-surface.hpp>
#include <output.hpp>
#include <core.hpp>
#include <debug.hpp>
#include "deco-subsurface.hpp"

class simple_decoration_surface : public wayfire_compositor_surface_t
{
    protected:
        virtual void damage(const wlr_box& box)
        {
            assert(false);
        }

        virtual void get_child_position(int &x, int &y)
        {
            x = y = 100;
        }

    public:
        simple_decoration_surface()
        {
            inc_keep_count();
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

        virtual void render_pixman(const wlr_fb_attribs& fb, int x, int y, pixman_region32_t* damage)
        {
            wlr_box geometry {x, y, width, height};
            geometry = get_output_box_from_box(geometry, output->handle->scale);

            float projection[9];
            wlr_matrix_projection(projection, fb.width, fb.height, fb.transform);

            float matrix[9];
            wlr_matrix_project_box(matrix, &geometry, WL_OUTPUT_TRANSFORM_NORMAL, 0, projection);

            wlr_renderer_begin(core->renderer, fb.width, fb.height);
            wlr_renderer_scissor(core->renderer, NULL);
            float color[] = {1, 1, 1, 1};
            wlr_render_quad_with_matrix(core->renderer, color, matrix);
            wlr_renderer_end(core->renderer);
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
            log_info("pointer enter");
        }

        virtual void on_pointer_leave()
        {
            log_info("pointer leave");
        }

        virtual void on_pointer_motion(int x, int y)
        {
            log_info("pointer motion");
        }

        virtual void on_pointer_button(uint32_t button, uint32_t state)
        {
            log_info("pointer button");
        }

        /* TODO: add touch events */
};

void init_view(wayfire_view view)
{
    auto surf = new simple_decoration_surface;
    surf->parent_surface = view.get();
    view->surface_children.push_back(surf);
    surf->set_output(view->get_output());
    view->damage();
}
