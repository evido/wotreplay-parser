#include "logger.h"

#include <iostream>

using namespace wotreplay;

namespace wotreplay {
    logger_t logger(std::cout);
}

logger_t::logger_t(std::ostream &os)
    : os(os) {
    // emtpy
}

void logger_t::set_log_level(log_level_t level) {
    this->level = level;
}

void logger_t::write(log_level_t level, const std::string& message) {
    if (level <= this->level) {
        os << message;
    }
}

log_level_t logger_t::get_log_level() const {
    return this->level;
}

