#ifndef WOBBLY_SIGNAL_HPP
#define WOBBLY_SIGNAL_HPP

#include <signal-definitions.hpp>

enum wobbly_event
{
    WOBBLY_EVENT_GRAB      = (1 << 0),
    WOBBLY_EVENT_MOVE      = (1 << 1),
    WOBBLY_EVENT_RESIZE    = (1 << 2),
    WOBBLY_EVENT_END       = (1 << 3),
    WOBBLY_EVENT_SNAP      = (1 << 4),
    WOBBLY_EVENT_UNSNAP    = (1 << 5),
    WOBBLY_EVENT_TRANSLATE = (1 << 6)
};

enum wobbly_corner
{
    WOBBLY_CORNER_TL = 0,
    WOBBLY_CORNER_TR = 1,
    WOBBLY_CORNER_BL = 2,
    WOBBLY_CORNER_BR = 3,
};

struct wobbly_signal : public _view_signal
{
    wobbly_event events;
    int grab_x, grab_y; // set only with events & WOBBLY_EVENT_GRAB
    wf_geometry geometry; // use only the needed fields
    bool unanchor;
};

inline void start_wobbly(wayfire_view view, int grab_x, int grab_y)
{
    wobbly_signal sig;
    sig.view = view;
    sig.events = WOBBLY_EVENT_GRAB;
    sig.grab_x = grab_x;
    sig.grab_y = grab_y;

    view->get_output()->emit_signal("wobbly-event", &sig);
}

inline void end_wobbly(wayfire_view view, bool unanchor = true)
{
    wobbly_signal sig;
    sig.view = view;
    sig.events = WOBBLY_EVENT_END;
    sig.unanchor = unanchor;
    view->get_output()->emit_signal("wobbly-event", &sig);
}

inline void move_wobbly(wayfire_view view, int grab_x, int grab_y)
{
    wobbly_signal sig;
    sig.view = view;
    sig.events = WOBBLY_EVENT_MOVE;
    sig.geometry.x = grab_x;
    sig.geometry.y = grab_y;

    view->get_output()->emit_signal("wobbly-event", &sig);
}

inline void resize_wobbly(wayfire_view view, int w, int h)
{
    wobbly_signal sig;
    sig.view = view;
    sig.events = WOBBLY_EVENT_RESIZE;
    sig.geometry.width = w;
    sig.geometry.height = h;

    view->get_output()->emit_signal("wobbly-event", &sig);
}

inline void snap_wobbly(wayfire_view view, wf_geometry geometry, bool snap = true)
{
    wobbly_signal sig;
    sig.view = view;
    sig.events = snap ? WOBBLY_EVENT_SNAP : WOBBLY_EVENT_UNSNAP;
    sig.geometry = geometry;

    view->get_output()->emit_signal("wobbly-event", &sig);
}

inline void translate_wobbly(wayfire_view view, int dx, int dy)
{
    wobbly_signal sig;
    sig.view = view;
    sig.events = WOBBLY_EVENT_TRANSLATE;
    sig.geometry.x = dx;
    sig.geometry.y=  dy;

    view->get_output()->emit_signal("wobbly-event", &sig);
}
#endif /* end of include guard: WOBBLY_SIGNAL_HPP */
