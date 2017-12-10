#include <output.hpp>
#include <core.hpp>
#include <workspace-manager.hpp>
#include <signal-definitions.hpp>
#include <view.hpp>
#include <set>
#include <fstream>

using function = std::function<void()>;
void idle_callback(void *data)
{
    (*(function*)(data))();
}

class wayfire_apps_logger : public wayfire_plugin_t
{
    signal_callback_t created_cb, destroyed_cb;

    using app = std::pair<std::string, std::string>;

    void update_log_file()
    {
        std::ofstream out("/tmp/wayfire-app-list",
                          std::ofstream::trunc | std::ofstream::out);

        size_t id = 1;
        output->workspace->for_each_view([&out, &id] (wayfire_view view)
        {
            auto c_app_id = weston_desktop_surface_get_app_id(view->desktop_surface);
            std::string app_id = c_app_id ? c_app_id : "(null)";

            auto c_title = weston_desktop_surface_get_title (view->desktop_surface);
            std::string title = c_title ? c_title : "(null)";

            out << "no. " << id << " app_id: " << app_id << " title: " << title << std::endl;
            ++id;
        });
    }

    function idle_func = [=] () { update_log_file(); };

    public:
    void init(wayfire_config *config)
    {
        created_cb = [=] (signal_data *data)
        {
            wl_event_loop_add_idle(wl_display_get_event_loop(core->ec->wl_display),
                                   idle_callback, &idle_func);
        };
        output->connect_signal("create-view", &created_cb);

        destroyed_cb = [=] (signal_data *data)
        {
            wl_event_loop_add_idle(wl_display_get_event_loop(core->ec->wl_display),
                                   idle_callback, &idle_func);
        };
        output->connect_signal("destroy-view", &destroyed_cb);
    }
};

extern "C"
{
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_apps_logger;
    }
}
