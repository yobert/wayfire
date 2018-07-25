#include <view.hpp>
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
        if (!core->set_decorator(&decorator))
            return;

        wl_global_create(core->display,
                         &wf_decorator_manager_interface,
                         1, NULL, bind_decorator);

        view_created = [=] (signal_data *data)
        {
            new_view(get_signaled_view(data));
        };

        output->connect_signal("map-view", &view_created);
    }

    void new_view(wayfire_view view)
    {
    }

    void fini()
    {
        /* TODO: when the plugin actually gets usable, it's broken now anyway */
    }
};

extern "C"
{
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_decoration;
    }
}

