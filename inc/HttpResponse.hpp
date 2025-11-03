#pragma once

#include <string>
#include <sstream>
#include <map>

/* HTTP Response

Request line: VERSION CODE STATUS_MSG

	HTTP/1.1 200 OK

Headers: key-value pairs that give meta information about the request
	Content-Type: text/html
	Content-Length: 1234
	Connection: close

Empty line: separates headers from body (\r\n)

Body: actual content of the response — HTML, JSON, image data, etc. _body contains binary data in case of image

	<html><body>Hello, world!</body></html>
*/

class HttpResponse {
private:
	std::string _version;
	int _statusCode;
	std::string _statusMessage;
	std::map<std::string, std::string> _headers;
	std::string _body;

public:
	HttpResponse();
	HttpResponse(int code, const std::string& body = "");
	HttpResponse(const HttpResponse& other) = default;
	HttpResponse& operator=(const HttpResponse& other) = default;
	~HttpResponse() = default;

	void setHeader(const std::string& key, const std::string& value);
	static std::string statusMessageForCode(int code);
	std::string serialize() const;
};
