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
    wl_event_source *created_idle = NULL, *destroyed_idle = NULL;

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

    function idle_func_created = [=] () { update_log_file(); created_idle = NULL; };
    function idle_func_destroyed = [=] () { update_log_file(); destroyed_idle = NULL; };

    public:
    void init(wayfire_config *config)
    {
        created_cb = [=] (signal_data *data)
        {
            if (!created_idle)
                created_idle = wl_event_loop_add_idle(core->ev_loop, idle_callback, &idle_func_created);
        };
        output->connect_signal("map-view", &created_cb);

        destroyed_cb = [=] (signal_data *data)
        {
            if (!destroyed_idle)
                destroyed_idle = wl_event_loop_add_idle(core->ev_loop, idle_callback, &idle_func_destroyed);
        };
        output->connect_signal("unmap-view", &destroyed_cb);
    }

    void fini()
    {
        output->disconnect_signal("map-view", &created_cb);
        output->disconnect_signal("unmap-view", &destroyed_cb);

        if (created_idle)
            wl_event_source_remove(created_idle);
        if (destroyed_idle)
            wl_event_source_remove(destroyed_idle);
    }
};

extern "C"
{
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_apps_logger;
    }
}
