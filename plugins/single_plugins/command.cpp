#include <output.hpp>
#include <core.hpp>
#include <linux/input.h>
#include <linux/input-event-codes.h>

#define TYPE_COMMAND 1
#define TYPE_BINDING 2

class wayfire_command : public wayfire_plugin_t
{
    std::vector<activator_callback> cmds;

    public:

    void setup_bindings_from_config(wayfire_config *config)
    {
        auto section = config->get_section("command");

        std::vector<std::string> commands;
        for (auto command : section->options)
        {
            if (command->name.length() > 8 && command->name.substr(0, 8) == "command_")
                commands.push_back(command->name.substr(8, command->name.size() - 8));
        }

        cmds.resize(commands.size());
        int i = 0;

        for (auto num : commands)
        {
            auto command = "command_" + num;
            auto binding = "binding_" + num;

            auto comvalue = section->get_option(command, "")->as_string();
            auto activator = section->get_option(binding, "none");

            cmds[i++] = [=] (wf_activator_source, uint32_t) { core->run(comvalue.c_str()); };
            output->add_activator(activator, &cmds[i - 1]);
        }
    }

    void clear_bindings()
    {
        for (size_t i = 0; i < cmds.size(); i++)
            output->rem_binding(&cmds[i]);

        cmds.clear();
    }

    signal_callback_t reload_config;

    void init(wayfire_config *config)
    {
        grab_interface->name = "command";
        using namespace std::placeholders;

        setup_bindings_from_config(config);

        reload_config = [=] (signal_data*)
        {
            clear_bindings();
            setup_bindings_from_config(core->config);
        };

        core->connect_signal("reload-config", &reload_config);
    }

    void fini()
    {
        core->disconnect_signal("reload-config", &reload_config);
        clear_bindings();
    }
};

extern "C"
{
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_command();
    }
}
