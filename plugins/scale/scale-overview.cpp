#include <wayfire/plugin.hpp>
#include <wayfire/plugins/scale-signal.hpp>
#include <wayfire/plugins/common/workspace-wall.hpp>
#include <wayfire/compositor-view.hpp>

#include <wayfire/util/log.hpp>

class overview_mirror_view_t : public wf::mirror_view_t
{
  public:
    overview_mirror_view_t(wayfire_view view) : wf::mirror_view_t(view)
    {
        LOGI("creating a mirror view");
        set_output(view->get_output());
        get_output()->workspace->add_view(self(), wf::LAYER_WORKSPACE);
        // get_output()->workspace->restack_above(self(), view);
        this->move(view->get_wm_geometry().x, view->get_wm_geometry().y);
        emit_map_state_change(this);
    }

    void set_activated(bool v) override
    {
        if (base_view)
        {
            base_view->set_activated(true);
        }
    }
};

class overview_t : public wf::plugin_interface_t
{
    std::unique_ptr<wf::workspace_wall_t> wall;

  public:
    void init() override
    {
        grab_interface->name = "overview";
        grab_interface->capabilities = 0;

        output->add_activator(
            wf::create_option_string<wf::activatorbinding_t>("<super>KEY_W"),
            &activate);

        output->connect_signal("scale-end", &on_deactivate);
        output->connect_signal("scale-filter", &on_scale_filter);
        wall = std::make_unique<wf::workspace_wall_t>(output);
    }

    wf::activator_callback activate = [=] (auto)
    {
        if (output->is_plugin_active("scale"))
        {
            wf::activator_data_t data;
            data.source = wf::activator_source_t::PLUGIN;
            output->call_plugin("scale/toggle", data);

            // Practically deactivate wall
            wall->set_viewport({0, 0, 0, 0});
            output->render->rem_effect(&workspace_overlay_hook);
            output->render->rem_effect(&workspace_overlay_damage);
            workspace_overlay_damage();

            return true;
        }

        // create mirror views but filter them out

        for (auto& view : output->workspace->get_views_in_layer(wf::WM_LAYERS))
        {
            auto uptr = std::make_unique<overview_mirror_view_t>(view);
            wf::get_core().add_view(std::move(uptr));
        }

        // TODO: handle activation and conflicts with other plugins
        wf::activator_data_t data;
        data.source = wf::activator_source_t::PLUGIN;
        output->call_plugin("scale/toggle", data);

        wall->set_viewport(wall->get_wall_rectangle());
        output->render->add_effect(&workspace_overlay_hook,
            wf::OUTPUT_EFFECT_OVERLAY);
        output->render->add_effect(&workspace_overlay_damage, wf::OUTPUT_EFFECT_PRE);
        return true;
    };

    wf::signal_connection_t on_deactivate = [=] (auto)
    {
        LOGI("Deactivating");
        std::vector<wayfire_view> tagged;
        for (auto& view : output->workspace->get_views_in_layer(wf::WM_LAYERS))
        {
            if (dynamic_cast<overview_mirror_view_t*>(view.get()))
            {
                tagged.push_back(view);
            }

            if (view->has_data("scale-hidden"))
            {
                view->set_visible(true);
            }
        }

        for (auto view : tagged)
        {
            LOGI("closing a mirror view");
            view->close();
        }
    };

    static inline bool is_normal_view(wayfire_view view)
    {
        return dynamic_cast<overview_mirror_view_t*>(view.get()) == nullptr;
    }

    wf::signal_connection_t on_scale_filter = [=] (auto data)
    {
        LOGI("scale filter!");
        return;

        auto ev = static_cast<scale_filter_signal*>(data);
        auto remove_and_hide = [=] (auto& container)
        {
            for (auto& view : container)
            {
                if (is_normal_view(view))
                {
                    view->store_data(
                        std::make_unique<wf::custom_data_t>(), "scale-hidden");
                    view->set_visible(false);
                }
            }

            auto it = std::remove_if(container.begin(),
                container.end(), is_normal_view);
            container.erase(it, container.end());
        };

        remove_and_hide(ev->views_shown);
        remove_and_hide(ev->views_hidden);
    };

    void toggle_visibility(bool mirrored_visible)
    {
        for (auto& view : output->workspace->get_views_in_layer(wf::WM_LAYERS))
        {
            if (dynamic_cast<overview_mirror_view_t*>(view.get()))
            {
                // LOGI("hiding mirror ", mirrored_visible);
                view->set_visible(mirrored_visible);
            } else if (view->has_data("scale-hidden"))
            {
// LOGI("showing other");
                view->set_visible(!mirrored_visible);
            }
        }
    }

    wf::effect_hook_t workspace_overlay_hook = [=] ()
    {
        // Trick: scale will hide the mirrored views, without ever touching them.
        // Now, to get the proper workspace textures without the scale transform,
        // we can just unhide them and hide the real ones while updating the
        // workspace streams.
        toggle_visibility(false);
        wall->render_wall(output->render->get_target_framebuffer(),
            {0, 0, 300, 600});
        toggle_visibility(true);
    };

    wf::effect_hook_t workspace_overlay_damage = [=] ()
    {
        output->render->damage({0, 0, 300, 600});
    };


    void fini() override
    {
        // TODO
    }
};

DECLARE_WAYFIRE_PLUGIN(overview_t);
