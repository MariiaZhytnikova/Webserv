#pragma once

#include <string>
#include <sstream>
#include <map>

class HttpResponse {
private:
	std::string _version;
	int _statusCode;
	std::string _statusMessage;
	std::map<std::string, std::string> _headers;
	std::string _body;

	static std::string statusMessageForCode(int code);

public:
	HttpResponse();
	HttpResponse(int code, const std::string& body = "");

	void setHeader(const std::string& key, const std::string& value);
	std::string serialize() const;
};

/*
Error handling
You can now implement all error responses (404, 405, 500…) 
consistently by just creating HttpResponse(code, body) and optionally setting extra headers.

Body flexibility
For static HTML this works. Later, for files, CGI output, or chunked encoding, 
you’ll extend _body handling.

Optional helper
You might want a sendResponse(int clientFd) helper inside HttpResponse or 
RequestHandler so you don’t have to repeat
*/