#include <memory>
#include <getopt.h>

#include "config.hpp"
#include "window.hpp"

static cairo_surface_t *create_dummy_surface(int w, int h)
{
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cairo_t *cr = cairo_create(surf);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_fill(cr);
    cairo_destroy(cr);

    return surf;
}

static cairo_surface_t *create_cairo_surface_from_file(std::string name, int w, int h)
{
    auto surface = cairo_try_load_png(name.c_str());
    return surface ?: create_dummy_surface(w, h);
}

class wayfire_background
{
    std::string image;

    cairo_surface_t *img_surface = NULL;
    cairo_t *cr = NULL;

    wayfire_output *output;
    wayfire_window *window = nullptr;

    public:
    wayfire_background(wayfire_output *output, std::string image)
    {
        this->image = image;
        this->output = output;
        output->resized_callback = [=] (wayfire_output *output, int w, int h)
        { resize(w, h); };

        output->destroyed_callback = [=] (wayfire_output* output)
        { delete this; };

        zwf_output_v1_inhibit_output(output->zwf);
    }

    void resize(int w, int h)
    {
        if (!img_surface)
            img_surface = create_cairo_surface_from_file(image, w, h);

        if (window)
        {
            /* the first inhibit was called in the constructor
             *
             * we check if the window has been configured, because if we had a
             * very fast subsequent resizes, the window might still haven't got
             * the resize event and thus it would have inhibited the output, in
             * which case we don't have to do it again */
            if (window->configured)
                zwf_output_v1_inhibit_output(output->zwf);
            delete window;
        }

        window = output->create_window(w, h, [=] () { init_background(w, h); });
    }

    void init_background(int width, int height)
    {
        window->zwf = zwf_output_v1_get_wm_surface(output->zwf, window->surface,
                                                   ZWF_OUTPUT_V1_WM_ROLE_BACKGROUND);
        zwf_wm_surface_v1_configure(window->zwf, 0, 0);

        using namespace std::placeholders;
        window->pointer_enter = std::bind(std::mem_fn(&wayfire_background::on_enter),
                                          this, _1, _2, _3, _4);

        cr = cairo_create(window->cairo_surface);

        double img_w = cairo_image_surface_get_width(img_surface);
        double img_h = cairo_image_surface_get_height(img_surface);

        cairo_rectangle(cr, 0, 0, width, height);
        cairo_scale(cr, width / img_w, height / img_h);
        cairo_set_source_surface(cr, img_surface, 0, 0);
        cairo_fill(cr);

        window->damage_commit();
        zwf_output_v1_inhibit_output_done(output->zwf);
    }

    void on_enter(wl_pointer *ptr, uint32_t serial, int x, int y)
    {
        output->display->show_default_cursor(serial);
    }

    ~wayfire_background()
    {
        if (window)
        {
            cairo_destroy(cr);
            delete window;
            cairo_surface_destroy(img_surface);
        }
    }
};

wayfire_config *config;
std::map<uint32_t, std::unique_ptr<wayfire_background>> outputs;

std::string bg_path;

/* TODO: share option parsing between panel and background */
int main(int argc, char *argv[])
{
    struct option opts[] = {
        { "image", required_argument, NULL, 'i' },
        { 0,       0,                 NULL,  0  }
    };

    int c, i;
    while((c = getopt_long(argc, argv, "i:", opts, &i)) != -1)
    {
        switch(c)
        {
            case 'i':
                bg_path = optarg;
                break;
            default:
                std::cerr << "failed to parse option " << optarg << std::endl;
        }
    }

    auto display = new wayfire_display([=] (wayfire_output *output)
    {
        new wayfire_background(output, bg_path);
    });

    while(true)
    {
        if (wl_display_dispatch(display->display) < 0)
            break;
    }

    delete display;
    return 0;
}
