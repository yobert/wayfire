#include "tree-controller.hpp"
#include "../common/preview-indication.hpp"

#include <algorithm>
#include <debug.hpp>
#include <core.hpp>

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
    std::unique_ptr<tree_node_t>& uroot, wf_point grab)
    : root(uroot)
{
    this->grabbed_view = find_view_at(root, grab);
    this->current_input = grab;
}

move_view_controller_t::~move_view_controller_t()
{
    if (this->preview)
        this->preview->set_target_geometry({0, 0}, 0.0, true);
}

nonstd::observer_ptr<view_node_t>
move_view_controller_t::check_drop_destination(wf_point input)
{
    auto dropped_at = find_view_at(this->root, this->current_input);
    if (!dropped_at || dropped_at == this->grabbed_view)
        return nullptr;

    return dropped_at;
}

void move_view_controller_t::ensure_preview(wf_point start,
    wf::output_t *output)
{
    if (this->preview)
        return;

    auto view = std::make_unique<wf::preview_indication_view_t>(output, start);

    this->preview = {view};
    wf::get_core().add_view(std::move(view));
}

void move_view_controller_t::input_motion(wf_point input)
{
    this->current_input = input;

    auto view = check_drop_destination(input);
    if (!view)
    {
        /* No view, no preview */
        if (this->preview)
            preview->set_target_geometry(input, 0.0);

        return;
    }

    auto split = calculate_insert_type(view, input);

    ensure_preview(input, view->view->get_output());
    this->preview->set_target_geometry(
        calculate_split_preview(view, split), 1.0);
}

/**
 * Find the index of the view in its parent list
 */
static int find_idx(nonstd::observer_ptr<tree_node_t> view)
{
    auto& children = view->parent->children;
    auto it = std::find_if(children.begin(), children.end(),
        [=] (auto& node) { return node.get() == view.get(); });

    return it - children.begin();
}

void move_view_controller_t::input_released()
{
    auto dropped_at = find_view_at(this->root, this->current_input);
    if (!dropped_at)
        return;

    auto split = calculate_insert_type(dropped_at, current_input);
    if (split == INSERT_NONE)
        return;

    auto split_type = (split == INSERT_LEFT || split == INSERT_RIGHT) ?
        SPLIT_VERTICAL : SPLIT_HORIZONTAL;

    if (dropped_at->parent->get_split_direction() == split_type)
    {
        /* We can simply add the dragged view as a sibling of the target view */
        auto view = grabbed_view->parent->remove_child(grabbed_view);

        int idx = find_idx(dropped_at);
        if (split == INSERT_RIGHT || split == INSERT_BELOW)
            ++idx;

        dropped_at->parent->add_child(std::move(view), idx);
    } else
    {
        /* Case 2: we need a new split just for the dropped on and the dragged
         * views */
        auto new_split = std::make_unique<split_node_t> (split_type);
        /* The size will be autodetermined by the tree structure, but we set
         * some valid size here to avoid UB */
        new_split->set_geometry(dropped_at->geometry);

        /* Find the position of the dropped view and its parent */
        int idx = find_idx(dropped_at);
        auto dropped_parent = dropped_at->parent;

        log_info("create new split %d", idx);

        /* Remove both views */
        auto dropped_view = dropped_at->parent->remove_child(dropped_at);
        auto dragged_view = grabbed_view->parent->remove_child(grabbed_view);

        if (split == INSERT_ABOVE || split == INSERT_LEFT)
        {
            new_split->add_child(std::move(dragged_view));
            new_split->add_child(std::move(dropped_view));
        } else
        {
            new_split->add_child(std::move(dragged_view));
            new_split->add_child(std::move(dropped_view));
        }

        /* Put them in place */
        dropped_parent->add_child(std::move(new_split), idx);
    }

    /* Clean up tree structure */
    flatten_tree(this->root);
}


}
}
