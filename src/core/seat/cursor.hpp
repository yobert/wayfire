#ifndef CURSOR_HPP
#define CURSOR_HPP

#include "seat.hpp"

struct axis_callback_data : wf_callback
{
    axis_callback *call;
    wf_option modifier;
};

struct button_callback_data : wf_callback
{
    button_callback *call;
    wf_option button;
};

struct wf_cursor
{
};

#endif /* end of include guard: CURSOR_HPP */
