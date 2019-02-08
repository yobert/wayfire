#ifndef MATCHER_AST_HPP
#define MATCHER_AST_HPP

#include <string>
#include <memory>
#include <utility>

namespace wf
{
    namespace matcher
    {
        struct view_t
        {
            std::string type;
            std::string title;
            std::string app_id;
        };

        /* A base class for expressions */
        struct expression_t
        {
            virtual bool evaluate(const view_t& view) = 0;
            virtual ~expression_t() = default;
        };

        using parse_result_t = std::pair<std::unique_ptr<expression_t>, std::string>;
        parse_result_t parse_expression(std::string expression);
    }
}

#endif /* end of include guard: MATCHER_AST_HPP */
