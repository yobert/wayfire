#ifndef WIDGETS_HPP
#define WIDGETS_HPP

#include <cairo.h>
#include <string>
#include <functional>
#include <config.hpp>
#include <memory>

struct widget
{
    static wf_color background_color;
    static int32_t font_size;

    /* those are initialized before calling create() */
    cairo_t *cr;

    /* leftmost position in panel, panel height, maximum width */
    int x, panel_h, width = 0;


    /* only panel_h is visible, the widget still hasn't been positioned */
    virtual void create() = 0;

    /* must return the actual size of the widget
     * in pixels right after create() has been called */
    virtual int get_width() = 0;

    /* return true if widget has to be painted on this iter */
    virtual bool update() = 0;

    virtual void repaint() = 0;

    virtual ~widget() {};

    std::function<void(int x, int y)> pointer_motion = nullptr;
    std::function<void(uint32_t, uint32_t, int, int)> pointer_button = nullptr;
};

struct clock_widget : public widget
{
    std::string current_text;

    void create();
    int get_width() { return width; };
    bool update();
    bool resized();
    void repaint();
};

struct battery_info;
struct upower_backend;

struct battery_options
{
    static std::string icon_path_prefix;
    static bool invert_icons;
    static float text_scale;
};

struct launcher;
struct launchers_widget : public widget
{
    bool need_repaint = true;

    std::vector<launcher*> launchers;
    void init_launchers(wayfire_config *config);

    ~launchers_widget();

    void create();
    int get_width() { return width; };
    bool update();
    bool resized();
    void repaint();

    void on_pointer_button(uint32_t state, int x, int y);
    void on_pointer_motion(int x, int y);
};

#endif /* end of include guard: WIDGETS_HPP */
