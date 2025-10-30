#pragma once

#include "ServerManager.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

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
	ServerManager& _serverManager; // access to servers, locations
	HttpRequest _request;
	int _clientFd;

	HttpMethod getMethod() const;
	bool isMethodAllowed(const std::vector<std::string>& allowed) const;
	void sendResponse(const HttpResponse& res);

	Server& matchServer(const HttpRequest& req, int listenPort);
	void handleGet(Server& srv, Location& loc);
	void handlePost(Server& srv, Location& loc);
	void handleDelete(Server& srv, Location& loc);
	HttpResponse makeErrorResponse(Server& srv, int code);

public:
	RequestHandler(ServerManager& manager, const std::string& rawRequest, int clientFd);

	void handle(int listenPort);
};
