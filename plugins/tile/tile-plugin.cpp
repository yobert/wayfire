#include <plugin.hpp>
#include <output.hpp>
#include <workspace-manager.hpp>
#include <signal-definitions.hpp>

#include "tree-controller.hpp"

namespace wf
{
class tile_plugin_t : public wf::plugin_interface_t
{
  private:
    std::vector<std::vector<std::unique_ptr<wf::tile::tree_node_t>>> roots;

    const wf::tile::split_direction_t default_split = wf::tile::SPLIT_VERTICAL;

    void initialize_roots()
    {
        auto wsize = output->workspace->get_workspace_grid_size();
        roots.resize(wsize.width);
        for (int i = 0; i < wsize.width; i++)
        {
            roots[i].resize(wsize.height);
            for (int j = 0; j < wsize.height; j++)
            {
                roots[i][j] =
                    std::make_unique<wf::tile::split_node_t>(default_split);
            }
        }

        update_root_size(output->workspace->get_workarea());
    }

    void update_root_size(wf_geometry output_geometry)
    {
        auto wsize = output->workspace->get_workspace_grid_size();
        for (int i = 0; i < wsize.width; i++)
        {
            for (int j = 0; j < wsize.height; j++)
            {
                /* Set size */
                auto vp_geometry = output_geometry;
                vp_geometry.x += i * output_geometry.width;
                vp_geometry.y += j * output_geometry.height;
                roots[i][j]->set_geometry(vp_geometry);
            }
        }
    }

    void flatten_roots()
    {
        for (auto& col : roots)
        {
            for (auto& root : col)
                tile::flatten_tree(root);
        }
    }

    bool can_tile_view(wayfire_view view)
    {
        if (view->role != wf::VIEW_ROLE_TOPLEVEL)
            return false;

        return true;
    }

    signal_callback_t on_view_mapped = [=] (signal_data_t *data)
    {
        auto view = get_signaled_view(data);
        if (!can_tile_view(view))
            return;

        auto vp = output->workspace->get_current_workspace();
        auto view_node = std::make_unique<wf::tile::view_node_t> (view);

        roots[vp.x][vp.y]->as_split_node()->add_child(std::move(view_node));
    };

    signal_callback_t on_view_unmapped = [=] (signal_data_t *data)
    {
        auto view = get_signaled_view(data);
        auto view_node = wf::tile::view_node_t::get_node(view);

        if (!view_node)
            return;

        view_node->parent->remove_child(view_node);
        /* View node is invalid now */
        flatten_roots();
    };

    signal_callback_t on_workarea_changed = [=] (signal_data_t *data)
    {
        update_root_size(output->workspace->get_workarea());
    };

    static std::unique_ptr<wf::tile::tile_controller_t> get_default_controller()
    {
        return std::make_unique<wf::tile::tile_controller_t> ();
    }

    std::unique_ptr<wf::tile::tile_controller_t> controller =
        get_default_controller();

    button_callback on_retile_view = [=] (uint32_t button, int32_t x, int32_t y)
    {
        auto vp = output->workspace->get_current_workspace();
        controller = std::make_unique<tile::move_view_controller_t> (
            roots[vp.x][vp.y]);

        output->activate_plugin(grab_interface);
        grab_interface->grab();
    };

  public:
    void init(wayfire_config *config) override
    {
        this->grab_interface->name = "simple-tile";
        /* TODO: change how grab interfaces work - plugins should do ifaces on
         * their own, and should be able to have more than one */
        this->grab_interface->capabilities = CAPABILITY_GRAB_INPUT;

        initialize_roots();
        output->connect_signal("attach-view", &on_view_mapped);
        output->connect_signal("detach-view", &on_view_unmapped);
        output->connect_signal("reserved-workarea", &on_workarea_changed);

        auto button = new_static_option("<super> BTN_LEFT");
        output->add_button(button, &on_retile_view);

        grab_interface->callbacks.pointer.button =
            [=] (uint32_t b, uint32_t state)
        {
            if (state == WLR_BUTTON_RELEASED &&
                b == button->as_cached_button().button)
            {
                output->deactivate_plugin(grab_interface);
                controller = get_default_controller();
            }
        };

        grab_interface->callbacks.pointer.motion = [=] (int32_t x, int32_t y)
        {
            controller->input_motion({x, y});
        };
    }

    void fini() override
    {
        output->disconnect_signal("attach-view", &on_view_mapped);
        output->disconnect_signal("detach-view", &on_view_unmapped);
        output->disconnect_signal("reserved-workarea", &on_workarea_changed);
    }
};
};

DECLARE_WAYFIRE_PLUGIN(wf::tile_plugin_t);
