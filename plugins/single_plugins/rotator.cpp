#include <output.hpp>
#include <core.hpp>
#include <linux/input-event-codes.h>

class wayfire_rotator : public wayfire_plugin_t {
    key_callback up, down, left, right;

    public:
    void init(wayfire_config *config) {
        grab_interface->name = "rotator";
        grab_interface->abilities_mask = WF_ABILITY_NONE;

        auto section = config->get_section("rotator");

        auto up_key    = section->get_option("rotate_up",   "<alt> <ctrl> KEY_UP");
        auto down_key  = section->get_option("rotate_down", "<alt> <ctrl> KEY_DOWN");
        auto left_key  = section->get_option("rotate_left", "<alt> <ctrl> KEY_LEFT");
        auto right_key = section->get_option("rotate_right","<alt> <ctrl> KEY_RIGHT");

        up = [=] (uint32_t) {
            output->set_transform(WL_OUTPUT_TRANSFORM_NORMAL);
        };
        down = [=] (uint32_t) {
            output->set_transform(WL_OUTPUT_TRANSFORM_180);
        };
        left = [=] (uint32_t) {
            output->set_transform(WL_OUTPUT_TRANSFORM_270);
        };
        right = [=] (uint32_t) {
            output->set_transform(WL_OUTPUT_TRANSFORM_90);
        };

        output->add_key(up_key,    &up);
        output->add_key(down_key,  &down);
        output->add_key(left_key,  &left);
        output->add_key(right_key, &right);
    }

    void fini()
    {
        output->rem_key(&up);
        output->rem_key(&down);
        output->rem_key(&left);
        output->rem_key(&right);
    }
};

extern "C"
{
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_rotator();
    }
}
