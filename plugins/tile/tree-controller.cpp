#include "tree-controller.hpp"
#include <algorithm>
#include <debug.hpp>

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
    nonstd::observer_ptr<tree_node_t> root, wf_point input)
{
    if (root->as_view_node())
        return root->as_view_node();

    for (auto& child : root->children)
    {
        if (child->geometry & input)
            return find_view_at({child}, input);
    }

    /* Children probably empty? */
    return nullptr;
}

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
 *
 * @param sensitivity What percentage of the view is "active", i.e the threshold
 *                    for INSERT_NONE
 */
static split_insertion_t calculate_insert_type(
    nonstd::observer_ptr<tree_node_t> node, wf_point input, double sensitivity)
{
    auto window = node->geometry;

    if (!(window & input))
        return INSERT_NONE;

    /*
     * Calculate how much to the left, right, top and bottom of the window
     * our input is, then filter through the sensitivity.
     *
     * In the end, take the edge which is closest to input.
     */
    std::vector<std::pair<double, split_insertion_t>> edges;

    double px = 1.0 * (input.x - window.x) / window.width;
    double py = 1.0 * (input.y - window.y) / window.height;

    edges.push_back({px, INSERT_LEFT});
    edges.push_back({py, INSERT_ABOVE});
    edges.push_back({1.0 - px, INSERT_RIGHT});
    edges.push_back({1.0 - py, INSERT_BELOW});

    /* Remove edges that are too far away */
    auto it = std::remove_if(edges.begin(), edges.end(),
        [sensitivity] (auto pair) {
            return pair.first > sensitivity;
        });
    edges.erase(it, edges.end());

    if (edges.empty())
        return INSERT_NONE;

    /* Return the closest edge */
    return std::min_element(edges.begin(), edges.end())->second;
}

/* By default, 1/3rd of the view can be dropped into */
static constexpr double SPLIT_PREVIEW_PERCENTAGE = 1.0 / 3.0;

/**
 * Calculate the position of the split that needs to be created if a view is
 * dropped at @input over @node
 */
split_insertion_t calculate_insert_type(
    nonstd::observer_ptr<tree_node_t> node, wf_point input)
{
    return calculate_insert_type(node, input, SPLIT_PREVIEW_PERCENTAGE);
}

/**
 * Calculate the bounds of the split preview
 */
wf_geometry calculate_split_preview(nonstd::observer_ptr<tree_node_t> over,
    split_insertion_t split_type)
{
    auto preview = over->geometry;
    switch(split_type)
    {
        case INSERT_RIGHT:
            preview.x += preview.width * (1 - SPLIT_PREVIEW_PERCENTAGE);
            // fallthrough
        case INSERT_LEFT:
            preview.width = preview.width * SPLIT_PREVIEW_PERCENTAGE;
            break;

        case INSERT_BELOW:
            preview.y += preview.height * (1 - SPLIT_PREVIEW_PERCENTAGE);
            // fallthrough
        case INSERT_ABOVE:
            preview.height = preview.height * SPLIT_PREVIEW_PERCENTAGE;
            break;

        default:
            break; // nothing to do
    }

    return preview;
}

/**
 * Insert @to_insert at the indicated position relative to @node
 */
void insert_split(nonstd::observer_ptr<tree_node_t> node,
    std::unique_ptr<tree_node_t> to_insert)
{
    // TODO: stub
}

/* ------------------------ move_view_controller_t -------------------------- */
move_view_controller_t::move_view_controller_t(
    nonstd::observer_ptr<tree_node_t> root)
{
    this->root = root;
}

move_view_controller_t::~move_view_controller_t()
{
    // TODO: actually do operations on the root
}

void move_view_controller_t::input_motion(wf_point input)
{
    auto view = find_view_at(root, input);
    log_info("ctrl: motionn %d,%d. View is %s", input.x, input.y,
        view ? view->view->get_title().c_str() : "null");

    auto split = calculate_insert_type(view, input);
    log_info("Split is %d", split);
}
}
}
