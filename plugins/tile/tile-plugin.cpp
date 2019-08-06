#include <plugin.hpp>
#include <output.hpp>
#include <workspace-manager.hpp>
#include <signal-definitions.hpp>

#include "tree-controller.hpp"

namespace wf
{
class tile_workspace_implementation_t : public wf::workspace_implementation_t
{
  public:
    bool view_movable(wayfire_view view) override
    {
        return wf::tile::view_node_t::get_node(view) == nullptr;
    }

    bool view_resizable(wayfire_view view) override
    {
        return wf::tile::view_node_t::get_node(view) == nullptr;
    }
};

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

    void update_root_size(wf_geometry workarea)
    {
        auto output_geometry = output->get_relative_geometry();
        auto wsize = output->workspace->get_workspace_grid_size();
        for (int i = 0; i < wsize.width; i++)
        {
            for (int j = 0; j < wsize.height; j++)
            {
                /* Set size */
                auto vp_geometry = workarea;
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

        if (view->parent)
            return false;

        return true;
    }

    static std::unique_ptr<wf::tile::tile_controller_t> get_default_controller()
    {
        return std::make_unique<wf::tile::tile_controller_t> ();
    }

    std::unique_ptr<wf::tile::tile_controller_t> controller =
        get_default_controller();

    /**
     * Translate coordinates from output-local coordinates to the coordinate
     * system of the tiling trees, depending on the current workspace
     */
    wf_point get_global_coordinates(wf_point local_coordinates)
    {
        auto vp = output->workspace->get_current_workspace();
        auto size = output->get_screen_size();
        local_coordinates.x += size.width * vp.x;
        local_coordinates.y += size.height * vp.y;

        return local_coordinates;
    }

    template<class Controller>
    void start_controller(wf_point grab)
    {
        auto vp = output->workspace->get_current_workspace();
        /* No action possible in this case */
        if (count_fullscreen_views(roots[vp.x][vp.y]))
            return;

        if (output->activate_plugin(grab_interface))
        {
            if (grab_interface->grab())
            {
                controller = std::make_unique<Controller> (
                    roots[vp.x][vp.y], get_global_coordinates(grab));
            } else
            {
                output->deactivate_plugin(grab_interface);
            }
        }
    }

    void stop_controller(bool force_stop)
    {
        if (!output->is_plugin_active(grab_interface->name))
            return;

        if (!force_stop)
            controller->input_released();

        output->deactivate_plugin(grab_interface);
        controller = get_default_controller();
    }

    void attach_view(wayfire_view view)
    {
        if (!can_tile_view(view))
            return;

        stop_controller(true);

        auto vp = output->workspace->get_current_workspace();
        auto view_node = std::make_unique<wf::tile::view_node_t> (view);
        roots[vp.x][vp.y]->as_split_node()->add_child(std::move(view_node));
    }

    signal_callback_t on_view_attached = [=] (signal_data_t *data)
    {
        auto view = get_signaled_view(data);
        attach_view(view);
    };

    signal_callback_t on_view_unmapped = [=] (signal_data_t *data)
    {
        stop_controller(true);
    };

    /** Remove the given view from its tiling container */
    void detach_view(nonstd::observer_ptr<tile::view_node_t> view)
    {
        stop_controller(true);

        view->parent->remove_child(view);
        /* View node is invalid now */
        flatten_roots();
    }

    signal_callback_t on_view_detached = [=] (signal_data_t *data)
    {
        auto view = get_signaled_view(data);
        auto view_node = wf::tile::view_node_t::get_node(view);

        if (view_node)
            detach_view(view_node);
    };

    signal_callback_t on_workarea_changed = [=] (signal_data_t *data)
    {
        update_root_size(output->workspace->get_workarea());
    };

    signal_callback_t on_tile_request = [=] (signal_data_t *data)
    {
        auto ev = static_cast<view_tiled_signal*> (data);
        if (ev->carried_out || !tile::view_node_t::get_node(ev->view))
            return;

        // we ignore those requests because we manage the tiled state manually
        ev->carried_out = true;
    };

    void set_view_fullscreen(wayfire_view view, bool fullscreen)
    {
        /* Set fullscreen, and trigger resizing of the views */
        view->set_fullscreen(fullscreen);
        update_root_size(output->workspace->get_workarea());
    }

    signal_callback_t on_fullscreen_request = [=] (signal_data_t *data)
    {
        auto ev = static_cast<view_fullscreen_signal*> (data);
        if (ev->carried_out || !tile::view_node_t::get_node(ev->view))
            return;

        ev->carried_out = true;
        set_view_fullscreen(ev->view, ev->state);
    };

    key_callback on_toggle_fullscreen = [=] (uint32_t key)
    {
        auto view = output->get_active_view();
        if (!view || !tile::view_node_t::get_node(view))
            return;

        if (output->activate_plugin(grab_interface))
        {
            stop_controller(true);
            set_view_fullscreen(view, !view->fullscreen);
            output->deactivate_plugin(grab_interface);
        }
    };

    key_callback on_toggle_tiled_state = [=] (uint32_t key)
    {
        auto view = output->get_active_view();
        if (!view || !output->activate_plugin(grab_interface))
            return;

        auto existing_node = tile::view_node_t::get_node(view);
        if (existing_node) {
            detach_view(existing_node);
            view->tile_request(0);
        } else {
            attach_view(view);
        }

        output->deactivate_plugin(grab_interface);
    };

    button_callback on_retile_view = [=] (uint32_t button, int32_t x, int32_t y)
    {
        start_controller<tile::move_view_controller_t> ({x, y});
    };

    button_callback on_resize_view = [=] (uint32_t button, int32_t x, int32_t y)
    {
        start_controller<tile::resize_view_controller_t> ({x, y});
    };

  public:
    void init(wayfire_config *config) override
    {
        this->grab_interface->name = "simple-tile";
        /* TODO: change how grab interfaces work - plugins should do ifaces on
         * their own, and should be able to have more than one */
        this->grab_interface->capabilities = CAPABILITY_MANAGE_COMPOSITOR;

        initialize_roots();
        // TODO: check whether this was successful
        output->workspace->set_workspace_implementation(
            std::make_unique<tile_workspace_implementation_t> (), true);

        output->connect_signal("unmap-view", &on_view_unmapped);
        output->connect_signal("attach-view", &on_view_attached);
        output->connect_signal("detach-view", &on_view_detached);
        output->connect_signal("reserved-workarea", &on_workarea_changed);
        output->connect_signal("view-maximized-request", &on_tile_request);
        output->connect_signal("view-fullscreen-request",
            &on_fullscreen_request);

        auto button = new_static_option("<super> BTN_LEFT");
        output->add_button(button, &on_retile_view);

        auto rb = new_static_option("<super> BTN_RIGHT");
        output->add_button(rb, &on_resize_view);

        auto toggle = new_static_option("<super> KEY_M");
        output->add_key(toggle, &on_toggle_fullscreen);

        auto toggle_tile = new_static_option("<super> KEY_N");
        output->add_key(toggle_tile, &on_toggle_tiled_state);

        grab_interface->callbacks.pointer.button =
            [=] (uint32_t b, uint32_t state)
        {
            if (state == WLR_BUTTON_RELEASED)
                stop_controller(false);
        };

        grab_interface->callbacks.pointer.motion = [=] (int32_t x, int32_t y)
        {
            controller->input_motion(get_global_coordinates({x, y}));
        };
    }

    void fini() override
    {
        output->workspace->set_workspace_implementation(nullptr);

        output->disconnect_signal("unmap-view", &on_view_unmapped);
        output->disconnect_signal("attach-view", &on_view_attached);
        output->disconnect_signal("detach-view", &on_view_detached);
        output->disconnect_signal("reserved-workarea", &on_workarea_changed);
        output->disconnect_signal("view-maximized-request", &on_tile_request);
        output->disconnect_signal("view-fullscreen-request",
            &on_fullscreen_request);
    }
};
};

DECLARE_WAYFIRE_PLUGIN(wf::tile_plugin_t);
