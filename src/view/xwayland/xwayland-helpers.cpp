#include "xwayland-helpers.hpp"

extern xcb_atom_t _NET_WM_WINDOW_TYPE_NORMAL;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_DIALOG;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_SPLASH;
extern xcb_atom_t _NET_WM_WINDOW_TYPE_DND;

void wf::xw::load_atom(xcb_connection_t *connection, xcb_atom_t& atom,
    const std::string& name)
{
    auto cookie = xcb_intern_atom(connection, 0, name.length(), name.c_str());

    xcb_generic_error_t *error = NULL;
    xcb_intern_atom_reply_t *reply;
    reply = xcb_intern_atom_reply(connection, cookie, &error);

    bool success = !error && reply;
    if (success)
    {
        atom = reply->atom;
    }

    free(reply);
    free(error);
}

bool wf::xw::load_atoms(const char *server_name)
{
    auto connection = xcb_connect(server_name, NULL);
    if (!connection || xcb_connection_has_error(connection))
    {
        return false;
    }

    load_atom(connection, _NET_WM_WINDOW_TYPE_NORMAL,
        "_NET_WM_WINDOW_TYPE_NORMAL");
    load_atom(connection, _NET_WM_WINDOW_TYPE_DIALOG,
        "_NET_WM_WINDOW_TYPE_DIALOG");
    load_atom(connection, _NET_WM_WINDOW_TYPE_SPLASH,
        "_NET_WM_WINDOW_TYPE_SPLASH");
    load_atom(connection, _NET_WM_WINDOW_TYPE_DND,
        "_NET_WM_WINDOW_TYPE_DND");

    xcb_disconnect(connection);
    return true;
}

bool wf::xw::xwayland_surface_has_type(wlr_xwayland_surface *xw, xcb_atom_t type)
{
    for (size_t i = 0; i < xw->window_type_len; i++)
    {
        if (xw->window_type[i] == type)
        {
            return true;
        }
    }

    return false;
}

wf::xw::window_type_t wf::xw::get_window_type(wlr_xwayland_surface *xw)
{
    if (xwayland_surface_has_type(xw, _NET_WM_WINDOW_TYPE_DND))
    {
        return window_type_t::DND;
    }

    if (xw->override_redirect)
    {
        return window_type_t::OR;
    }

    /** Example: Android Studio dialogs */
    if (xw->parent && !xwayland_surface_has_type(xw, _NET_WM_WINDOW_TYPE_NORMAL))
    {
        return window_type_t::OR;
    }

    if (xwayland_surface_has_type(xw, _NET_WM_WINDOW_TYPE_DIALOG) ||
        (xw->parent && (xw->window_type_len == 0)))
    {
        return window_type_t::DIALOG;
    }

    return window_type_t::TOPLEVEL;
}
