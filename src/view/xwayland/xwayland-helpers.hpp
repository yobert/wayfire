#pragma once

#include <wayfire/nonstd/wlroots-full.hpp>
#include <string>

namespace wf
{
namespace xw
{
extern xcb_atom_t _NET_WM_WINDOW_TYPE_NORMAL;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_DIALOG;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_SPLASH;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_DND;

void load_atom(xcb_connection_t *connection, xcb_atom_t& atom, const std::string& name);
bool load_atoms(const char *server_name);

bool xwayland_surface_has_type(wlr_xwayland_surface* xw, xcb_atom_t type);

enum class window_type_t
{
    DND,
    OR,
    DIALOG,
    TOPLEVEL,
};
window_type_t get_window_type(wlr_xwayland_surface* xw);
}
}
