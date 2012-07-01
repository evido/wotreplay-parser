//
//  HttpClient.cpp
//  wotstats
//
//  Created by Jan Temmerman on 24/05/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include <iostream>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <ctime>

#include "Url.h"
#include "HttpClient.h"


using boost::asio::ip::tcp;
using namespace boost::asio;


class DefaultHttpClientHandler : public HttpClientHandler {
public:
    virtual void handle_error(const boost::system::error_code &err) {
        if (!err) {
            return;
        }
        std::cout << "Error: " << err.message() << "\n";
    }
    
    virtual void handle_response(boost::asio::streambuf &response) {
        std::cout << "Processed response!\n";
    }
    
    virtual ~DefaultHttpClientHandler() {}
};

HttpClient::HttpClient(io_service &service) 
    : service(service),
        socket(service),
        resolver(service)
{
    // empty
}

void HttpClient::handle_write_request(const boost::system::error_code& err) {
    if (err) {
        this->handle_error(err);
        return;
    }
     
    // Read the response status line. The response_ streambuf will
    // automatically grow to accommodate the entire line. The growth may be
    // limited by passing a maximum size to the streambuf constructor.
    boost::asio::async_read_until(socket, response, "\r\n",
                                  boost::bind(&HttpClient::handle_read_status_line, this,
                                              boost::asio::placeholders::error));    
}

void HttpClient::handle_read_status_line(const boost::system::error_code& err)
{
    if (err) {
        this->handle_error(err);
        return;
    }
    
    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);
    if (!response_stream || http_version.substr(0, 5) != "HTTP/")
    {
        std::cout << "Invalid response\n";
        return;
    }
    if (status_code != 200)
    {
        std::cout << "Response returned with status code ";
        std::cout << status_code << "\n";
        return;
    }
    
    // Read the response headers, which are terminated by a blank line.
    boost::asio::async_read_until(socket, response, "\r\n\r\n",
                                  boost::bind(&HttpClient::handle_read_headers, this,
                                              boost::asio::placeholders::error));
}

void HttpClient::handle_read_headers(const boost::system::error_code& err)
{
    if (err) {
        this->handle_error(err);
        return;
    }
     
    // Process the response headers.
    std::istream response_stream(&response);
    std::string header;
    while (std::getline(response_stream, header) && header != "\r")
        std::cout << header << "\n";
    std::cout << "\n";
    
    // Write whatever content we already have to output.
    if (response.size() > 0)
        std::cout << &response;
    
    // Start reading remaining data until EOF.
    boost::asio::async_read(socket, response,
                            boost::asio::transfer_at_least(1),
                            boost::bind(&HttpClient::handle_read_content, this,
                                        boost::asio::placeholders::error));    
}

void HttpClient::handle_read_content(const boost::system::error_code& err)
{
    if (err) {
        this->handle_error(err);
        return;
    }
    
    // Write all of the data that has been read so far.
    std::cout << &response;
    
    // Continue reading remaining data until EOF.
    boost::asio::async_read(socket, response,
                            boost::asio::transfer_at_least(1),
                            boost::bind(&HttpClient::handle_read_content, this,
                                        boost::asio::placeholders::error));
}


void HttpClient::handle_connect(const boost::system::error_code& err) {
    if (err) {
        this->handle_error(err);
        return;
    }
    
    // The connection was successful. Send the request.
    boost::asio::async_write(socket, request,
                             boost::bind(&HttpClient::handle_write_request, this,
                                         boost::asio::placeholders::error));
}

void HttpClient::get(const std::string &urlString, boost::weak_ptr<HttpClientHandler> handler) {
    this->handler = handler;
    
    Url url(urlString);
    
    tcp::resolver::query query(url.hostname, url.protocol);
    
    std::ostream request_stream(&request);
    request_stream << "GET " << url.path << " HTTP/1.0\r\n";
    request_stream << "Host: " << url.hostname << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";
    
    resolver.async_resolve(query, 
                           boost::bind(&HttpClient::handle_resolve, this,
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::iterator));
}

void HttpClient::handle_resolve(
                                const boost::system::error_code &err, 
                                tcp::resolver::iterator endpoint_iterator) {
    if (err) {
        this->handle_error(err);
        return;
    }
    
    // Attempt a connection to each endpoint in the list until we
    // successfully establish a connection.
    boost::asio::async_connect(socket, endpoint_iterator,
                               boost::bind(&HttpClient::handle_connect, this,
                                           boost::asio::placeholders::error));
}

void HttpClient::reset() {
    boost::shared_ptr<HttpClientHandler> handler((HttpClientHandler*) nullptr);
    this->handler = handler;
}

void HttpClient::handle_response(boost::asio::streambuf &response ) {
    boost::shared_ptr<HttpClientHandler> handler = this->handler.lock();
    if (handler) {
        handler->handle_response(response);
    }
    this->reset();
}

void HttpClient::handle_error(const boost::system::error_code& err) {
    boost::shared_ptr<HttpClientHandler> handler = this->handler.lock();
    if (handler) {
        handler->handle_error(err);
    }
    this->reset();
}