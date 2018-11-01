#ifndef COMPOSITOR_VIEW_HPP
#define COMPOSITOR_VIEW_HPP

#include "compositor-surface.hpp"

/* Subclass this if you want the view to be able to interact with keyboard
 * and to receive focus */
class wayfire_compositor_interactive_view
{
    public:
        void handle_keyboard_enter() {}
        void handle_keyboard_leave() {}
        void handle_key(uint32_t key, uint32_t state) {}
};

static wayfire_compositor_interactive_view *interactive_view_from_view(wayfire_view_t *view)
{
    return dynamic_cast<wayfire_compositor_interactive_view*> (view);
}

/* a base class for writing compositor views
 *
 * It can be used by plugins to create views with compositor-generated content */
class wayfire_compositor_view_t : virtual public wayfire_compositor_surface_t, virtual public wayfire_view_t
{
    protected:
        /* Implement _wlr_render_box to get something on screen */
        virtual void _wlr_render_box(const wlr_fb_attribs& fb, int x, int y, const wlr_box& scissor) { assert(false); }

    public:

        virtual bool is_mapped() { return map_state; }
        virtual void send_frame_done(const timespec& now) {}

        /* override this if you want to get pointer events or to stop input passthrough */
        virtual bool accepts_input(int32_t sx, int32_t sy) { return false; }


    public:
        wayfire_compositor_view_t();
        virtual ~wayfire_compositor_view_t() {}

        /* By default, use move/resize/set_geometry to set the size */
        virtual wf_point get_output_position();
        virtual wf_geometry get_output_geometry();
        virtual wf_geometry get_wm_geometry();
        virtual void set_geometry(wf_geometry g) {wayfire_view_t::geometry = g;}

        virtual void activate(bool active) {}
        virtual void close();

        virtual wlr_surface *get_keyboard_focus_surface() { return nullptr; };

        virtual std::string get_app_id() { return "wayfire-compositor-view"; }
        virtual std::string get_title() { return "wayfire-compositor-view-" + this->wf_object_base::to_string(); }

        virtual bool should_be_decorated() { return false; }

        /* Usually compositor view implementations don't need to override this */
        virtual void render_fb(pixman_region32_t* damage, wf_framebuffer fb);

        /* NON-API functions which don't have a meaning for compositor views */
        virtual bool update_size() { assert(false); }

        virtual void get_child_position(int &x, int &y) { x = y = 0; }
        virtual bool is_subsurface() { return false; }

        virtual void get_child_offset(int &x, int &y) { x = y = 0;}

        bool map_state = false;
        virtual void map();
        virtual void map(wlr_surface *surface) {map();}
        virtual void unmap();

        virtual wlr_buffer *get_buffer() { return NULL; }
        virtual bool can_take_snapshot() { return is_mapped(); }
        virtual void commit() {assert(false); }
};

#endif /* end of include guard: COMPOSITOR_VIEW_HPP */
