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
	Session*		_session;
	ServerManager&	_serverManager;
	HttpRequest		_request;
	int				_clientFd;
	bool			_keepAlive;
	bool			_processed = false;
	bool			_newSession;

	HttpMethod getMethod() const;

	Server&	matchServer(const HttpRequest& req, int listenPort);
	void	handleGet(Server& srv, Location& loc);
	void	handlePost(Server& srv, Location& loc);
	void	handleDelete(Server& srv, Location& loc);
	void	handleVisitCounter();

public:
	RequestHandler(ServerManager& manager, const std::string& rawRequest, int clientFd);

	void handle(int listenPort);

	const HttpRequest& getRequest() const;

	bool keepAlive() const;
	void setKeepAlive(bool val);
	void markProcessed();
	bool processed() const;

	void sendResponse(const HttpResponse& res);
	HttpResponse makeErrorResponse(
		const Server& srv,
		int code,
		bool fatal = false
		);
	HttpResponse makeSuccessResponse(
		const Server& srv,
		const std::map<std::string,
		std::string>& vars
		);
};
