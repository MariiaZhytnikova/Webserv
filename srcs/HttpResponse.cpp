#include "HttpResponse.hpp"

HttpResponse::HttpResponse()
	: _version("HTTP/1.1"), _statusCode(200), _statusMessage("OK"), _body("") { }

HttpResponse::HttpResponse(int code, const std::string& body)
	: _statusCode(code), _body(body) {
	_statusMessage = statusMessageForCode(code);
	_headers["Content-Length"] = std::to_string(_body.size());
}

std::string HttpResponse::statusMessageForCode(int code) {
	switch(code) {
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 408: return "Request Timeout";
		case 411: return "Length Required";
		case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
		case 415: return "Unsupported Media Type";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 505: return "HTTP Version Not Supported";
		default:  return "Unknown Status";
	}
}

std::string HttpResponse::serialize() const {
	std::string response = "HTTP/1.1 " + std::to_string(_statusCode) + " " + _statusMessage + "\r\n";

	// normal headers
	for (std::map<std::string,std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
		response += it->first + ": " + it->second + "\r\n";
	}

	// cookie headers
	for (const std::string& cookie : _setCookies) {
		response += "Set-Cookie: " + cookie + "\r\n";
	}
	
	response += "\r\n" + _body;
	return response;
}

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
	_headers[key] = value;
}

void HttpResponse::setBody(const std::string& body) { _body = body; }

const std::string& HttpResponse::getBody() const { return _body; }
int HttpResponse::getStatusCode() const { return _statusCode; }
const std::map<std::string, std::string>& HttpResponse::getHeaders() const { return _headers; }

void HttpResponse::setCookie(const std::string& key,
							 const std::string& value,
							 const std::string& attrs) {
	_setCookies.push_back(key + "=" + value + "; " + attrs);
}