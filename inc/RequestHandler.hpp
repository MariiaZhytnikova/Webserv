#pragma once

#include "ServerManager.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Session.hpp"
#include "Logger.hpp"

const size_t MAX_URI_LENGTH = 8192;

enum HttpMethod {
	METHOD_GET,
	METHOD_POST,
	METHOD_DELETE,
	METHOD_UNKNOWN
};

inline std::string methodToString(HttpMethod m) {
	switch (m) {
		case METHOD_GET: return "GET";
		case METHOD_POST: return "POST";
		case METHOD_DELETE: return "DELETE";
		default: return "UNKNOWN";
	}
}

inline HttpMethod stringToMethod(const std::string& m) {
	if (m == "GET") return METHOD_GET;
	if (m == "POST") return METHOD_POST;
	if (m == "DELETE") return METHOD_DELETE;
	return METHOD_UNKNOWN;
}

class RequestHandler {
private:
	ServerManager&	_serverManager;
	HttpRequest		_request;
	int				_clientFd;
	bool			_keepAlive;

	HttpMethod getMethod() const;

	Server& matchServer(const HttpRequest& req, int listenPort);
	void handleGet(Server& srv, Location& loc);
	void handlePost(Server& srv, Location& loc);
	void handleDelete(Server& srv, Location& loc);

public:
	RequestHandler(ServerManager& manager, const std::string& rawRequest, int clientFd);

	void handle(int listenPort);

	const HttpRequest& getRequest() const;
	HttpResponse makeErrorResponse(const Server& srv, int code);
	HttpResponse makeSuccessResponse(
		const Server& srv,
		const std::string& name,
		const std::map<std::string, std::string>& vars);
	void sendResponse(const HttpResponse& res);
};


/*
- Host header

Mandatory in HTTP/1.1.

Every request must have: Host: example.com

- Header parsing

Case-insensitive header names.

Multiple headers with same name can appear — handle correctly.

Trailing whitespace should be trimmed.


- Content-Length

If not using chunked transfer, you must send:



*/