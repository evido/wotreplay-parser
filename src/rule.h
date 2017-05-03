#include "game.h"
#include "packet.h"

#include <boost/variant.hpp>

#include <memory>
#include <string>

/** @file */

namespace wotreplay {
    /** operator types */
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

    /** symbol types */
    enum symbol_t {
        PLAYER,
        CLOCK,
        TEAM,
        TANK_ICON,
        TANK_NAME,
        TANK_TIER,
        TANK_CLASS,
        TANK_COUNTRY
    };

    struct operation_t;

    /** null value */
    struct nil_t {};

    typedef boost::variant<nil_t, std::string, symbol_t, boost::recursive_wrapper<operation_t>> operand_t;

    /** operation type */
    struct operation_t {
        operator_t op;
        operand_t left;
        operand_t right;
    };

    /** draw rule */
    struct draw_rule_t {
        /** color to use for matching packets */
        uint32_t color;
        /** condition to match packets against */
        operation_t expr;
    };

    /** 
     * @fn std::vector<draw_rule_t> parse_draw_rules(const std::string &expr)
     * Parse drawing rules from expression
     * @param param expr expression string
     */
    std::vector<draw_rule_t> parse_draw_rules(const std::string &expr);

    /**
     * @fn void print(const std::vector<draw_rule_t>& rules)
     * Print drawing rules
     * @param rules drawing rules
     */
    void print(const std::vector<draw_rule_t>& rules);

    /**
     * evaluate packet against the drawing rules
     */
    class virtual_machine_t {
    public:
        typedef std::string result_type;
        virtual_machine_t(const game_t &game, const std::vector<draw_rule_t> &rules);
        /** 
         * get the index of the first matching rule 
         * @param param packet packet to evaluate against the rules
         */
        int operator()(const packet_t &packet);
        /**
         * evaluate current packet against rule
         * @param rule rule to evaluate
         * @return \true if packet matches rule
         */
        bool operator()(const draw_rule_t rule);
        /**
         * evaluate nil operator
         * @param nil nil
         * @return get nil value
         */
        std::string operator()(nil_t nil);
        /**
         * get symbol value for current packet
         * @param symbol symbol
         * @return get symbol value
         */
        std::string operator()(symbol_t symbol);
        /**
         * get operation value for current packet
         * @param operation operation
         * @return get operation result
         */
        std::string operator()(operation_t operation);
        /**
         * get string value for current packet
         * @param str string
         * @return get string value
         */
        std::string operator()(std::string str);
    private:
        const std::vector<draw_rule_t> &rules;
        const game_t &game;
        packet_t const *p;
    };
}
