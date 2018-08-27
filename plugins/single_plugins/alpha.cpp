/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Scott Moreau
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <core.hpp>
#include <view.hpp>
#include <plugin.hpp>
#include <output.hpp>
#include "view-transform.hpp"
#include "workspace-manager.hpp"
#include <nonstd/make_unique.hpp>

class wayfire_alpha : public wayfire_plugin_t
{
    axis_callback axis_cb;

    public:
    void init(wayfire_config *config)
    {
        grab_interface->name = "alpha";
        grab_interface->abilities_mask = WF_ABILITY_CONTROL_WM;

        axis_cb = [=] (wlr_event_pointer_axis* ev)
        {
            if (!output->activate_plugin(grab_interface))
                return;

            output->deactivate_plugin(grab_interface);

            auto focus = core->get_cursor_focus();

            if (!focus)
                return;

            auto view = core->find_view(focus->get_main_surface());
            auto layer = output->workspace->get_view_layer(view);

            if (layer == WF_LAYER_BACKGROUND)
                return;

            if (ev->orientation == WLR_AXIS_ORIENTATION_VERTICAL)
                update_alpha_target(view, ev->delta);
        };

        auto section = config->get_section("alpha");
        auto modifier = section->get_option("modifier", "<alt>");

        output->add_axis(modifier, &axis_cb);
    }

    void update_alpha_target(wayfire_view view, float delta)
    {
        wf_2D_view *transformer;
        float alpha;

        if (!view->get_transformer("alpha"))
            view->add_transformer(nonstd::make_unique<wf_2D_view> (view), "alpha");

        transformer = dynamic_cast<wf_2D_view*> (view->get_transformer("alpha").get());
        alpha = transformer->alpha;

        alpha -= delta * 0.003;

        if (alpha > 1.0)
            alpha = 1.0;

        if (alpha == 1.0)
            return view->pop_transformer("alpha");

        if (alpha < 0)
            alpha = 0;

        if (transformer->alpha != alpha)
        {
            transformer->alpha = alpha;
            view->damage();
        }
    }

    void fini()
    {
        output->rem_axis(&axis_cb);
    }
};

extern "C"
{
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_alpha();
    }
}
