#include "logger.h"

#include <iostream>

using namespace wotreplay;

namespace wotreplay {
    logger_t log(std::cout);
}

logger_t::logger_t(std::ostream &os)
    : os(os) {
    // emtpy
}

void logger_t::set_log_level(log_level_t level) {
    this->level = level;
}

void logger_t::write(log_level_t level, const std::string& message) {
    if (level < this->level) {
        os << message << std::endl;
    }
}

