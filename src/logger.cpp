#include "logger.h"

#include <iostream>

using namespace wotreplay;

namespace wotreplay {
    logger log(std::cout);
}

logger::logger(std::ostream &os)
    : os(os) {
    // emtpy
}

void logger::set_log_level(log_level level) {
    this->level = level;
}

void logger::write(log_level level, const std::string& message) {
    if (level < this->level) {
        os << message << std::endl;
    }
}

