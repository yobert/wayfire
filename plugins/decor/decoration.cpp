#include <view.hpp>
#include <workspace-manager.hpp>
#include <output.hpp>
#include <debug.hpp>
#include <signal-definitions.hpp>

#include "deco-subsurface.hpp"
class wayfire_decoration : public wayfire_plugin_t
{
    signal_callback_t view_created;

    public:
    void init(wayfire_config *config)
    {
        view_created = [=] (signal_data *data)
        {
            new_view(get_signaled_view(data));
        };

        output->connect_signal("map-view", &view_created);
    }

    void new_view(wayfire_view view)
    {
        if (view->should_be_decorated())
            init_view(view);
    }

    void fini()
    {
        output->workspace->for_each_view([] (wayfire_view view)
        {
            view->set_decoration(nullptr);
        }, WF_ALL_LAYERS);
        output->disconnect_signal("map-view", &view_created);
    }
};

extern "C"
{
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_decoration;
    }
}

