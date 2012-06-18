//
//  HttpClient.h
//  wotstats
//
//  Created by Jan Temmerman on 24/05/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef wotstats_HttpClient_h
#define wotstats_HttpClient_h

#include <boost/asio.hpp>
#include <string>

class HttpClient {
private:
    boost::asio::io_service &service;
    boost::asio::streambuf request;
    boost::asio::streambuf response;
    boost::asio::ip::tcp::resolver resolver;
    boost::asio::ip::tcp::socket socket;
public:
    HttpClient(boost::asio::io_service &service);
    void get(const std::string& url);
private:    
    void handle_resolve(const boost::system::error_code& err,
                        boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_connect(const boost::system::error_code& err);
    void handle_write_request(const boost::system::error_code& err);
    void handle_read_status_line(const boost::system::error_code& err);
};

#endif
