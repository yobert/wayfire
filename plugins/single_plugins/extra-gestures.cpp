#include <wayfire/plugin.hpp>
#include <wayfire/core.hpp>
#include <wayfire/touch/touch.hpp>
#include <wayfire/view.hpp>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/output-layout.hpp>

#include <wayfire/util/log.hpp>

namespace wf
{
using namespace touch;
class extra_gestures_plugin_t : public plugin_interface_t
{
    std::unique_ptr<gesture_t> touch_and_hold_move;

    wf::option_wrapper_t<int> move_fingers{"extra-gestures/move_fingers"};
    wf::option_wrapper_t<int> move_delay{"extra-gestures/move_delay"};

  public:
    void init() override
    {
        build_touch_and_hold_move();
        move_fingers.set_callback([=] () { build_touch_and_hold_move(); });
        move_delay.set_callback([=] () { build_touch_and_hold_move(); });
        wf::get_core().add_touch_gesture({touch_and_hold_move});
    }

    void build_touch_and_hold_move()
    {
        if (touch_and_hold_move)
        {
            wf::get_core().rem_touch_gesture({touch_and_hold_move});
        }

        auto touch_down = std::make_unique<wf::touch_action_t>(move_fingers, true);
        touch_down->set_move_tolerance(50);
        touch_down->set_duration(100);

        auto hold = std::make_unique<wf::hold_action_t>(move_delay);
        hold->set_move_tolerance(100);

        std::vector<std::unique_ptr<gesture_action_t>> actions;
        actions.emplace_back(std::move(touch_down));
        actions.emplace_back(std::move(hold));
        touch_and_hold_move = std::make_unique<gesture_t>(std::move(actions),
            [=] ()
        {
            auto& core = wf::get_core();
            auto state = core.get_touch_state();
            auto center_touch_point = state.get_center().current;
            wf::pointf_t center     = {center_touch_point.x, center_touch_point.y};

            if (core.output_layout->get_output_at(center.x, center.y) !=
                this->output)
            {
                return;
            }

            auto view = core.get_view_at({center.x, center.y});
            if (view && (view->role == VIEW_ROLE_TOPLEVEL))
            {
                view->move_request();
            }
        });
    }

    void fini() override
    {
        wf::get_core().rem_touch_gesture({touch_and_hold_move});
    }
};
}

DECLARE_WAYFIRE_PLUGIN(wf::extra_gestures_plugin_t);
