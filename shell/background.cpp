#include "background.hpp"
#include "../proto/wayfire-shell-client.h"

#if HAS_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkcairo.h>
#endif

wayfire_background::wayfire_background(std::string image)
{
    this->image = image;
}

bool g_type_init_ran = false;

static cairo_surface_t *create_cairo_surface_from_file(std::string name)
{
#if HAS_PIXBUF

#if !GLIB_CHECK_VERSION(2,35,0)
    if (!g_type_init_ran)
    {
        g_type_init();
        g_type_init_ran = true;
    }
#endif

    GError *err = NULL;
    auto pbuf = gdk_pixbuf_new_from_file(name.c_str(), &err);

    if (!pbuf)
    {
        std::cerr << "Failed to create a pbuf" << std::endl;
        unsigned char data[] = {0, 0, 0, 1} ;
        cairo_surface_t *surf = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, 1, 1, 0);
        return surf;
    }


    int w = gdk_pixbuf_get_width(pbuf);
    int h = gdk_pixbuf_get_height(pbuf);

    auto surface = cairo_image_surface_create(
        gdk_pixbuf_get_has_alpha(pbuf) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
        w, h);

    auto cr = cairo_create(surface);
    gdk_cairo_set_source_pixbuf(cr, pbuf, 0, 0);
    cairo_paint(cr);

    cairo_destroy(cr);
    g_object_unref(pbuf);

    return surface;
#else
    return cairo_image_surface_create_from_png(name.c_str());
#endif
}

void wayfire_background::create_background(uint32_t output, uint32_t w, uint32_t h)
{
    this->output = output;
    window = create_window(w, h);
    wayfire_shell_add_background(display.wfshell, output, window->surface, 0, 0);

    using namespace std::placeholders;
    window->pointer_enter = std::bind(std::mem_fn(&wayfire_background::on_enter),
            this, _1, _2, _3, _4);

    cr = cairo_create(window->cairo_surface);
    img_surface = create_cairo_surface_from_file(image);

    set_active_window(window);

    double img_w = cairo_image_surface_get_width(img_surface);
    double img_h = cairo_image_surface_get_height(img_surface);

    cairo_rectangle(cr, 0, 0, w, h);
    cairo_scale(cr, 1.0 * w / img_w, 1.0 * h / img_h);
    cairo_set_source_surface(cr, img_surface, 0, 0);

    cairo_fill(cr);

    damage_commit_window(window);
}

void wayfire_background::resize(uint32_t w, uint32_t h)
{
    cairo_destroy(cr);
    cairo_surface_destroy(img_surface);
    delete_window(window);
    create_background(output, w, h);
}

void wayfire_background::on_enter(wl_pointer *ptr, uint32_t serial, int x, int y)
{
    show_default_cursor(serial);
}
