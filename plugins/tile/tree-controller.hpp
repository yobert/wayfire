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
 * Calculate which view node is at the given position
 *
 * Returns null if no view nodes are present.
 */
nonstd::observer_ptr<view_node_t> find_view_at(
    nonstd::observer_ptr<tree_node_t> root, wf_point input);

enum split_insertion_t
{
    /** Insert is invalid */
    INSERT_NONE  = 0,
    /** Insert above the view */
    INSERT_ABOVE = 1,
    /** Insert below the view */
    INSERT_BELOW = 2,
    /** Insert to the left of the view */
    INSERT_LEFT  = 3,
    /** Insert to the right of the view */
    INSERT_RIGHT = 4,
};

/**
 * Calculate the position of the split that needs to be created if a view is
 * dropped at @input over @node
 */
split_insertion_t calculate_insert_type(
    nonstd::observer_ptr<tree_node_t> node, wf_point input);

/**
 * Calculate the bounds of the split preview
 */
wf_geometry calculate_split_preview(nonstd::observer_ptr<tree_node_t> over,
    split_insertion_t split_type);

/**
 * Insert @to_insert at the indicated position relative to @node
 */
void insert_split(nonstd::observer_ptr<tree_node_t> node,
    std::unique_ptr<tree_node_t> to_insert);

}
}


#endif /* end of include guard: WF_TILE_PLUGIN_TREE_CONTROLLER_HPP */

