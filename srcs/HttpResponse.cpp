#include "HttpResponse.hpp"

HttpResponse::HttpResponse() : _version("HTTP/1.1"), _statusCode(200), _statusMessage("OK") {}

HttpResponse::HttpResponse(int code, const std::string& body)
    : _statusCode(code), _body(body) {
    _statusMessage = statusMessageForCode(code);
    _headers["Content-Length"] = std::to_string(_body.size());
    _headers["Content-Type"] = "text/html";
}


std::string HttpResponse::statusMessageForCode(int code) {
    switch(code) {
        case 200: return "OK";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "Unknown";
    }
}

std::string HttpResponse::serialize() const {
    std::string response = "HTTP/1.1 " + std::to_string(_statusCode) + " " + _statusMessage + "\r\n";
    for (std::map<std::string,std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        response += it->first + ": " + it->second + "\r\n";
    }
    response += "\r\n" + _body;
    return response;
}
void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
}
