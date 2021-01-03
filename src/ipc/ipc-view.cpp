#include "ipc-view.hpp"
#include <wayfire/core.hpp>
#include <wayfire/view.hpp>
#include <wayfire/util/stringify.hpp>
#include <sstream>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/util/stringify.hpp>

using namespace wf::log::detail;

nlohmann::json wf::ipc::handle_view_list()
{
    nlohmann::json j;
    std::vector<uint32_t> views;
    for (auto& v : wf::get_core().get_all_views())
    {
        views.push_back(v->get_id());
    }

    j["status"] = "ok";
    j["views"]  = views;
    return j;
}

void dump_view_data(wayfire_view view, nlohmann::json& j)
{
    j["title"]  = view->get_title();
    j["app-id"] = view->get_app_id();

    std::ostringstream out;
    out << view->get_wm_geometry() << "$" << view->get_bounding_box();
    j["geometry"] = out.str();

    j["tiled"] = view->tiled_edges;
    j["fullscreen"] = view->fullscreen;
    j["minimized"]  = view->minimized;

    if (view->get_wlr_surface())
    {
        auto wlr = view->get_wlr_surface();
        wlr_fbox fbox;
        wlr_surface_get_buffer_source_box(wlr, &fbox);

        j["surface-size"]["width"]  = wlr->current.width;
        j["surface-size"]["height"] = wlr->current.height;

        j["texture-size"]["width"]  = wlr->buffer->texture->width;
        j["texture-size"]["height"] = wlr->buffer->texture->height;

        j["wp-viewporter"]["x"]     = fbox.x;
        j["wp-viewporter"]["y"]     = fbox.y;
        j["wp-viewporter"]["width"] = fbox.width;
        j["wp-viewporter"]["height"] = fbox.height;
    }
}

nlohmann::json wf::ipc::handle_view_info(uint32_t id)
{
    nlohmann::json j;

    std::vector<uint32_t> views;
    for (auto& v : wf::get_core().get_all_views())
    {
        if (v->get_id() == id)
        {
            j["status"] = "ok";
            dump_view_data(v, j);
            return j;
        }
    }

    j["status"]  = "error";
    j["message"] = wf::log::detail::format_concat(
        "View with ID ", id, " does not exist");

    return j;
}
