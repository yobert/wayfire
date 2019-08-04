#ifndef WF_TILE_PLUGIN_TREE_CONTROLLER_HPP
#define WF_TILE_PLUGIN_TREE_CONTROLLER_HPP

#include "tree.hpp"
#include <config.hpp>

/* Contains functions which are related to manipulating the tiling tree */
namespace wf
{
class preview_indication_view_t;
namespace tile
{
/**
 * Represents the current mode in which the tile plugin is.
 *
 * Invariant: while a controller is active, the tree structure shouldn't change,
 * except for changes by the controller itself.
 *
 * If such an external event happens, then controller will be destroyed.
 */
class tile_controller_t
{
  public:
    virtual ~tile_controller_t() = default;

    /** Called when the input is moved */
    virtual void input_motion(wf_point input) {}

    /**
     * Called when the input is released or the controller should stop
     * Note that a controller may be deleted without receiving input_released(),
     * in which case it should simply stop operation.
     */
    virtual void input_released() {}
};

/**
 * Represents the moving view action, i.e dragging a window to change its
 * position in the grid
 */
class move_view_controller_t : public tile_controller_t
{
  public:
    /**
     * Start the dragging action.
     *
     * @param root The root of the tiling tree which is currently being
     *             manipulated
     * @param Where the grab has started
     */
    move_view_controller_t(std::unique_ptr<tree_node_t>& root,
        wf_point grab);

    /** Called when the input is released */
    ~move_view_controller_t();

    void input_motion(wf_point input) override;
    void input_released() override;

  protected:
    std::unique_ptr<tree_node_t>& root;
    nonstd::observer_ptr<view_node_t> grabbed_view;
    wf_point current_input;

    nonstd::observer_ptr<wf::preview_indication_view_t> preview;
    /**
     * Create preview if it doesn't exist
     *
     * @param now The position of the input now. Used only if the preview
     *            needs to be created.
     *
     * @param output The output on which to create the preview. Used only if
     *               the preview needs to be created.
     */
    void ensure_preview(wf_point now, wf::output_t *output);

    /**
     * Return the node under the input which is suitable for dropping on.
     */
    nonstd::observer_ptr<view_node_t> check_drop_destination(wf_point input);

};

}
}


#endif /* end of include guard: WF_TILE_PLUGIN_TREE_CONTROLLER_HPP */

