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
#include <boost/weak_ptr.hpp>
#include <string>

class HttpRequest {

};

class HttpResponse {
public:
    void setStatusCode(int statusCode);
    int getStatusCode() const;
    void setBody(const std::string &body);
    const std::string &getBody() const;
    const std::string &getHeaders() const;
private:
    std::string body;
    std::string headers;
    int status_code;
};

class HttpClientHandler {
public:
    virtual void handle_response(boost::asio::streambuf &response) = 0;
    virtual void handle_error(const boost::system::error_code &err) = 0;
    virtual ~HttpClientHandler() {}
};

class HttpClient {
private:
    boost::asio::io_service &service;
    boost::asio::streambuf request;
    boost::asio::streambuf response;
    boost::asio::ip::tcp::resolver resolver;
    boost::asio::ip::tcp::socket socket;
    boost::weak_ptr<HttpClientHandler> handler;
public:
    HttpClient(boost::asio::io_service &service);
    void get(const std::string& url, boost::weak_ptr<HttpClientHandler> handler);
    void reset();
private:    
    void handle_resolve(const boost::system::error_code& err,
                        boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    void handle_connect(const boost::system::error_code& err);
    void handle_write_request(const boost::system::error_code& err);
    void handle_read_status_line(const boost::system::error_code& err);
    void handle_read_content(const boost::system::error_code& err);
    void handle_read_headers(const boost::system::error_code& err);
    void handle_error(const boost::system::error_code& err);
    void handle_response(boost::asio::streambuf &response);
};

#endif
