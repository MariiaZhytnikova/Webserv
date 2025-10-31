#pragma once

#include <map>
#include <string>
#include <sstream>

/* HTTP Request

Request line: METHOD PATH VERSION
	<request-line>\r\n
	<header1>: <value1>\r\n
	<header2>: <value2>\r\n

	...\r\n
	\r\n
	<body>

Headers: key-value pairs that give meta information about the request
	Host: localhost:8080
	User-Agent: curl/7.81.0
	Accept: 
	Content-Length: 13

Empty line: separates headers from body (\r\n)

Body: optional, usually for POST or PUT requests;
	its length is given by Content-Length header.
*/

class HttpRequest {
	private:
		std::string _method;
		std::string _path;
		std::string _version;
		std::map<std::string, std::string> _headers;
		std::string _body;

		void parseRequestLine(std::istringstream& stream);
		void parseHeaders(std::istringstream& stream);
		void parseBody(std::istringstream& stream);

	public:
		HttpRequest(const std::string& raw);
		std::string getHeader(const std::string& key) const;

	// -------------------- Getters --------------------
		const std::string& getMethod() const;
		const std::string& getPath() const;
		const std::string& getVersion() const;
		const std::map<std::string, std::string>& getHeaders() const;
		const std::string& getBody() const;
};
