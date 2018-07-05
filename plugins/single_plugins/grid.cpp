#include <output.hpp>
#include <core.hpp>
#include <debug.hpp>
#include <view.hpp>
#include <workspace-manager.hpp>
#include <render-manager.hpp>
#include <algorithm>
#include <linux/input-event-codes.h>
#include "signal-definitions.hpp"
#include <nonstd/make_unique.hpp>
#include <animation.hpp>

#include "snap_signal.hpp"
#include "../wobbly/wobbly-signal.hpp"

const std::string grid_view_id = "grid-view";
class wayfire_grid_view : public wf_custom_view_data
{
    wf_duration duration;
    bool is_active = true;

    wayfire_view view;
    wayfire_output *output;
    effect_hook_t pre_hook;
    signal_callback_t unmapped;

    bool tiled;
    wf_geometry target, initial;
    wayfire_grab_interface iface;
    wf_option animation_type;

    public:

    wayfire_grid_view(wayfire_view view, wayfire_grab_interface iface,
                      wf_option animation_type, wf_option animation_duration)
        {
            this->view = view;
            this->output = view->get_output();
            this->iface = iface;
            this->animation_type = animation_type;
            duration = wf_duration(animation_duration);

            if (!view->get_output()->activate_plugin(iface))
            {
                is_active = false;
                return;
            }

            pre_hook = [=] () {
                adjust_geometry();
            };
            output->render->add_effect(&pre_hook, WF_OUTPUT_EFFECT_PRE);

            unmapped = [=] (signal_data *data)
            {
                if (get_signaled_view(data) == view)
                    view->custom_data.erase(grid_view_id);
            };
            output->connect_signal("unmap-view", &unmapped);
            output->connect_signal("detach-view", &unmapped);
        }

        void destroy()
        {
            view->custom_data.erase(grid_view_id);
        }

        void adjust_target_geometry(wf_geometry geometry, bool tiled)
        {
            target = geometry;
            initial = view->get_wm_geometry();
            this->tiled = tiled;

            log_info("adjust geometry");

            auto type = animation_type->as_string();
            if (output->is_plugin_active("wobbly") || !is_active)
                type = "wobbly";

            if (type == "none")
            {
                view->set_maximized(tiled);
                view->set_geometry(geometry);

                return destroy();
            }

            if (type == "wobbly")
            {
                snap_wobbly(view, geometry);
                view->set_maximized(tiled);
                view->set_geometry(geometry);

                if (!tiled) // release snap, so subsequent size changes don't bother us
                    snap_wobbly(view, geometry, false);

                return destroy();
            }

            view->set_maximized(1);
            view->set_moving(1);
            view->set_resizing(1);
            duration.start();
        }

        void adjust_geometry()
        {
            log_info("adjust");
            if (!duration.running())
            {
                log_info("direct end %d", tiled);
                view->set_geometry(target);
                view->set_maximized(tiled);
                view->set_moving(0);
                view->set_resizing(0);

                return destroy();
            }

            int cx = duration.progress(initial.x, target.x);
            int cy = duration.progress(initial.y, target.y);
            int cw = duration.progress(initial.width, target.width);
            int ch = duration.progress(initial.height, target.height);

            log_info("step %d@%d %dx%d", cx, cy, cw, ch);

            view->set_geometry({cx, cy, cw, ch});
        }

        ~wayfire_grid_view()
        {
            if (!is_active)
                return;

            output->render->rem_effect(&pre_hook, WF_OUTPUT_EFFECT_PRE);
            output->deactivate_plugin(iface);
            output->disconnect_signal("unmap-view", &unmapped);
            output->disconnect_signal("detach-view", &unmapped);
        }
};

wayfire_grid_view *ensure_grid_view(wayfire_view view, wayfire_grab_interface iface,
                      wf_option animation_type, wf_option animation_duration)
{
    if (view->custom_data.count(grid_view_id))
        return static_cast<wayfire_grid_view*> (view->custom_data[grid_view_id].get());

    auto saved = nonstd::make_unique<wayfire_grid_view> (view, iface, animation_type, animation_duration);
    auto ret = saved.get();
    view->custom_data[grid_view_id] = std::move(saved);

    return ret;
}

const std::string grid_saved_pos_id = "grid-saved-pos";
class saved_view_geometry : public wf_custom_view_data
{
    public:
    wf_geometry geometry;
    bool was_maximized; // used by fullscreen-request
};

bool has_saved_position(wayfire_view view, std::string suffix = "")
{
    return view->custom_data.count(grid_saved_pos_id + suffix);
}

saved_view_geometry *ensure_saved_geometry(wayfire_view view, std::string suffix = "")
{
    if (has_saved_position(view, suffix))
        return static_cast<saved_view_geometry*> (view->custom_data[grid_saved_pos_id + suffix].get());

    auto saved = nonstd::make_unique<saved_view_geometry> ();
    auto ret = saved.get();
    view->custom_data[grid_saved_pos_id + suffix] = std::move(saved);

    return ret;
}

void erase_saved(wayfire_view view, std::string suffix = "")
{
    view->custom_data.erase(grid_saved_pos_id + suffix);
}

class wayfire_grid : public wayfire_plugin_t
{
    signal_callback_t output_resized_cb, view_destroyed_cb;


    std::vector<std::string> slots = {"unused", "bl", "b", "br", "l", "c", "r", "tl", "t", "tr"};
    std::vector<std::string> default_keys = {
        "none",
        "<alt> <ctrl> KEY_KP1",
        "<alt> <ctrl> KEY_KP2",
        "<alt> <ctrl> KEY_KP3",
        "<alt> <ctrl> KEY_KP4",
        "<alt> <ctrl> KEY_KP5",
        "<alt> <ctrl> KEY_KP6",
        "<alt> <ctrl> KEY_KP7",
        "<alt> <ctrl> KEY_KP8",
        "<alt> <ctrl> KEY_KP9",
    };
    key_callback bindings[10];
    wf_option keys[10];

    signal_callback_t snap_cb, maximized_cb, fullscreen_cb;

    wf_option animation_duration, animation_type;

    public:
    void init(wayfire_config *config)
    {
        grab_interface->name = "grid";
        grab_interface->abilities_mask = WF_ABILITY_CHANGE_VIEW_GEOMETRY;

        auto section = config->get_section("grid");
        animation_duration = section->get_option("duration", "300");
        animation_type = section->get_option("type", "simple");

        for (int i = 1; i < 10; i++) {
            keys[i] = section->get_option("slot_" + slots[i], default_keys[i]);

            bindings[i] = [=] (uint32_t key) {
                handle_key(output->get_active_view(), i);
            };

            output->add_key(keys[i], &bindings[i]);
        }

        using namespace std::placeholders;
        snap_cb = std::bind(std::mem_fn(&wayfire_grid::snap_signal_cb), this, _1);
        output->connect_signal("view-snap", &snap_cb);

        maximized_cb = std::bind(std::mem_fn(&wayfire_grid::maximize_signal_cb), this, _1);
        output->connect_signal("view-maximized-request", &maximized_cb);

        fullscreen_cb = std::bind(std::mem_fn(&wayfire_grid::fullscreen_signal_cb), this, _1);
        output->connect_signal("view-fullscreen-request", &fullscreen_cb);
    }

    void handle_key(wayfire_view view, int key)
    {
        wf_geometry target = get_slot_dimensions(key, output->workspace->get_workarea());
        bool tiled = true;

        if (view->maximized && view->get_wm_geometry() == target)
        {
            return;
        }
        else if (!has_saved_position(view) && key)
        {
            ensure_saved_geometry(view)->geometry = view->get_wm_geometry();
        } else if (has_saved_position(view) && key == 0)
        {
            tiled = false;
            target = calculate_restored_geometry(ensure_saved_geometry(view)->geometry);
            erase_saved(view);
        } else if (!has_saved_position(view))
        {
            return;
        }

        auto gv = ensure_grid_view(view, grab_interface, animation_type, animation_duration);
        gv->adjust_target_geometry(target, tiled);
    }

    /* calculates the target geometry so that it is centered around the pointer */
    wf_geometry calculate_restored_geometry(wf_geometry base_restored)
    {
        GetTuple(cx, cy, output->get_cursor_position());
        base_restored.x = cx - base_restored.width / 2;
        base_restored.y = cy - base_restored.height / 2;

        /* if the view goes outside of the workarea, try to move it back inside */
        auto wa = output->workspace->get_workarea();
        if (base_restored.x + base_restored.width > wa.x + wa.width)
            base_restored.x = wa.x + wa.width - base_restored.width;
        if (base_restored.y + base_restored.height > wa.y + wa.width)
            base_restored.y = wa.y + wa.height - base_restored.width;
        if (base_restored.x < wa.x)
            base_restored.x = wa.x;
        if (base_restored.y < wa.y)
            base_restored.y = wa.y;


        return base_restored;
    }

    /*
     * 7 8 9
     * 4 5 6
     * 1 2 3
     * */
    wf_geometry get_slot_dimensions(int n, wf_geometry area)
    {
        if (n == 0) // toggle off slot
            return {0, 0, 0, 0};

        int w2 = area.width / 2;
        int h2 = area.height / 2;

        if (n % 3 == 1)
            area.width = w2;
        if (n % 3 == 0)
            area.width = w2, area.x += w2;

        if (n >= 7)
            area.height = h2;
        else if (n <= 3)
            area.height = h2, area.y += h2;

        return area;
    }

    void snap_signal_cb(signal_data *ddata)
    {
        snap_signal *data = static_cast<snap_signal*>(ddata);
        handle_key(data->view, data->tslot);
    }

    void maximize_signal_cb(signal_data *ddata)
    {
        auto data = static_cast<view_maximized_signal*> (ddata);
        handle_key(data->view, data->state ? 5 : 0);
    }

    void fullscreen_signal_cb(signal_data *ddata)
    {
        auto data = static_cast<view_fullscreen_signal*> (ddata);

        if (data->state)
        {
            if (!has_saved_position(data->view, "-fs"))
            {
                auto sg = ensure_saved_geometry(data->view, "-fs");
                sg->geometry = data->view->get_wm_geometry();
                sg->was_maximized = data->view->maximized;
            }

            auto gv = ensure_grid_view(data->view, grab_interface, animation_type, animation_duration);
            gv->adjust_target_geometry(output->get_relative_geometry(), true);

            data->view->set_fullscreen(true);
        } else
        {
            if (has_saved_position(data->view, "-fs"))
            {
                auto sg = ensure_saved_geometry(data->view, "-fs");
                auto target_geometry = sg->geometry;
                auto maximized = sg->was_maximized;
                erase_saved(data->view, "-fs");

                auto gv = ensure_grid_view(data->view, grab_interface, animation_type, animation_duration);
                gv->adjust_target_geometry(target_geometry, maximized);
            }

            data->view->set_fullscreen(false);
        }
    }

    void fini()
    {
        for (int i = 1; i < 10; i++)
            output->rem_key(&bindings[i]);

        output->disconnect_signal("view-snap", &snap_cb);
        output->disconnect_signal("view-maximized-request", &maximized_cb);
        output->disconnect_signal("view-fullscreen-request", &fullscreen_cb);
        output->disconnect_signal("output-resized", &output_resized_cb);
        output->disconnect_signal("unmap-view", &view_destroyed_cb);
        output->disconnect_signal("detach-view", &view_destroyed_cb);
    }
};

extern "C" {
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_grid;
    }
}
