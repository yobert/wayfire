#include <vector>
#include <unordered_map>
#include "plugin.hpp"
#include "config.h"

class wayfire_output;
class wayfire_config;

using wayfire_plugin = std::unique_ptr<wayfire_plugin_t>;
struct plugin_manager
{
    plugin_manager(wayfire_output *o, wayfire_config *config);
    ~plugin_manager();

    void reload_dynamic_plugins();
    wl_event_source *idle_reload_dynamic_plugins = NULL;

private:
    wayfire_config *config;
    wayfire_output *output;
    wf_option plugins_opt;

    std::unordered_map<std::string, wayfire_plugin> loaded_plugins;
    wf_option_callback list_updated;

    void deinit_plugins(bool unloadable, bool internal);

    wayfire_plugin load_plugin_from_file(std::string path);
    void load_static_plugins();

    void init_plugin(wayfire_plugin& plugin);
    void destroy_plugin(wayfire_plugin& plugin);
};
