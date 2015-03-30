#include "rule.h"

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/include/qi.hpp>
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
    (operand, left)
    (operand, right)
)

BOOST_FUSION_ADAPT_STRUCT(
    draw_rule_t,
    (uint32_t, color)
    (operation_t, expr)
)

BOOST_FUSION_ADAPT_STRUCT(
    draw_rules_t,
    (std::vector<draw_rule_t>, rules)
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
struct draw_rules_grammar_t : qi::grammar<Iterator, draw_rules_t(), ascii::space_type> {
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
                   ("team"  , symbol_t::TEAM);

        rules = rule[push_back(at_c<0>(_val), _1)] >>
                *(';' >> rule[push_back(at_c<0>(_val), _1)]);

        rule  %= color >> ":=" >> expression;

        color %= '#' >> qi::hex;

        expression = operation[at_c<1>(_val) = _1] >>
            *(logical_operators[at_c<0>(_val) = _1] > expression[at_c<2>(_val) = _1]);

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
    qi::rule<Iterator, draw_rules_t(), ascii::space_type> rules;
    qi::rule<Iterator, draw_rule_t() , ascii::space_type> rule;
    qi::rule<Iterator, uint32_t()    , ascii::space_type> color;
    qi::rule<Iterator, operation_t() , ascii::space_type> expression;
    qi::rule<Iterator, operation_t() , ascii::space_type> operation;
    qi::rule<Iterator, operand() , ascii::space_type> operand;
    qi::rule<Iterator, std::string() , ascii::space_type> value;

    // symbol maps
    qi::symbols<char, operator_t> operators;
    qi::symbols<char, operator_t> logical_operators;
    qi::symbols<char, symbol_t>   symbols;
};

draw_rules_t wotreplay::parse_draw_rules(const std::string &expr) {
    draw_rules_grammar_t<std::string::const_iterator> grammar;
    draw_rules_t rules;

    std::string::const_iterator iter = expr.begin();
    std::string::const_iterator end = expr.end();
    bool r = qi::phrase_parse(iter, end, grammar, ascii::space, rules);
    std::cout << std::string(iter, end) << std::endl;
    if (r && iter == end)
    {
        std::cout << boost::fusion::tuple_open('[');
        std::cout << boost::fusion::tuple_close(']');
        std::cout << boost::fusion::tuple_delimiter(", ");

        std::cout << "-------------------------\n";
        std::cout << "Parsing succeeded\n";
        std::cout << "\n-------------------------\n";
    }
    else
    {
        std::cout << "-------------------------\n";
        std::cout << "Parsing failed\n";
        std::cout << "-------------------------\n";
    }

    return rules;
}