//
//  Url.cpp
//  wotstats
//
//  Created by Jan Temmerman on 24/05/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include <iostream>
#include <regex>

#include <boost/format.hpp>

#include "Url.h"

Url::Url(const std::string &str) {
    std::cmatch what;
    std::regex_constants::syntax_option_type options = 
    std::regex_constants::extended | std::regex_constants::icase;
    std::regex regex("(([a-zA-z]+)://)?([A-z0-9\\.]+)(/.*)?", options);
    if (std::regex_match(str.c_str(), what, regex)) {
        for (int i = 0; i < what.size(); ++i) std::cout << what[i] << std::endl;
        this->protocol = what[2];
        this->path = what[4];
        this->hostname = what[3];
    } else {
        std::string message = boost::io::str(boost::format("Invalid URL: %s") % str);
        throw std::runtime_error(message);
    }
}