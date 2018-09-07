#include "widgets.hpp"
#include "window.hpp"

#include <iostream>
#include <chrono>
#include <ctime>
#include <unistd.h>
#include <sys/wait.h>
#include <cmath>
#include <wayland-client.h>
#include <linux/input-event-codes.h>
#include <mutex>
#include <thread>

#include <sys/dir.h>
#include <dirent.h>

#include <freetype2/ft2build.h>
#include <pthread.h>

wf_color widget::background_color;
int32_t widget::font_size;


/* -------------------- Clock widget ----------------- */
void clock_widget::create()
{
    width = font_size * 18;
    this->current_text = "";
}

const std::string months[] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

std::string format(int x)
{
    if (x < 10) {
        return "0" + std::to_string(x);
    } else {
        return std::to_string(x);
    }
}

bool clock_widget::update()
{
    using std::chrono::system_clock;

    time_t now = system_clock::to_time_t(system_clock::now());
    auto time = std::localtime(&now);

    std::string time_string = std::to_string(time->tm_mday) + " " +
        months[time->tm_mon] + " " + format(time->tm_hour) +
        ":" + format(time->tm_min);

    if (time_string != this->current_text)
    {
        current_text = time_string;

        cairo_set_font_size(cr, font_size);

        cairo_text_extents_t te;
        cairo_text_extents(cr, current_text.c_str(), &te);

        width = te.width;

        return true;
    } else
    {
        return false;
    }
}

void clock_widget::repaint()
{
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); /* blank to white */

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgb(cr, 0.91, 0.918, 0.965);

    cairo_move_to(cr, x, font_size);
    cairo_show_text(cr, current_text.c_str());
}

/* --------------- Launchers widget ---------------------- */
struct launcher
{
    cairo_surface_t *img;
    std::string command;

    float scale;

    int x, y, size;
    bool active = false;
};

void execute(std::string cmd)
{
    pid_t pid = fork();

    /* The following is a "hack" for disowning the child processes,
     * otherwise they will simply stay as zombie processes */
    if (!pid)
    {
        if (!fork())
        {
            exit(execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), NULL));
        } else
        {
            exit(0);
        }
    } else
    {
        int status;
        waitpid(pid, &status, 0);
    }
}

/* format in config file:
 * launcher1_icon
 * launcher1_cmd
 */

constexpr float default_launcher_scale = 0.9;
constexpr float hover_launcher_scale = 1.1;

void launchers_widget::init_launchers(wayfire_config *config)
{
    auto section = config->get_section("shell_panel");

    for (int i = 0; i < 20; i++)
    {
        std::string icon = *section->get_option("launcher" + std::to_string(i) + "_icon", "");
        std::string cmd =  *section->get_option("launcher" + std::to_string(i) + "_cmd", "");

        if (icon == "" || cmd == "")
            continue;

        launcher *l = new launcher;
        l->scale = default_launcher_scale;

        l->img = cairo_try_load_png(icon.c_str());

        if (!l->img)
        {
            delete l;
            continue;
        }

        l->command = cmd;
        launchers.push_back(l);
    }
}

#define pointer_in_launcher(l,x,y) (l->x <= x && l->y <= y && \
                l->x + l->size > x && l->y + l->size > y)

int icon_offset, base_icon_size;

void launchers_widget::create()
{
    icon_offset = font_size * 0.5;
    base_icon_size = font_size * 1.1;

    width = launchers.size() * (base_icon_size + icon_offset) - icon_offset;

    pointer_motion = [=] (int x, int y)
    {
        for (auto l : launchers)
        {
            bool was_active = l->active;
            if (pointer_in_launcher(l, x, y))
            {
                l->scale = hover_launcher_scale;
                l->active = true;
            } else {
                l->scale = default_launcher_scale;
                l->active = false;
            }

            if (was_active != l->active)
                need_repaint = true;
        }
    };

    pointer_button = [=] (uint32_t button, uint32_t state, int x, int y)
    {
        if (button != BTN_LEFT || state != WL_POINTER_BUTTON_STATE_RELEASED)
            return;

        for (auto l : launchers)
        {
            if (pointer_in_launcher(l, x, y))
                execute(l->command);
        }
    };

}

void launchers_widget::repaint()
{
    cairo_identity_matrix(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    for (size_t i = 0; i < launchers.size(); i++)
    {
        int icon_size = base_icon_size * launchers[i]->scale;

        int sy = (panel_h - icon_size) / 2;
        int sx = x + i * (base_icon_size + icon_offset) - (icon_size - base_icon_size) / 2;

        launchers[i]->x = sx;
        launchers[i]->y = sy;
        launchers[i]->size = icon_size;

        double img_w = cairo_image_surface_get_width(launchers[i]->img);
        double img_h = cairo_image_surface_get_height(launchers[i]->img);

        cairo_identity_matrix(cr);
        cairo_new_path(cr);

        float scale_w = 1.0 * icon_size / img_w;
        float scale_h = 1.0 * icon_size / img_h;
        cairo_scale(cr, scale_w, scale_h);

        cairo_rectangle(cr, sx / scale_w, sy / scale_h, icon_size / scale_w, icon_size / scale_h);
        cairo_set_source_rgba(cr, 1, 1, 1, 1);
        cairo_set_source_surface(cr, launchers[i]->img, sx / scale_w, sy / scale_h);
        cairo_fill(cr);
    }

    need_repaint = false;
}

bool launchers_widget::update()
{
    return need_repaint;
}

launchers_widget::~launchers_widget()
{
    for (auto l : launchers)
    {
        cairo_surface_destroy(l->img);
        delete l;
    }
}
