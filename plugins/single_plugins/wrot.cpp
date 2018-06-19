#include "view.hpp"
#include "view-transform.hpp"
#include "output.hpp"
#include <linux/input.h>

static double cross (double x1, double y1, double x2, double y2) // cross product
{
    return x1 * y2 - x2 * y1;
}

static double vlen(double x1, double y1) // length of vector centered at the origin
{
    return std::sqrt(x1 * x1 + y1 * y1);
}

class wf_wrot : public wayfire_plugin_t
{
    button_callback call;

    int last_x, last_y;
    public:
        void init(wayfire_config *config)
        {
            call = [=] (uint32_t, int x, int y)
            {
                output->activate_plugin(grab_interface);
                grab_interface->grab();

                last_x = x;
                last_y = y;
            };

            auto button = (*config)["wrot"]->get_option("activate", "<alt> BTN_RIGHT");
            output->add_button(button, &call);

            grab_interface->callbacks.pointer.motion = [=] (int x, int y)
            {
                auto view = output->get_top_view();

                if (!view->get_transformer())
                    view->set_transformer(std::unique_ptr<wf_2D_view> (new wf_2D_view(output)));

                auto tr = dynamic_cast<wf_2D_view*> (view->get_transformer());

                view->damage();

                auto g = view->get_wm_geometry();

                double cx = g.x + g.width / 2.0;
                double cy = g.y + g.height / 2.0;

                double x1 = last_x - cx, y1 = last_y - cy;
                double x2 = x - cx, y2 = y - cy;

                /* cross(a, b) = |a| * |b| * sin(a, b) */
                tr->angle -= std::asin(cross(x1, y1, x2, y2) / vlen(x1, y1) / vlen(x2, y2));

                view->damage();

                last_x = x;
                last_y = y;
            };

            grab_interface->callbacks.pointer.button = [=] (uint32_t, uint32_t s)
            {
                if (s == WLR_BUTTON_RELEASED)
                {
                    grab_interface->ungrab();
                    output->deactivate_plugin(grab_interface);
                }
            };
        }
};

extern "C"
{
wayfire_plugin_t *newInstance()
{
    return new wf_wrot;
}
}
