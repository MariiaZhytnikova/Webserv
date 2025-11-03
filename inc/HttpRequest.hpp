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
		std::string _body;
		std::map<std::string, std::string> _headers;
		std::map<std::string, std::string> _cookies;

		void parseRequestLine(std::istringstream& stream);
		void parseHeaders(std::istringstream& stream);
		void parseBody(std::istringstream& stream);
		void parseCookies(const std::string& cookieStr);

	public:
		HttpRequest(const std::string& raw);
		HttpRequest(const HttpRequest& other) = default;
		HttpRequest& operator=(const HttpRequest& other) = default;
		~HttpRequest() = default;

		std::string getHeader(const std::string& key) const;

	// -------------------- Getters --------------------
		const std::string& getMethod() const;
		const std::string& getPath() const;
		const std::string& getVersion() const;
		const std::map<std::string, std::string>& getHeaders() const;
		const std::string& getBody() const;
		const std::string& getCookies(const std::string& which) const;
};


/*
Client → Server
	Cookie: session=abc123; theme=dark

Server → Client
	Set-Cookie: session=abc123; Path=/; HttpOnly

	If you want to go a bit further (for your own testing or a “bonus” feel):

	Parse Cookie: header in your HttpRequest class → store it as a map<string, string> cookies.

	Add Set-Cookie: header in your HttpResponse when needed.

*/