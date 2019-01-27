#include <output.hpp>
#include <core.hpp>
#include <linux/input-event-codes.h>

class wayfire_rotator : public wayfire_plugin_t
{
    activator_callback up, down, left, right;

    public:
    void init(wayfire_config *config) {
        grab_interface->name = "rotator";
        grab_interface->abilities_mask = WF_ABILITY_NONE;

        auto section = config->get_section("rotator");

        auto up_key    = section->get_option("rotate_up",   "<alt> <ctrl> <shift> KEY_UP");
        auto down_key  = section->get_option("rotate_down", "<alt> <ctrl> <shift> KEY_DOWN");
        auto left_key  = section->get_option("rotate_left", "<alt> <ctrl> <shift> KEY_LEFT");
        auto right_key = section->get_option("rotate_right","<alt> <ctrl> <shift> KEY_RIGHT");

        up    = [=] (wf_activator_source, uint32_t) { output->set_transform(WL_OUTPUT_TRANSFORM_NORMAL); };
        down  = [=] (wf_activator_source, uint32_t) { output->set_transform(WL_OUTPUT_TRANSFORM_180); };
        left  = [=] (wf_activator_source, uint32_t) { output->set_transform(WL_OUTPUT_TRANSFORM_270); };
        right = [=] (wf_activator_source, uint32_t) { output->set_transform(WL_OUTPUT_TRANSFORM_90); };

        output->add_activator(up_key,    &up);
        output->add_activator(down_key,  &down);
        output->add_activator(left_key,  &left);
        output->add_activator(right_key, &right);
    }

    void fini()
    {
        output->rem_binding(&up);
        output->rem_binding(&down);
        output->rem_binding(&left);
        output->rem_binding(&right);
    }
};

extern "C"
{
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_rotator();
    }
}
