/**
 * The functions provided in this file are useful for core and many other
 * plugins when managing a toplevel.
 *
 * Note that these functions are not mandatory to use, and can be reimplemented
 * by plugins. However, using them makes cooperation with the core plugins
 * easier.
 */
#pragma once

#include <wayfire/nonstd/observer_ptr.h>
#include <wayfire/toplevel.hpp>
#include <wayfire/view.hpp>

namespace wf
{
/**
 * Find the 'primary' view for the given toplevel.
 *
 * Wayfire's core creates a single view per toplevel by default, but plugins may
 * add additional views on outputs other than the toplevel's primary output.
 *
 * This function attempts to find the original view by selecting the oldest of
 * the toplevel's associated views which is on its primary output. In case of no
 * views on the toplevel's primary output, simply the oldest view is selected.
 */
wayfire_view toplevel_find_primary_view(const toplevel_sptr_t& toplevel);

/**
 * An extension of wf::toplevel_t to keep the last non-tiled and non-fullscreen
 * geometry. Useful for plugins implementing untiling and unfullscreening.
 */
class toplevel_saved_geometry_t : public wf::custom_data_t
{
  public:
    /**
     * Last geometry of the toplevel when it was neither fullscreened nor tiled.
     */
    std::optional<wf::geometry_t> last_windowed_geometry;

    /**
     * The workarea size where this toplevel last had its non-fullscreen
     * non-tiled geometry.
     */
    std::optional<wf::geometry_t> saved_workarea;

    /**
     * Store the toplevel's current geometry.
     * This should only be called when the toplevel is not tiled or fullscreen.
     */
    void store_geometry(const toplevel_sptr_t& toplevel);

    /**
     * Calculate the geometry the toplevel should be restored to.
     * The previous geometry will be scaled to match the given workarea size,
     * e.g. if the toplevel took half of the saved workarea, the computed
     * geometry will take half of the new workarea.
     */
    std::optional<wf::geometry_t> calculate_geometry(
        const wf::geometry_t& workarea);
};

/**
 * Emit the view-move-request signal for this view.
 * This is a no-op unless there is a plugin like Move which can start an
 * interactive move operation on the toplevel.
 */
void toplevel_emit_move_request(const toplevel_sptr_t& toplevel);

/**
 * Emit the view-move-request signal for this view.
 * This is a no-op unless there is a plugin like Resize which can start an
 * interactive resize operation on the toplevel.
 *
 * @param edges The edges of the toplevel which should be resized. The opposite
 * edges should stay immobile.
 */
void toplevel_emit_resize_request(const toplevel_sptr_t& toplevel,
    uint32_t edges = 0);

/**
 * Emit the view-minimize-request for this view.
 *
 * If no plugin handles this, a default action is taken:
 * - All of the toplevel's associated views are moved to/out of the minimized
 *   layers of their respective workspace sets.
 * - The toplevel's minimized state is updated.
 */
void toplevel_emit_minimize_request(const toplevel_sptr_t& toplevel,
    bool minimized);

/**
 * Emit the view-tile-request signal for this toplevel.
 *
 * If no plugins handles this, a default action is taken:
 *  - The toplevel's tiled edges are set to @tiled_edges
 *  - The toplevel's geometry is adjusted to the full workarea of its current
 *    workspace set, if the toplevel is being maximized, otherwise, it is
 *    restored to its last non-tiled geometry or to its native size.
 *  - The toplevel is moved to the desired workspace, if specified.
 *
 * This request interacts with the toplevel_saved_geometry_t extension of a
 * toplevel to store windowed geometry and restore it when the toplevel is no
 * longer tiled.
 */
void toplevel_emit_tile_request(const toplevel_sptr_t& toplevel,
    uint32_t tiled_edges, std::optional<wf::point_t> ws = {});

/**
 * Emit the view-fullscreen-request signal for this toplevel.
 *
 * If no plugins handles this, a default action is taken:
 *  - The toplevel is moved to the desired output.
 *  - The toplevel's fullscreen state is set to @state
 *  - The toplevel's geometry is adjusted to the full extents of its current
 *    workspace set, if the toplevel is being fullscreened, otherwise, it is
 *    restored to its last non-fullscreen geometry or to its native size.
 *  - The toplevel is moved to the desired workspace, if specified.
 *
 * This request interacts with the toplevel_saved_geometry_t extension of a
 * toplevel to store windowed geometry and restore it when the toplevel is no
 * longer fullscreen.
 */
void toplevel_emit_fullscreen_request(const toplevel_sptr_t& toplevel,
    wf::output_t *output, bool state, std::optional<wf::point_t> ws = {});
}
