#include <wayfire/singleton-plugin.hpp>
#include <wayfire/core.hpp>
#include <wayfire/util/log.hpp>
#include <wayfire/workspace-manager.hpp>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/nonstd/reverse.hpp>
#include <linux/input-event-codes.h>
#include <cstdlib>
#include <config.h>

#include <../../src/core/seat/cursor.hpp>
#include <../../src/core/core-impl.hpp>
#include <../../src/core/seat/pointer.hpp>
#include <../../src/core/seat/input-manager.hpp>

class wayfire_debug : public wf::plugin_interface_t
{
  public:
    void init() override
    {
        set_reorder();
        set_motion();
        set_key();
    }

    wf::wl_timer reorder;
    wf::wl_timer key;
    wf::wl_timer motion;

    wf::wl_idle_call idle_reorder, idle_motion, idle_key;

    void set_reorder()
    {
        reorder.set_timeout(50, [=] ()
        {
            auto views = output->workspace->get_views_on_workspace(
                output->workspace->get_current_workspace(),
                wf::LAYER_WORKSPACE);

            if (!views.empty())
            {
                // we don't need truly random just next
                int idx = std::rand() % views.size();
                output->workspace->bring_to_front(views[idx]);
            }

            idle_reorder.run_once([=] () { set_reorder(); });
        });
    }

    void set_motion()
    {
        motion.set_timeout(5, [=] ()
        {
            wlr_event_pointer_motion ev;
            ev.delta_x    = 0;
            ev.delta_x    = 0;
            ev.unaccel_dx = 0;
            ev.unaccel_dy = 0;
            ev.time_msec  = wf::get_current_time();
            wf::get_core_impl().input->lpointer->handle_pointer_motion(&ev);

            idle_motion.run_once([=] () { set_motion(); });
        });
    }

    void set_key()
    {
        key.set_timeout(10, [=] ()
        {
            wf::get_core_impl().input->handle_keyboard_key(KEY_ENTER,
                WLR_KEY_PRESSED);
            wf::get_core_impl().input->handle_keyboard_key(KEY_ENTER,
                WLR_KEY_RELEASED);

            idle_key.run_once([=] () { set_key(); });
        });
    }
};

DECLARE_WAYFIRE_PLUGIN(wayfire_debug);
