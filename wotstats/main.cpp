//
//  main.cpp
//  wotstats
//
//  Created by Jan Temmerman on 14/05/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include <iostream>
#include <string>

#include <boost/asio.hpp>

#include "HttpClient.h"

int main(int argc, const char * argv[])
{
    
    boost::asio::io_service io_service;
    HttpClient httpClient(io_service);
    httpClient.get("http://localhost/");
    return 0;
}

