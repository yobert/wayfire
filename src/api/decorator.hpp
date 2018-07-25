#ifndef DECORATOR_HPP
#define DECORATOR_HPP

#include <view.hpp>

class wf_decorator_frame_t
{
    public:
        virtual wf_geometry expand_wm_geometry(wf_geometry contained_wm_geometry) = 0;
        virtual void calculate_resize_size(int& target_width, int& target_height) = 0;

        virtual void notify_view_activated(bool active) {}
        virtual void notify_view_resized(wf_geometry view_geometry) {}
        virtual void notify_view_maximized() {}
        virtual void notify_view_fullscreened() {}

        virtual ~wf_decorator_frame_t() {}
};

class decorator_base_t
{
    public:
        virtual bool is_decoration_window(std::string title) = 0;
        /* a decoration window has been mapped, it is ready to be set as such */
        virtual void decoration_ready(wayfire_view decor_window) = 0;
};

#endif /* end of include guard: DECORATOR_HPP */
