#ifndef WF_MATCHER_PLUGIN_HPP
#define WF_MATCHER_PLUGIN_HPP

#include <view.hpp>
#include <config.hpp>

namespace wf
{
    namespace matcher
    {
        class view_matcher
        {
            public:
            virtual bool matches(wayfire_view view) const = 0;
            virtual ~view_matcher() = default;
        };
#define WF_MATCHER_MATCHES(matcher,view) (matcher ? matcher->matches(view) : false)

        struct match_signal : public signal_data
        {
            std::unique_ptr<view_matcher> result;
            wf_option expression;
        };

#define WF_MATCHER_CREATE_QUERY_SIGNAL "matcher-create-query"

        /* Tries to create a view matcher on the given domain (usually the output
         * of the plugin) with the given expression. May return null */
        std::unique_ptr<view_matcher> get_matcher(wf_signal_provider_t& domain,
            wf_option expression)
        {
            match_signal data;
            data.expression = expression;
            domain.emit_signal(WF_MATCHER_CREATE_QUERY_SIGNAL, &data);
            return std::move(data.result);
        }
    }
}

#endif /* end of include guard: WF_MATCHER_PLUGIN_HPP */
