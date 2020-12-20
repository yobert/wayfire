#ifndef DEBUG_HPP
#define DEBUG_HPP

#ifndef WAYFIRE_PLUGIN
    #include "config.h"
#endif

#define nonull(x) ((x) ? (x) : ("nil"))
#include <wayfire/util/log.hpp>

namespace wf
{
/**
 * Print the current stacktrace at runtime.
 *
 * @param fast_mode If fast_mode is true, the stacktrace will be generated
 *   using the fastest possible method. However, this means that not all
 *   information will be printed (for ex., line numbers may be missing).
 */
void print_trace(bool fast_mode);
}

#if __has_include(<nlohmann/json.hpp>)
    #include <nlohmann/json.hpp>
namespace wf
{
/**
 * Publish a message to the JSON IPC, if it is enabled.
 */
void publish_message(const std::string& category, nlohmann::json json);
}

inline void LOG_IPC(const std::string& category, nlohmann::json&& j)
{
    if constexpr (1)
    {
        wf::publish_message(category, j);
    }
}

template<class ObjectType>
inline void LOG_IPC_EV(const std::string& category,
    const std::string& event, const ObjectType& object,
    nlohmann::json&& j = {})
{
    if constexpr (1)
    {
        j["event"]  = event;
        j["object"] = object;
        wf::publish_message(category, std::move(j));
    }
}

#endif

/* ------------------- Miscallaneous helpers for debugging ------------------ */
#include <ostream>
#include <glm/glm.hpp>
#include <wayfire/geometry.hpp>

std::ostream& operator <<(std::ostream& out, const glm::mat4& mat);
wf::pointf_t operator *(const glm::mat4& m, const wf::pointf_t& p);
wf::pointf_t operator *(const glm::mat4& m, const wf::point_t& p);

#endif
