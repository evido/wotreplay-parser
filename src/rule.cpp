#include "rule.h"
#include "logger.h"
#include "tank.h"

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_repeat.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <boost/variant/apply_visitor.hpp>

using namespace wotreplay;

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

BOOST_FUSION_ADAPT_STRUCT(
    operation_t,
    (operator_t, op)
    (operand_t, left)
    (operand_t, right)
)

BOOST_FUSION_ADAPT_STRUCT(
    draw_rule_t,
    (uint32_t, color)
    (operation_t, expr)
)

struct error_handler_
{
    template <typename, typename, typename>
    struct result { typedef void type; };

    template <typename Iterator>
    void operator()(
                    qi::info const& what
                    , Iterator err_pos, Iterator last) const
    {
        std::cout
        << "Error! Expecting "
        << what                         // what failed?
        << " here: \""
        << std::string(err_pos, last)   // iterators to error-pos, end
        << "\""
        << std::endl
        ;
    }
};

boost::phoenix::function<error_handler_> const error_handler = error_handler_();

template<typename Iterator>
struct draw_rules_grammar_t : qi::grammar<Iterator, std::vector<draw_rule_t>(), ascii::space_type> {
    draw_rules_grammar_t() : draw_rules_grammar_t::base_type(rules) {
        using qi::char_;
        using namespace qi::labels;
        using boost::phoenix::at_c;
        using boost::phoenix::push_back;

        qi::_2_type _2;
        qi::_3_type _3;
        qi::_4_type _4;

        using qi::on_error;
        using qi::fail;

        // define operator
        operators.add("=" , operator_t::EQUAL)
                     ("!=", operator_t::NOT_EQUAL)
                     (">=", operator_t::GREATER_THAN_OR_EQUAL)
                     (">" , operator_t::GREATER_THAN)
                     ("<=", operator_t::LESS_THAN_OR_EQUAL)
                     ("<" , operator_t::LESS_THAN);

        // define logical operators
        logical_operators.add("and", operator_t::AND)
                             ("or" , operator_t::OR);

        // define symbols
        symbols.add("player", symbol_t::PLAYER)
                   ("clock" , symbol_t::CLOCK)
                   ("team"  , symbol_t::TEAM)
                   ("tank_tier", symbol_t::TANK_TIER)
                   ("tank_class", symbol_t::TANK_CLASS)
                   ("tank_icon", symbol_t::TANK_ICON)
                   ("tank_country", symbol_t::TANK_COUNTRY)
                   ("tank_name", symbol_t::TANK_NAME);

        rules = rule[push_back(_val, _1)] >>
                *(';' >> rule[push_back(_val, _1)]);

        rule  %= color >> ":=" >> expression;

        color  %= '#' > (color8 | color6);
        color6  = qi::uint_parser<uint32_t, 16, 6, 6>()[_val = _1 << 8 | 0xFF];
        color8 %= qi::uint_parser<uint32_t, 16, 8, 8>();

        expression =
            (operation[at_c<1>(_val) = _1] >> logical_operators[at_c<0>(_val) = _1]
                                           >> expression[at_c<2>(_val) = _1]) |
            (operation[at_c<1>(_val) = _1] >> logical_operators[at_c<0>(_val) = _1]
                                           >> operation[at_c<2>(_val) = _1]) |
            (operation[_val = _1]);

        operation = operand  [at_c<1>(_val) = _1] >
                    operators[at_c<0>(_val) = _1] >
                    operand  [at_c<2>(_val) = _1];

        operand %= symbols | value;

        value = '\'' >>  +qi::char_("a-zA-Z0-9\\-_")[_val += _1] >> '\'';

        // Debugging and error handling and reporting support.
        BOOST_SPIRIT_DEBUG_NODES(
                                 (expression)(operation)(operand)(value));

        // Error handling
        on_error<fail>(expression, error_handler(_4, _3, _2));
    }

    // parsing rules
    qi::rule<Iterator, std::vector<draw_rule_t>(), ascii::space_type> rules;
    qi::rule<Iterator, draw_rule_t() , ascii::space_type> rule;
    qi::rule<Iterator, uint32_t()    , ascii::space_type> color;
    qi::rule<Iterator, uint32_t()    , ascii::space_type> color6;
    qi::rule<Iterator, uint32_t()    , ascii::space_type> color8;
    qi::rule<Iterator, operation_t() , ascii::space_type> expression;
    qi::rule<Iterator, operation_t() , ascii::space_type> operation;
    qi::rule<Iterator, operand_t()   , ascii::space_type> operand;
    qi::rule<Iterator, std::string() , ascii::space_type> value;

    // symbol maps
    qi::symbols<char, operator_t> operators;
    qi::symbols<char, operator_t> logical_operators;
    qi::symbols<char, symbol_t>   symbols;
};

std::vector<draw_rule_t> wotreplay::parse_draw_rules(const std::string &expr) {
    draw_rules_grammar_t<std::string::const_iterator> grammar;
    std::vector<draw_rule_t> rules;

    std::string::const_iterator iter = expr.begin();
    std::string::const_iterator end = expr.end();
    bool r = qi::phrase_parse(iter, end, grammar, ascii::space, rules);

    if (r && iter == end)
    {
        logger.writef(log_level_t::info, "Parsing succesfull\n");
        print(rules);
    }
    else
    {
        logger.writef(log_level_t::warning, "Parsing failed, remaining: %1%\n", std::string(iter, end));
    }

    return rules;
}

/** pretty print drawing rules */
class printer_t : public boost::static_visitor<void> {
public:
    /** 
     * contstruct new printer
     * @param rules rules
     */
    printer_t(const std::vector<draw_rule_t> &rules)
        : rules(rules) {}

    /**
     * Print current rules
     */
    void operator()() {
        logger.writef(log_level_t::info, "Number of rules: %1%\n", rules.size());
        for (int i = 0; i < rules.size(); i += 1) {
            pad();
            logger.writef(log_level_t::debug, "Rule #%1%\n", i);
            (*this)(rules[i]);
        }
    }

    /**
     * Print string value
     * @param str string value
     */
    void operator()(std::string str) {
        logger.writef(log_level_t::debug, "'%1%'", str);
    }

    /**
     * Print nil value
     * @param nil nil value
     */
    void operator()(nil_t nil) {
        logger.write(log_level_t::debug, "(nil)");
    }

    /**
     * Print symbol value
     * @param symbol symbol value
     */
    void operator()(symbol_t symbol) {
        switch(symbol) {
            case wotreplay::PLAYER:
                logger.write(log_level_t::debug, "PLAYER");
                break;
            case wotreplay::TEAM:
                logger.write(log_level_t::debug, "TEAM");
                break;
            case wotreplay::CLOCK:
                logger.write(log_level_t::debug, "CLOCK");
                break;
            case wotreplay::TANK_ICON:
                logger.write(log_level_t::debug, "TANK_ICON");
                break;
            case wotreplay::TANK_NAME:
                logger.write(log_level_t::debug, "TANK_NAME");
                break;
            case wotreplay::TANK_COUNTRY:
                logger.write(log_level_t::debug, "TANK_COUNTRY");
                break;
            case wotreplay::TANK_TIER:
                logger.write(log_level_t::debug, "TANK_TIER");
                break;
            case wotreplay::TANK_CLASS:
                logger.write(log_level_t::debug, "TANK_CLASS");
                break;
            default:
                logger.write(log_level_t::debug, "<invalid symbol>");
                break;
        }
    }

    /**
     * Print operator value
     * @param op operator value
     */
    void operator()(operator_t op) {
        switch(op) {
            case wotreplay::AND:
                logger.write(log_level_t::debug, "and");
                break;
            case wotreplay::OR:
                logger.write(log_level_t::debug, "or");
                break;
            case wotreplay::EQUAL:
                logger.write(log_level_t::debug, "=");
                break;
            case wotreplay::NOT_EQUAL:
                logger.write(log_level_t::debug, "!=");
                break;
            case wotreplay::GREATER_THAN:
                logger.write(log_level_t::debug, ">");
                break;
            case wotreplay::GREATER_THAN_OR_EQUAL:
                logger.write(log_level_t::debug, ">=");
                break;
            case wotreplay::LESS_THAN:
                logger.write(log_level_t::debug, "<");
                break;
            case wotreplay::LESS_THAN_OR_EQUAL:
                logger.write(log_level_t::debug, "<=");
                break;
            default:
                break;
        }
    }

    /**
     * Print operation
     * @param operation operation
     */
    void operator()(operation_t operation) {
        logger.write(log_level_t::debug, "(");
        (*this)(operation.op);
        logger.write(log_level_t::debug, ", ");
        boost::apply_visitor(*this, operation.left);
        logger.write(log_level_t::debug, ", ");
        boost::apply_visitor(*this, operation.right);
        logger.write(log_level_t::debug, ")");
    }

    /**
     * Print rule
     * @param rule rule
     */
    void operator()(draw_rule_t &rule) {
        indent++;
        pad();
        logger.writef(log_level_t::debug, "Color: #%1$06x\n", rule.color);
        pad();
        logger.write(log_level_t::debug, "Expression: ");
        (*this)(rule.expr);
        logger.write(log_level_t::debug, "\n");
        indent--;
    }

    void pad() {
        std::string pad(indent * 3, ' ');
        logger.write(log_level_t::debug, pad);
    }

    std::vector<draw_rule_t> rules;
    int indent = 0;
};

void wotreplay::print(const std::vector<draw_rule_t>& rules) {
    printer_t p(rules);
    p();
}


virtual_machine_t::virtual_machine_t(const game_t &game, const std::vector<draw_rule_t> &rules)
    : rules(rules), game(game)
{}

int virtual_machine_t::operator()(const packet_t &packet) {
    if (game.get_team_id(packet.player_id()) == -1)
        return -1;

    this->p = &packet;
    for (int i = 0; i < rules.size(); i += 1) {
        if ((*this)(rules[i])) return i;
    }

    return -1;
}

bool virtual_machine_t::operator()(const draw_rule_t rule) {
    return (*this)(rule.expr) == "true";
}

std::string virtual_machine_t::operator()(nil_t nil) {
    return "nil";
}

std::string virtual_machine_t::operator()(std::string str) {
    return str;
}

static const tank_t get_tank(const game_t &game, const packet_t &p) {
    const player_t player = game.get_player(p.player_id());
    const auto &tanks = get_tanks();
    return tanks.count(player.tank) ? tanks.at(player.tank) : tank_t {};
}

std::string virtual_machine_t::operator()(symbol_t symbol) {
    switch(symbol) {
        case symbol_t::PLAYER:
            return p->has_property(property_t::player_id) ?
                boost::lexical_cast<std::string>(p->player_id()) : "";
        case symbol_t::TEAM:
            return p->has_property(property_t::player_id) ?
                boost::lexical_cast<std::string>(game.get_team_id(p->player_id())) : "";
        case symbol_t::CLOCK:
            return p->has_property(property_t::clock) ?
                boost::lexical_cast<std::string>(p->clock()) : "";
        case symbol_t::TANK_NAME:
            if (!p->has_property(property_t::player_id)) {
                return "";
            }
            return get_tank(game, *p).name;
        case symbol_t::TANK_ICON:
            if (!p->has_property(property_t::player_id)) {
                return "";
            }
            return game.get_player(p->player_id()).tank;
        case symbol_t::TANK_COUNTRY:
            if (!p->has_property(property_t::player_id)) {
                return "";
            }
            return get_tank(game, *p).country_name;
        case symbol_t::TANK_CLASS:
            if (!p->has_property(property_t::player_id)) {
                return "";
            }
            return get_tank(game, *p).class_name;
        case symbol_t::TANK_TIER:
        {
            if (!p->has_property(property_t::player_id)) {
                return "";
            }
            int tier = get_tank(game, *p).tier;
            return boost::lexical_cast<std::string>(tier);
        }
        default:
            return "";
    }
}

std::string virtual_machine_t::operator()(operation_t operation) {
    std::string lhs = boost::apply_visitor(*this, operation.left);
    std::string rhs = boost::apply_visitor(*this, operation.right);

    bool result = false;

    switch(operation.op) {
        case operator_t::EQUAL:
            result = lhs == rhs;
            break;
        case operator_t::NOT_EQUAL:
            result = lhs != rhs;
            break;
        case operator_t::LESS_THAN:
            result = boost::lexical_cast<double>(lhs) < boost::lexical_cast<double>(rhs);
            break;
        case operator_t::GREATER_THAN:
            result = boost::lexical_cast<double>(lhs) > boost::lexical_cast<double>(rhs);
            break;
        case operator_t::LESS_THAN_OR_EQUAL:
            result = boost::lexical_cast<double>(lhs) <= boost::lexical_cast<double>(rhs);
            break;
        case operator_t::GREATER_THAN_OR_EQUAL:
            result = boost::lexical_cast<double>(lhs) >= boost::lexical_cast<double>(rhs);
            break;
        case operator_t::AND:
            result = lhs == "true" && rhs == "true";
            break;
        case operator_t::OR:
            result = lhs == "true" || rhs == "true";
            break;
        default:
            result = false;
    }

    return result ? "true" : "false";
}

