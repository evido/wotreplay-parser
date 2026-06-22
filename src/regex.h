#ifndef wotreplay_parser_regex_h
#define wotreplay_parser_regex_h

#ifndef USE_BOOST_REGEX
#include <regex>

using std::regex;
using std::regex_search;
using std::smatch;

#else

// fallback to boost, implementation in gcc 4.7 seems incomplete
#include <boost/regex.hpp>
using boost::regex;
using boost::regex_search;
using boost::smatch;

#endif

#endif
