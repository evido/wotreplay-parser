//
//  HttpClient.cpp
//  wotstats
//
//  Created by Jan Temmerman on 24/05/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include <iostream>

#include <boost/bind.hpp>
#include <tbb/tick_count.h>

#include "Url.h"
#include "HttpClient.h"


using boost::asio::ip::tcp;
using namespace boost::asio;

HttpClient::HttpClient(io_service &service) 
    : service(service),
        socket(service),
        resolver(service)
{
    // empty

    tbb::tick_count start = tbb::tick_count::now();
    tbb::tick_count stop = tbb::tick_count::now();
    tbb::tick_count::interval_t elapsed_time = (stop - start);
    std::cout << elapsed_time.seconds();
}

void HttpClient::handle_write_request(const boost::system::error_code& err) {
    if (!err)
    {
        // Read the response status line. The response_ streambuf will
        // automatically grow to accommodate the entire line. The growth may be
        // limited by passing a maximum size to the streambuf constructor.
        boost::asio::async_read_until(socket, response, "\r\n",
                                      boost::bind(&HttpClient::handle_read_status_line, this,
                                                  boost::asio::placeholders::error));
    }
    else
    {
        std::cout << "Error: " << err.message() << "\n";
    }
}

void HttpClient::handle_read_status_line(const boost::system::error_code& err) {
    
}

void HttpClient::handle_connect(const boost::system::error_code& err) {
    if (!err)
    {
        // The connection was successful. Send the request.
        boost::asio::async_write(socket, request,
                                 boost::bind(&HttpClient::handle_write_request, this,
                                             boost::asio::placeholders::error));
    }
    else
    {
        std::cout << "Error: " << err.message() << "\n";
    }
}

void HttpClient::get(const std::string &urlString) {
    Url url(urlString);
    
    tcp::resolver::query query(url.hostname, url.protocol);
    
    std::ostream request_stream(&request);
    request_stream << "GET " << url.path << " HTTP/1.0\r\n";
    request_stream << "Host: " << url.hostname << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";
    
    resolver.async_resolve(
                           query, 
                           boost::bind(&HttpClient::handle_resolve, this,
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::iterator));
}

void HttpClient::handle_resolve(
                                const boost::system::error_code &err, 
                                tcp::resolver::iterator endpoint_iterator) {
    if (!err)
    {
        // Attempt a connection to each endpoint in the list until we
        // successfully establish a connection.
        boost::asio::async_connect(socket, endpoint_iterator,
                                   boost::bind(&HttpClient::handle_connect, this,
                                               boost::asio::placeholders::error));
    }
    else
    {
        std::cout << "Error: " << err.message() << "\n";
    }
}