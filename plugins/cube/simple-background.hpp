#ifndef WF_CUBE_SIMPLE_BACKGROUND_HPP
#define WF_CUBE_SIMPLE_BACKGROUND_HPP

#include "cube-background.hpp"

class wf_cube_simple_background : public wf_cube_background_base
{
    wf_option background_color;

    public:
    wf_cube_simple_background();
    virtual void render_frame(const wf_framebuffer& fb,
        wf_cube_animation_attribs& attribs) override;
};

#endif /* end of include guard: WF_CUBE_SIMPLE_BACKGROUND_HPP */
