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
        std::ofstream out("/tmp/wayfire-app-list-" + std::to_string(output->id),
                          std::ofstream::trunc | std::ofstream::out);

        size_t id = 1;
        output->workspace->for_each_view([&out, &id] (wayfire_view view)
        {
            if (view->is_mapped())
            out << "no. " << id << " app_id: " << view->get_app_id() << " title: " << view->get_title() << std::endl;
            ++id;
        }, WF_LAYER_WORKSPACE);
    }

    function idle_func = [=] () { update_log_file(); };

    public:
    void init(wayfire_config *config)
    {
        created_cb = [=] (signal_data *data)
        {
            wl_event_loop_add_idle(core->ev_loop, idle_callback, &idle_func);
        };
        output->connect_signal("map-view", &created_cb);

        destroyed_cb = [=] (signal_data *data)
        {
            wl_event_loop_add_idle(core->ev_loop, idle_callback, &idle_func);
        };
        output->connect_signal("unmap-view", &destroyed_cb);
    }
};

extern "C"
{
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_apps_logger;
    }
}
