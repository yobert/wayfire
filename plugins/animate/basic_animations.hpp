#include "animate.hpp"
#include <debug.hpp>
#include <plugin.hpp>
#include <opengl.hpp>
#include <view-transform.hpp>
#include <output.hpp>

class fade_animation : public animation_base
{
    wayfire_view view;
    wf_2D_view *our_transform = nullptr;

    float start = 0, end = 1;
    wf_duration duration;

    public:

    void init(wayfire_view view, wf_duration dur, bool close)
    {
        this->view = view;
        duration = dur;
        duration.start();

        if (close)
            std::swap(start, end);

        auto output = view->get_output();
        our_transform = new wf_2D_view(output);
        view->set_transformer(std::unique_ptr<wf_2D_view> (our_transform));
    }

    bool step()
    {
        if (view->get_transformer() != our_transform)
            return false;

        log_info("duration has progress %f", duration.progress_percentage());

        our_transform->alpha = duration.progress(start, end);
        return duration.running();
    }

    ~fade_animation()
    {
        view->alpha = 1.0f;
        if (view->get_transformer() == our_transform)
            view->set_transformer(nullptr);
    }
};

class zoom_animation : public animation_base
{
    wayfire_view view;
    wf_2D_view *our_transform = nullptr;

    float alpha_start = 0, alpha_end = 1;
    float zoom_start = 1./3, zoom_end = 1;
    wf_duration duration;

    public:

    void init(wayfire_view view, wf_duration dur, bool close)
    {
        this->view = view;
        duration = dur;
        duration.start();

        if (close)
        {
            std::swap(alpha_start, alpha_end);
            std::swap(zoom_start, zoom_end);
        }

        auto output = view->get_output();
        our_transform = new wf_2D_view(output);
        view->set_transformer(std::unique_ptr<wf_2D_view> (our_transform));
    }

    bool step()
    {
        if (view->get_transformer() != our_transform)
            return false;

        float c = duration.progress(zoom_start, zoom_end);
        our_transform->alpha = duration.progress(alpha_start, alpha_end);
        our_transform->scale_x = c;
        our_transform->scale_y = c;

        return duration.running();
    }

    ~zoom_animation()
    {
        if (view->get_transformer() == our_transform)
            view->set_transformer(nullptr);
        view->alpha = 1.0;
    }
};
