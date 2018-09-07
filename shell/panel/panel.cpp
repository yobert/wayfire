#include <sstream>
#include <unistd.h>
#include <sys/time.h>
#include <linux/input-event-codes.h>
#include "panel.hpp"
#include "widgets.hpp"

void panel_redraw(void *data, wl_callback*, uint32_t)
{
    wayfire_panel *panel = (wayfire_panel*) data;
    panel->render_frame();
}

static const struct wl_callback_listener frame_listener = {
    panel_redraw
};

static void load_misc_config(wayfire_config *config)
{
    auto section = config->get_section("shell_panel");

    widget::background_color = *section->get_option("background_color",
            "0.033 0.041 0.047 0.9");
    widget::font_size = *section->get_option("font_size", "25");
}

void zwf_output_hide_panels(void *data,
                            struct zwf_output_v1 *zwf_output_v1,
                            uint32_t autohide)
{
    auto panel = (wayfire_panel*) data;
    panel->set_autohide(autohide);
}

const struct zwf_output_v1_listener zwf_output_impl =
{
    zwf_output_hide_panels
};

wayfire_panel::wayfire_panel(wayfire_config *config, wayfire_output *output)
{
    this->config = config;
    this->output = output;
    load_misc_config(config);

    autohide_opt = (*config)["shell_panel"]->get_option("autohide", "1");
    autohide = (bool)autohide_opt->as_int();

    output->resized_callback = [=] (wayfire_output *output, int w, int h)
    { init(w, h); };

    output->destroyed_callback = [=] (wayfire_output *output)
    { delete this; };

    zwf_output_v1_add_listener(output->zwf, &zwf_output_impl, this);
}

wayfire_panel::~wayfire_panel()
{
    destroy();
}

void wayfire_panel::init(int w, int h)
{
    width = w;
    height = 1.3 * widget::font_size;

    /* we check if the window has been configured, because if we had a
     * very fast subsequent resizes, the window might still haven't got
     * the resize event and thus it would have inhibited the output, in
     * which case we don't have to do it again */
    if (!window || window->configured)
        zwf_output_v1_inhibit_output(output->zwf);

    if (window)
        destroy();

    window = output->create_window(width, height, [=] ()
    {
        configure();
    });

    cr = cairo_create(window->cairo_surface);
}

void wayfire_panel::destroy()
{
    cairo_destroy(clock->cr);
    cairo_destroy(launchers->cr);

    clock = nullptr;
    launchers = nullptr;

    cairo_destroy(cr);
    delete window;
}

void wayfire_panel::configure()
{
    window->zwf = zwf_output_v1_get_wm_surface(output->zwf, window->surface,
                                               ZWF_OUTPUT_V1_WM_ROLE_PANEL);

    init_input();
    init_widgets();

    if (repaint_callback)
        wl_callback_destroy(repaint_callback);
    repaint_callback = nullptr;

    if (!autohide)
        zwf_wm_surface_v1_set_exclusive_zone(window->zwf, ZWF_WM_SURFACE_V1_ANCHOR_EDGE_TOP, height);

    zwf_wm_surface_v1_configure(window->zwf, 0, -height);

    state = HIDDEN;
    animation.y = -height;
    show(0);

    render_frame(true);
    zwf_output_v1_inhibit_output_done(output->zwf);
}

void wayfire_panel::init_input()
{
    using namespace std::placeholders;
    window->pointer_enter = [=] (wl_pointer*, uint32_t time, int, int)
    {
        show(200);
        on_enter(time);
        ++count_input;
    };

    window->pointer_leave = [=] ()
    {
        --count_input;
        if (!count_input)
        {
            if (autohide)
                hide(300);
            on_leave();
        }
    };

    window->pointer_move  = std::bind(std::mem_fn(&wayfire_panel::on_motion), this, _1, _2);
    window->pointer_button= std::bind(std::mem_fn(&wayfire_panel::on_button), this, _1, _2, _3, _4);
}

void wayfire_panel::init_widgets()
{
    cr = cairo_create(window->cairo_surface);

    clock = std::unique_ptr<clock_widget> (new clock_widget());
    clock->cr = cairo_create(window->cairo_surface);
    clock->panel_h = height;
    clock->create();

    launchers = std::unique_ptr<launchers_widget> (new launchers_widget());
    launchers->init_launchers(config);
    launchers->cr = cairo_create(window->cairo_surface);
    launchers->panel_h = height;
    launchers->create();
}

void wayfire_panel::position_widgets()
{
    int widget_spacing = widget::font_size * 0.5;

    launchers->x = widget_spacing;
    clock->x = width - clock->get_width() - widget_spacing;
}

void wayfire_panel::set_autohide(bool ah)
{
    autohide += ah ? 1 : -1;

    if (autohide == 0)
    {
        show(0);
        on_enter(0);
    } else if (!count_input)
    {
        hide(0);
        on_leave();
    }
}

static inline void set_timer_target(timeval& timer, int delay)
{
    gettimeofday(&timer, 0);
    timer.tv_usec += delay * 1000;

    const int usec_in_sec = 1000000;
    timer.tv_sec += timer.tv_usec / usec_in_sec;
    timer.tv_usec %= usec_in_sec;
}

void wayfire_panel::show(int delay)
{
    if (animation.dy <= 0)
    {
        animation.target = 0;
        animation.dy = 5;
    }

    if (state & SHOWN)
    {
        state = HIDDEN | ANIMATING;
        return;
    } else if (!(state & WAITING))
    {
        state = HIDDEN | WAITING;
        set_timer_target(timer_target, delay);
        add_callback(false);
    }
}

void wayfire_panel::hide(int delay)
{
    if (animation.dy >= 0)
    {
        animation.target = hidden_height - height;
        animation.dy = -5;
    }

    if (state & HIDDEN)
    {
        if (state == (HIDDEN | WAITING))
            state = HIDDEN;
        else
            state = SHOWN | ANIMATING;
        return;
    } else if (!(state & WAITING))
    {
        state = SHOWN | WAITING;
        set_timer_target(timer_target, delay);
        add_callback(false);
    }
}

void wayfire_panel::on_enter(uint32_t serial)
{
    output->display->show_default_cursor(serial);
    add_callback(false);
}

void wayfire_panel::on_leave()
{
    on_motion(-1, -1);
}

void wayfire_panel::on_button(uint32_t button, uint32_t state, int x, int y)
{
    launchers->pointer_button(button, state, x, y);
}

void wayfire_panel::on_motion(int x, int y)
{
    launchers->pointer_motion(x, y);
}

void wayfire_panel::add_callback(bool swapped)
{
    if (repaint_callback)
        wl_callback_destroy(repaint_callback);

    repaint_callback = wl_surface_frame(window->surface);
    wl_callback_add_listener(repaint_callback, &frame_listener, this);

    if (!swapped)
        wl_surface_commit(window->surface);
}


void wayfire_panel::render_frame(bool first_call)
{
    /* maybe a resize, the window still hasn't been initialized */
    if (!window || !window->zwf)
        return;

    if (state & WAITING)
    {
        timeval time;
        gettimeofday(&time, 0);

        long long delta = (timer_target.tv_sec - time.tv_sec) * 1e6 +
            (timer_target.tv_usec - time.tv_usec);

        if (delta <= 0)
        {
            state &= ~WAITING;
            state |= ANIMATING;
        }
    }

    if (state & ANIMATING) {
        animation.y += animation.dy;

        if (animation.y * animation.dy > animation.target * animation.dy)
        {
            animation.y = animation.target;
            if (state & HIDDEN)
            {
                state = SHOWN;

                if (!count_input && autohide)
                    hide(300);
            }
            else
            {
                state = HIDDEN;
            }
        }

        zwf_wm_surface_v1_configure(window->zwf, 0, animation.y);
    }

    bool should_swap = first_call;
    if (animation.target == 0 || !autohide)
    {
        should_swap |= launchers->update();
        should_swap |= clock->update();
    }

    should_swap = should_swap || need_fullredraw;

    if (should_swap)
    {
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        render_rounded_rectangle(cr, 0, 0, width, height,
                0, widget::background_color.r, widget::background_color.g,
                widget::background_color.b, widget::background_color.a);
        need_fullredraw = false;

        position_widgets();
        launchers->repaint();
        clock->repaint();
    }

    /* we don't need to redraw only if we are autohiding and hidden now */
    if (state != HIDDEN)
        add_callback(should_swap);

    if (should_swap)
        window->damage_commit();

    /* don't repaint too often if there is no interaction with the panel,
     * otherwise the panel eats up some CPU power */
    if (((state & (WAITING | ANIMATING)) == 0) && count_input <= 0)
        usleep(1e6/10);
}
