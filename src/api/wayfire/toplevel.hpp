#pragma once

#include <wayfire/geometry.hpp>
#include <wayfire/output.hpp>
#include <wayfire/nonstd/wlroots.hpp>

namespace wf
{
/**
 * A bitmask consisting of all tiled edges.
 * This corresponds to a maximized state.
 */
constexpr uint32_t TILED_EDGES_ALL =
    WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT;

/**
 * Describes a state of a toplevel.
 */
struct toplevel_state_t
{
    /**
     * The primary output of a toplevel.
     * The toplevel may be displayed on zero or more outputs by creating views
     * for this toplevel and its surfaces, however, the primary output serves
     * as a reference for the geometry of the toplevel.
     */
    wf::output_t *primary_output = NULL;

    /**
     * The geometry of the base surface of the toplevel, excluding shadows.
     * This may not be the final geometry of the toplevel on the screen, which
     * is also subject to transformers.
     *
     * Geometry is in the coordinate system of the toplevel's primary output.
     */
    wf::geometry_t geometry = {0, 0, 0, 0};

    /**
     * The geometry of the base surface of the toplevel, including shadows (if
     * they are drawn by the client). This may not be the final geometry of the
     * toplevel on the screen, which is also subject to transformers.
     *
     * Geometry is in the coordinate system of the toplevel's primary output.
     */
    wf::geometry_t base_geometry = {0, 0, 0, 0};

    /**
     * The fullscreen state of the toplevel.
     * A toplevel with fullscreen state typically has geometry equal to the
     * full output geometry of its primary output, and is visible only on it.
     */
    bool fullscreen = false;

    /**
     * The activated state of the toplevel.
     * An activated toplevel usually has the keyboard focus and the client
     * usually shows visual indication of that fact.
     */
    bool activated = false;

    /**
     * The minimized state of the toplevel.
     * Minimized toplevels' views are typically hidden from the user, but
     * remain on the workspace where they were last visible.
     */
    bool minimized = false;

    /**
     * The sticky state of the toplevel.
     * If a toplevel is sticky, then its position is and should be adjusted
     * so that it is always visible on the current workspace.
     */
    bool sticky = false;

    /**
     * The tiled edges of a toplevel.
     * If a toplevel is tiled to an edge, usually there is another surface
     * or an output edge right next to that edge. Clients typically hide
     * shadows and other decorative elements on these edges.
     *
     * If all tiled edges are set, then the view is effectively maximized.
     */
    uint32_t tiled_edges = 0;
};

/**
 * Toplevels are desktop surfaces which represent application windows only.
 * They have additional configurable state like geometry, tiled edges, etc.
 */
class toplevel_t
{
  public:
    toplevel_t(const toplevel_t&) = delete;
    toplevel_t(toplevel_t&&) = delete;
    toplevel_t& operator =(const toplevel_t&) = delete;
    toplevel_t& operator =(toplevel_t&&) = delete;
    virtual ~toplevel_t() = default;

    virtual toplevel_state_t& current() = 0;

    /** Set the minimized state of the view. */
    virtual void set_minimized(bool minimized);
    /** Set the tiled edges of the view */
    virtual void set_tiled(uint32_t edges);
    /** Set the fullscreen state of the view */
    virtual void set_fullscreen(bool fullscreen);
    /** Set the view's activated state.  */
    virtual void set_activated(bool active);
    /** Set the view's sticky state. */
    virtual void set_sticky(bool sticky);

    /** Move the view to the given output-local coordinates.  */
    virtual void move(int x, int y) = 0;

    /**
     * Request that the view change its size to the given dimensions. The view
     * is not obliged to assume the given dimensions.
     *
     * Maximized and tiled views typically do obey the resize request.
     */
    virtual void resize(int w, int h);

    /**
     * A convenience function, has the same effect as calling move and resize
     * atomically.
     */
    virtual void set_geometry(wf::geometry_t g);

    /**
     * Start a resizing mode for this view. While a view is resizing, one edge
     * or corner of the view is made immobile (exactly the edge/corner opposite
     * to the edges which are set as resizing)
     *
     * @param resizing whether to enable or disable resizing mode
     * @param edges the edges which are being resized
     */
    virtual void set_resizing(bool resizing, uint32_t edges = 0);

    /**
     * Set the view in moving mode.
     *
     * @param moving whether to enable or disable moving mode
     */
    virtual void set_moving(bool moving);

    /**
     * Request that the view resizes to its native size.
     */
    virtual void request_native_size();

    virtual void set_output(wf::output_t *new_output);
};
}
