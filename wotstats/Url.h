//
//  Url.h
//  wotstats
//
//  Created by Jan Temmerman on 24/05/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef wotstats_Url_h
#define wotstats_Url_h

#include <string>

struct Url {
    Url(const std::string &url);
    
    std::string protocol;
    std::string hostname;
    std::string path;
};

#endif
