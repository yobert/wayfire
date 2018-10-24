#include "wm.hpp"
#include "output.hpp"
#include "view.hpp"
#include "debug.hpp"
#include "core.hpp"
#include "workspace-manager.hpp"
#include <linux/input.h>
#include "signal-definitions.hpp"

void wayfire_exit::init(wayfire_config*)
{
    key = [](uint32_t key)
    {
        wl_display_terminate(core->display);
    };

    output->add_key(new_static_option("<ctrl> <alt> KEY_BACKSPACE"), &key);
}

void wayfire_close::init(wayfire_config *config)
{
    grab_interface->abilities_mask = WF_ABILITY_GRAB_INPUT;
    auto key = config->get_section("core")->get_option("view_close", "<super> KEY_Q");
    callback = [=] (uint32_t key)
    {
        if (!output->activate_plugin(grab_interface))
            return;

        output->deactivate_plugin(grab_interface);
        auto view = output->get_active_view();
        if (view) view->close();
    };

    output->add_key(key, &callback);
}

void wayfire_focus::init(wayfire_config *)
{
    grab_interface->name = "_wf_focus";
    grab_interface->abilities_mask = WF_ABILITY_CHANGE_VIEW_GEOMETRY;

    const auto check_focus_view = [=] (wayfire_surface_t *focus)
    {
        if (!focus)
            return;

        auto main_surface = focus->get_main_surface();
        wayfire_view view = nullptr;

        if (!main_surface || !(view = core->find_view(main_surface)))
            return;

        if (!view->is_mapped() || !output->activate_plugin(grab_interface))
            return;

        output->deactivate_plugin(grab_interface);
        view->get_output()->focus_view(view);
    };

    callback = [=] (uint32_t button, int x, int y)
    {
        check_focus_view(core->get_cursor_focus());
    };

    output->add_button(new_static_option("BTN_LEFT"), &callback);
    touch = [=] (int x, int y)
    {
        log_info("got binding");
        check_focus_view(core->get_touch_focus());
    };

    output->add_touch(new_static_option(""), &touch);
}

void wayfire_handle_focus_parent::focus_view(wayfire_view view)
{
    last_view = view;
    view->get_output()->bring_to_front(view);
    for (auto child : view->children)
        focus_view(child);
}

void wayfire_handle_focus_parent::init(wayfire_config*)
{
    focus_event = [&] (signal_data *data)
    {
        auto view = get_signaled_view(data);
        if (!view || intercept_recursion)
            return;


        auto to_focus = view;
        while(to_focus->parent)
            to_focus = to_focus->parent;

        focus_view(to_focus);

        /* because output->focus_view() will fire focus-view signal again,
         * we use this flag to know that this is happening and don't fall
         * into the depths of the infinite recursion */
        intercept_recursion = true;
        output->focus_view(last_view);
        intercept_recursion = false;

        /* free shared_ptr reference */
        last_view.reset();
    };
    output->connect_signal("focus-view", &focus_event);
}
