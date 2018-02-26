#ifndef FIRE_H
#define FIRE_H

#include <functional>
#include <memory>
#include <vector>
#include <map>

#include <compositor.h>

struct weston_desktop_surface;

class input_manager;
class wayfire_config;
class wayfire_output;
class wayfire_view_t;

using wayfire_view = std::shared_ptr<wayfire_view_t>;
using output_callback_proc = std::function<void(wayfire_output *)>;

class wayfire_core
{
        friend struct plugin_manager;
        friend class wayfire_output;

        wayfire_config *config;

        std::vector<weston_output*> pending_outputs;

        wayfire_output *active_output;
        std::map<uint32_t, wayfire_output *> outputs;
        std::map<weston_view *, wayfire_view> views;

        void configure(wayfire_config *config);

        int times_wake = 0;

    public:

        std::vector<wl_resource*> shell_clients;


        std::string wayland_display, xwayland_display;

        input_manager *input;

        weston_compositor *ec;
        void init(weston_compositor *ec, wayfire_config *config);
        void wake();
        void sleep();
        void refocus_active_output_active_view();

        void setup_renderer();

        weston_seat *get_current_seat();

        void add_view(weston_desktop_surface *);
        wayfire_view find_view(weston_view *);
        wayfire_view find_view(weston_desktop_surface *);
        wayfire_view find_view(weston_surface *);

        /* completely destroy a view */
        void erase_view(wayfire_view view, bool destroy_libweston_view = false);

        /* brings the view to the top
         * and also focuses its output */
        void focus_view(wayfire_view win, weston_seat *seat);
        void close_view(wayfire_view win);
        void move_view_to_output(wayfire_view v, wayfire_output *new_output);

        void add_output(weston_output *output);
        wayfire_output *get_output(weston_output *output);

        void focus_output(wayfire_output *o);
        void remove_output(wayfire_output *o);

        wayfire_output *get_active_output();
        wayfire_output *get_next_output(wayfire_output *output);
        wayfire_output *get_output_at(int x, int y);
        size_t          get_num_outputs();

        void for_each_output(output_callback_proc);

        void run(const char *command);

        int vwidth, vheight;

        std::string shadersrc, plugin_path, plugins;
        bool run_panel;

        weston_compositor_backend backend;
};

extern wayfire_core *core;
#endif
