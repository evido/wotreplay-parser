#include "game.h"
#include "packet.h"

#include <boost/variant.hpp>

#include <memory>
#include <string>

namespace wotreplay {
    enum operator_t {
        NOT_EQUAL,
        EQUAL,
        GREATER_THAN_OR_EQUAL,
        GREATER_THAN,
        LESS_THAN_OR_EQUAL,
        LESS_THAN,
        AND,
        OR
    };

    enum symbol_t {
        PLAYER,
        CLOCK,
        TEAM
    };

    struct operation_t;
    struct nil {};

    typedef boost::variant<nil, std::string, symbol_t, boost::recursive_wrapper<operation_t>> operand;

    struct operation_t {
        operator_t op;
        operand left;
        operand right;
    };
    
    struct draw_rule_t {
        uint32_t color;
        operation_t expr;
    };
    
    struct draw_rules_t {
        std::vector<draw_rule_t> rules;
    };

    draw_rules_t parse_draw_rules(const std::string &expr);
    void print(const draw_rules_t& rules);
}

