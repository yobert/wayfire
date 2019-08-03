#include <plugin.hpp>
#include <output.hpp>
#include <workspace-manager.hpp>
#include <signal-definitions.hpp>

#include "tree.hpp"

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

        update_root_size(output->get_relative_geometry());
    }

    void update_root_size(wf_geometry output_geometry)
    {
        auto wsize = output->workspace->get_workspace_grid_size();
        for (int i = 0; i < wsize.width; i++)
        {
            for (int j = 0; j < wsize.height; j++)
            {
                /* Set size */
                output_geometry.x = i * output_geometry.width;
                output_geometry.y = j * output_geometry.height;
                roots[i][j]->set_geometry(output_geometry);
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

  public:
    void init(wayfire_config *config) override
    {
        initialize_roots();
        output->connect_signal("attach-view", &on_view_mapped);
        output->connect_signal("detach-view", &on_view_unmapped);
    }

    void fini() override
    {
        output->disconnect_signal("attach-view", &on_view_mapped);
        output->disconnect_signal("detach-view", &on_view_unmapped);
    }
};
};

DECLARE_WAYFIRE_PLUGIN(wf::tile_plugin_t);
