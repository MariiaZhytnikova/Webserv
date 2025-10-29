#pragma once

#include <string>
#include <map>

class HttpResponse {
public:
	int statusCode;
	std::string reason;
	std::map<std::string, std::string> headers;
	std::string body;

	std::string toString() const;
};
