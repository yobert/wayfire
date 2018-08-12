#include <map>

#include "input-inhibit.hpp"
#include "output.hpp"
#include "plugin.hpp"

static const std::string iface_name = "_input_inhibitor";
std::map<wayfire_output*, wayfire_grab_interface> iface_map;

void inhibit_output(wayfire_output *output)
{
    wayfire_grab_interface iface = new wayfire_grab_interface_t(output);
    iface->name = iface_name;
    iface->abilities_mask = WF_ABILITY_ALL;

    output->break_active_plugins();
    output->activate_plugin(iface);

    iface_map[output] = iface;
}

void uninhibit_output(wayfire_output *output)
{
    if (!output->is_plugin_active(iface_name))
        return;

    auto iface = iface_map[output];
    assert(iface);

    iface_map.erase(output);
    output->deactivate_plugin(iface);
}
