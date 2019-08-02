#include "tree.hpp"
#include <debug.hpp>

#include <output.hpp>
#include <workspace-manager.hpp>
#include <algorithm>

namespace wf
{
namespace tile
{
void tree_node_t::set_geometry(wf_geometry geometry)
{
    this->geometry = geometry;
}

/* ---------------------- split_node_t implementation ----------------------- */
wf_geometry split_node_t::get_child_geometry(
    int32_t child_pos, int32_t child_size)
{
    wf_geometry child_geometry = this->geometry;
    switch (get_split_direction())
    {
        case SPLIT_HORIZONTAL:
            child_geometry.y += child_pos;
            child_geometry.height = child_size;
            break;

        case SPLIT_VERTICAL:
            child_geometry.x += child_pos;
            child_geometry.width = child_size;
            break;
    }

    return child_geometry;
}

int32_t split_node_t::calculate_splittable(wf_geometry available) const
{
    switch (get_split_direction())
    {
        case SPLIT_HORIZONTAL:
            return available.height;
        case SPLIT_VERTICAL:
            return available.width;
    }
}

int32_t split_node_t::calculate_splittable() const
{
    return calculate_splittable(this->geometry);
}

void split_node_t::recalculate_children(wf_geometry available)
{
    if (this->children.empty())
        return;

    double old_child_sum = 0.0;
    for (auto& child : this->children)
        old_child_sum += calculate_splittable(child->geometry);

    int32_t total_splittable = calculate_splittable(available);

    /* Sum of children sizes up to now */
    double up_to_now = 0.0;

    auto progress = [=] (double current) {
        return (current / old_child_sum) * total_splittable;
    };

    /* For each child, assign its percentage of the whole. */
    for (auto& child : this->children)
    {
        /* Calculate child_start/end every time using the percentage from the
         * beginning. This way we avoid rounding errors causing empty spaces */
        int32_t child_start = progress(up_to_now);
        up_to_now += calculate_splittable(child->geometry);
        int32_t child_end = progress(up_to_now);

        /* Set new size */
        int32_t child_size = child_end - child_start;
        child->set_geometry(get_child_geometry(child_start, child_size));
    }
}

void split_node_t::add_child(std::unique_ptr<tree_node_t> child)
{
    int num_children = this->children.size() + 1;

    /* Round up */
    int size_new_child =
        (calculate_splittable() + num_children - 1) / num_children;

    /* Resize older children */
    int size_others = calculate_splittable() - size_new_child;
    recalculate_children(get_child_geometry(0, size_others));

    /* Finally, set the size of the new child, and add it to the list */
    child->set_geometry(get_child_geometry(size_others, size_new_child));
    this->children.emplace_back(std::move(child));
}

std::unique_ptr<tree_node_t> split_node_t::remove_child(
    nonstd::observer_ptr<tree_node_t> child)
{
    /* Remove child */
    std::unique_ptr<tree_node_t> result;
    auto it = this->children.begin();

    while (it != this->children.end())
    {
        if (it->get() == child.get())
        {
            result = std::move(*it);
            it = this->children.erase(it);
        }

        it = std::next(it);
    }

    /* Remaining children have the full geometry */
    recalculate_children(this->geometry);
    return result;
}

void split_node_t::try_flatten()
{
    if (this->children.size() >= 2)
    {
        for (auto& child : this->children)
        {
            auto split_child = dynamic_cast<split_node_t*> (child.get());
            if (split_child)
                split_child->try_flatten();
        }

        return;
    }

    if (this->children.empty())
    {
        /* Only the root node can be empty */
        assert(!parent);
        return;
    }

    auto child = dynamic_cast<split_node_t*> (children.front().get());
    if (child)
    {
    }
}

void split_node_t::set_geometry(wf_geometry geometry)
{
    tree_node_t::set_geometry(geometry);
    recalculate_children(geometry);
}

split_direction_t split_node_t::get_split_direction() const
{
    return this->split_direction;
}

split_node_t::split_node_t(split_direction_t dir)
{
    this->split_direction = dir;
    this->geometry = {0, 0, 0, 0};
}

/* -------------------- view_node_t implementation -------------------------- */
struct view_node_custom_data_t : public custom_data_t
{
    nonstd::observer_ptr<view_node_t> ptr;
    view_node_custom_data_t(view_node_t *node)
    {
        ptr = nonstd::make_observer(node);
    }
};

view_node_t::view_node_t(wayfire_view view)
{
    this->view = view;
    view->store_data(std::make_unique<view_node_custom_data_t> (this));
}

view_node_t::~view_node_t()
{
    view->erase_data<view_node_custom_data_t>();
}

void view_node_t::set_geometry(wf_geometry geometry)
{
    tree_node_t::set_geometry(geometry);

    /* Calculate view geometry in coordinates local to the active workspace,
     * because tree coordinates are kept in workspace-agnostic coordinates. */
    auto output = view->get_output();

    auto vp = output->workspace->get_current_workspace();
    auto size = output->get_screen_size();

    auto local_geometry = geometry;
    local_geometry.x -= vp.x * size.width;
    local_geometry.y -= vp.y * size.height;

    view->set_tiled(TILED_EDGES_ALL);
    view->set_geometry(local_geometry);
}

}
}
