#include <output.hpp>
#include <core.hpp>
#include <config.hpp>
#include <linux/input-event-codes.h>

class wayfire_rotator : public wayfire_plugin_t {
    key_callback up, down, left, right;

    public:
    void init(wayfire_config *config) {
        grab_interface->name = "rotator";
        grab_interface->abilities_mask = WF_ABILITY_NONE;

        auto section = config->get_section("rotator");

        wayfire_key up_key    = section->get_key("rotate_up",
                {WLR_MODIFIER_ALT|WLR_MODIFIER_CTRL, KEY_UP});
        wayfire_key down_key  = section->get_key("rotate_down",
                {WLR_MODIFIER_ALT|WLR_MODIFIER_CTRL, KEY_DOWN});
        wayfire_key left_key  = section->get_key("rotate_left",
                {WLR_MODIFIER_ALT|WLR_MODIFIER_CTRL, KEY_LEFT});
        wayfire_key right_key = section->get_key("rotate_right",
                {WLR_MODIFIER_ALT|WLR_MODIFIER_CTRL, KEY_RIGHT});

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

        output->add_key(up_key.mod, up_key.keyval, &up);
        output->add_key(down_key.mod, down_key.keyval, &down);
        output->add_key(left_key.mod, left_key.keyval, &left);
        output->add_key(right_key.mod, right_key.keyval, &right);
    }
};

extern "C" {
    wayfire_plugin_t *newInstance() {
        return new wayfire_rotator();
    }
}
