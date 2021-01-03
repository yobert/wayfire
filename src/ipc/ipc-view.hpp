#pragma once

#include <nlohmann/json.hpp>

namespace wf
{
namespace ipc
{
nlohmann::json handle_view_list();
nlohmann::json handle_view_info(uint32_t id);
}
}
