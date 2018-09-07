#ifndef PANEL_HPP
#define PANEL_HPP

#include "window.hpp"
#include <config.hpp>
#include <vector>

#include "widgets.hpp"

class wayfire_config;
class wayfire_panel
{
    wl_callback *repaint_callback = nullptr;
    cairo_t *cr;

    wayfire_output *output;
    uint32_t width, height;

    int last_x, last_y;

    int hidden_height = 1;

    wf_option autohide_opt;
    int autohide = 0;

    bool need_fullredraw = false;

    struct {
        int dy = 0;
        int y, target;
    } animation;

    timeval timer_target;
    enum animation_state
    { WAITING = (1 << 0),
      ANIMATING = (1 << 1),
      HIDDEN = (1 << 2),
      SHOWN = (1 << 3)
    };
    uint32_t state = HIDDEN;

    void show(int delay_ms);
    void hide(int delay_ms);

    int count_input = 0;
    void on_enter(uint32_t);
    void on_leave();

    void on_button(uint32_t, uint32_t, int, int);
    void on_motion(int, int);

    void add_callback(bool swapped);

    std::unique_ptr<clock_widget> clock;
    std::unique_ptr<launchers_widget> launchers;

    void position_widgets();
    void init_widgets();

    void init(int32_t width, int32_t height);
    void init_input();

    void configure();
    void destroy();

    wayfire_config *config;

    public:
        wayfire_window *window = nullptr;
        wayfire_panel(wayfire_config *config, wayfire_output *output);
        ~wayfire_panel();

        void render_frame(bool first_call = false);
        void set_autohide(bool ah);
};

#endif /* end of include guard: PANEL_HPP */
