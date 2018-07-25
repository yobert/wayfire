#include <sstream>
#include <set>
#include <memory>
#include <dlfcn.h>

#include "plugin-loader.hpp"
#include "output.hpp"
#include "../core/wm.hpp"
#include "core.hpp"
#include "debug.hpp"

namespace
{
    template<class A, class B> B union_cast(A object)
    {
        union {
            A x;
            B y;
        } helper;
        helper.x = object;
        return helper.y;
    }
}

static const std::string default_plugins = "viewport_impl move resize animate \
                                            switcher vswitch cube expo command \
                                            grid";

static void idle_reload(void *data)
{
    auto manager = (plugin_manager *) data;
    manager->reload_dynamic_plugins();
    manager->idle_reload_dynamic_plugins = NULL;
}

plugin_manager::plugin_manager(wayfire_output *o, wayfire_config *config)
{
    this->config = config;
    this->output = o;

    auto section = config->get_section("core");
    plugins_opt = section->get_option("plugins", "default");

    reload_dynamic_plugins();
    load_static_plugins();

    list_updated = [=] ()
    {
        /* reload when config reload has finished */
        idle_reload_dynamic_plugins =
            wl_event_loop_add_idle(core->ev_loop, idle_reload, this);
    };

    plugins_opt->updated.push_back(&list_updated);
}

plugin_manager::~plugin_manager()
{
    for (auto& p : loaded_plugins)
        destroy_plugin(p.second);

    loaded_plugins.clear();
    wl_event_source_remove(idle_reload_dynamic_plugins);

    plugins_opt->updated.erase(std::remove(plugins_opt->updated.begin(), plugins_opt->updated.end(),
                                           &list_updated), plugins_opt->updated.end());
}

void plugin_manager::init_plugin(wayfire_plugin& p)
{
    p->grab_interface = new wayfire_grab_interface_t(output);
    p->output = output;

    p->init(config);
}

void plugin_manager::destroy_plugin(wayfire_plugin& p)
{
    p->grab_interface->ungrab();
    output->deactivate_plugin(p->grab_interface);

    p->fini();
    delete p->grab_interface;

    /* we load the same plugins for each output, so we must dlclose() the handle
     * only when we remove the last output */
    if (core->get_num_outputs() < 1)
    {
        if (p->dynamic)
            dlclose(p->handle);
    }

    p.reset();
}

wayfire_plugin plugin_manager::load_plugin_from_file(std::string path)
{
    void *handle = dlopen(path.c_str(), RTLD_NOW);
    if(handle == NULL)
    {
        log_error("error loading plugin: %s", dlerror());
        return nullptr;
    }


    auto initptr = dlsym(handle, "newInstance");
    if(initptr == NULL)
    {
        log_error("%s: missing newInstance(). %s", path.c_str(), dlerror());
        return nullptr;
    }

    log_debug("loading plugin %s", path.c_str());
    get_plugin_instance_t init = union_cast<void*, get_plugin_instance_t> (initptr);

    auto ptr = wayfire_plugin(init());

    ptr->handle = handle;
    ptr->dynamic = true;

    return wayfire_plugin(init());
}

void plugin_manager::reload_dynamic_plugins()
{
    std::stringstream stream(plugins_opt->as_string());
    std::vector<std::string> next_plugins;

    std::string plugin;
    while(stream >> plugin)
    {
        if(plugin != "")
            next_plugins.push_back(plugin);
    }

    /* erase plugins that have been removed from the config */
    auto it = loaded_plugins.begin();
    while(it != loaded_plugins.end())
    {
        /* skip built-in(static) plugins */
        if (it->first.size() && it->first[0] == '_')
        {
            ++it;
            continue;
        }

        if (std::find(next_plugins.begin(), next_plugins.end(), it->first) == next_plugins.end())
        {
            log_debug("unload plugin %s", it->first.c_str());
            destroy_plugin(it->second);
            it = loaded_plugins.erase(it);
        } else
        {
            ++it;
        }
    }


    /* load new plugins */
    auto path = std::string(INSTALL_PREFIX "/lib/wayfire/");
    for (auto plugin : next_plugins)
    {
        if (!plugin.length())
            continue;


        std::string plugin_path;
        if (plugin.size() && plugin[0] == '/')
        {
            plugin_path = plugin;
        } else
        {
            plugin_path = path + "lib" + plugin + ".so";
        }

        if (loaded_plugins.count(plugin_path))
            continue;

        auto ptr = load_plugin_from_file(plugin_path);
        if (ptr)
        {
            init_plugin(ptr);
            loaded_plugins[plugin] = std::move(ptr);
        }
    }
}

template<class T> static wayfire_plugin create_plugin()
{
    return std::unique_ptr<wayfire_plugin_t>(new T);
}

void plugin_manager::load_static_plugins()
{
    loaded_plugins["_exit"]         = create_plugin<wayfire_exit>();
    loaded_plugins["_focus"]        = create_plugin<wayfire_focus>();
    loaded_plugins["_close"]        = create_plugin<wayfire_close>();
    loaded_plugins["_focus_parent"] = create_plugin<wayfire_handle_focus_parent>();

    init_plugin(loaded_plugins["_exit"]);
    init_plugin(loaded_plugins["_focus"]);
    init_plugin(loaded_plugins["_close"]);
    init_plugin(loaded_plugins["_focus_parent"]);
}
