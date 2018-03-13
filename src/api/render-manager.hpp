#ifndef RENDER_MANAGER_HPP
#define RENDER_MANAGER_HPP

#include "plugin.hpp"
#include <vector>


namespace OpenGL { struct context_t; }

class wayfire_view_t;
using wayfire_view = std::shared_ptr<wayfire_view_t>;

struct weston_gl_renderer_api;

/* Workspace streams are used if you need to continuously render a workspace
 * to a texture, for example if you call texture_from_viewport at every frame */
struct wf_workspace_stream
{
    std::tuple<int, int> ws;
    uint fbuff, tex;
    bool running = false;

    float scale_x, scale_y;
};

class render_manager
{
    private:
        friend bool custom_renderer_cb(weston_output *o, pixman_region32_t *damage);
        friend void post_render_cb(weston_output *o);
        friend void redraw_idle_cb(void *data);
        friend void idle_full_redraw_cb(void *data);

        wayfire_output *output;

        bool dirty_context = true;
        void load_context();
        void release_context();

        bool draw_overlay_panel = true;
        pixman_region32_t frame_damage, single_pixel;

        int streams_running = 0;

        signal_callback_t view_moved_cb, viewport_changed_cb;
        bool fdamage_track_enabled = false;

        void update_full_damage_tracking_view(wayfire_view view);
        void update_full_damage_tracking();
        void disable_full_damage_tracking();
        void get_ws_damage(std::tuple<int, int> ws, pixman_region32_t *out_damage);

        std::vector<effect_hook_t*> output_effects;
        int constant_redraw = 0;
        bool frame_was_custom_rendered = false, dirty_renderer = false;
        wl_event_source *idle_redraw_source = NULL, *full_repaint_source = NULL;
        render_hook_t renderer;

        bool paint(pixman_region32_t *damage);
        void post_paint();

        void transformation_renderer();
        void run_effects();
        void render_panels();

    public:
        OpenGL::context_t *ctx;
        static const weston_gl_renderer_api *renderer_api;

        render_manager(wayfire_output *o);
        ~render_manager();

        void set_renderer(render_hook_t rh = nullptr);
        void reset_renderer();

        /* schedule repaint immediately after finishing the last one
         * to undo, call auto_redraw(false) as much times as auto_redraw(true) was called */
        void auto_redraw(bool redraw);
        void schedule_redraw();
        void set_hide_overlay_panels(bool set);

        void add_output_effect(effect_hook_t*, wayfire_view v = nullptr);
        void rem_effect(const effect_hook_t*, wayfire_view v = nullptr);

        /* this function renders a viewport and
         * saves the image in texture which is returned */
        void texture_from_workspace(std::tuple<int, int>, uint& fbuff, uint &tex);

        void workspace_stream_start(wf_workspace_stream *stream);
        void workspace_stream_update(wf_workspace_stream *stream,
                float scale_x = 1, float scale_y = 1);
        void workspace_stream_stop(wf_workspace_stream *stream);
};

#endif
