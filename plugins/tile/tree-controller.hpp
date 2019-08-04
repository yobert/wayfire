#ifndef WF_TILE_PLUGIN_TREE_CONTROLLER_HPP
#define WF_TILE_PLUGIN_TREE_CONTROLLER_HPP

#include "tree.hpp"
#include <config.hpp>

/* Contains functions which are related to manipulating the tiling tree */
namespace wf
{
namespace tile
{
/**
 * Represents the current mode in which the tile plugin is.
 */
class tile_controller_t
{
  public:
    /** The tile controller is destroyed when the action has come to and end,
     * for ex. when the mouse button is released */
    virtual ~tile_controller_t() = default;

    /** Called when the input is moved */
    virtual void input_motion(wf_point input) {}
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
     */
    move_view_controller_t(nonstd::observer_ptr<tree_node_t> root);

    /** Called when the input is released */
    ~move_view_controller_t();

    void input_motion(wf_point input) override;

  protected:
    nonstd::observer_ptr<tree_node_t> root;
};

}
}


#endif /* end of include guard: WF_TILE_PLUGIN_TREE_CONTROLLER_HPP */

