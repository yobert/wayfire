#include <output.hpp>
#include <core.hpp>
#include <linux/input.h>
#include <linux/input-event-codes.h>

#define TYPE_COMMAND 1
#define TYPE_BINDING 2

class wayfire_command : public wayfire_plugin_t
{
    std::vector<key_callback> cmds;

    public:
    void init(wayfire_config *config)
    {
        grab_interface->name = "command";
        using namespace std::placeholders;


        auto section = config->get_section("command");

        std::vector<std::string> commands;
        for (auto command : section->options)
        {
            if (command.first.length() > 8 && command.first.substr(0, 8) == "command_")
            {
                commands.push_back(command.first.substr(8, command.first.size() - 8));
            }
        }

        cmds.resize(commands.size());
        int i = 0;

        for (auto num : commands)
        {
            auto command = "command_" + num;
            auto binding = "binding_" + num;

            auto comvalue = section->get_option(command, "")->as_string();
            auto key = section->get_option(binding, "none");

            if (!key->as_key().valid() || command == "")
                continue;

            cmds[i++] = [=] (uint32_t key) { core->run(comvalue.c_str()); };
            output->add_key(key, &cmds[i - 1]);
        }
    }
};

extern "C"
{
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_command();
    }
}
