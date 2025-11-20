#include "RequestHandler.hpp"
#include "CgiHandler.hpp"
#include "StaticGet.hpp"
#include "StaticPost.hpp"
#include "StaticDelete.hpp"
#include "RequestValidator.hpp"
#include <algorithm>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <fstream>
#include "utils.hpp"
#include <cstring>

RequestHandler::RequestHandler(ServerManager& manager, 
						const std::string& rawRequest, int clientFd)
	: _serverManager(manager),
	_request(rawRequest),
	_clientFd(clientFd),
	_keepAlive(true) {}

void RequestHandler::handle(int listenPort) {
	Server& srv = matchServer(_request, listenPort);
	Logger::log(INFO, "host: " + _request.getHeader("host"));
	try {
		std::string sessionId = _request.returnHeaderValue("cookie", "session_id");

		// 🔹 Close connection if server request it
		if (_request.isHeaderValue("connection", "close"))
			_keepAlive = false;

		// 🔹 If no cookie was sent, make a new one
		if (sessionId.empty()) {
			sessionId = Session::generateSessionId();
			
		}

		Session& session = _serverManager.getSessionManager().getOrCreate(sessionId);
		session.set("last_path", _request.getPath());
		// 🔹 Find matching location
		Location loc = srv.findLocation(_request.getPath());
		// 🔹 Check request
		if (RequestValidator::check(*this, srv,loc) == false)
			return;
		// 🔹 Detect CGI <- Janeeeeeeee!!!!!
		std::string path = _request.getPath();
		std::string ext = getFileExtension(path);

		// If extension matches a CGI handler in this location
		if (loc.getCgiExtensions().count(ext)) {
			std::string interpreter = loc.getCgiExtensions().at(ext);
			CgiHandler cgi(_request);
			HttpResponse res = cgi.execute(srv.getRoot() + path, interpreter);
			sendResponse(res);
			return;
		}

		// 🔹 Dispatch to correct handler
		HttpMethod method = stringToMethod(_request.getMethod());
		switch (method) {
			case METHOD_GET:	handleGet(srv, loc); break;
			case METHOD_POST:	handlePost(srv, loc); break;
			case METHOD_DELETE:	handleDelete(srv, loc); break;
			default:			sendResponse(makeErrorResponse(srv, 400)); break;
		}
	}
	catch (const std::exception& e) {
		Logger::log(ERROR, std::string("500 error handling request: ") + e.what());
		sendResponse(makeErrorResponse(srv, 500));
	}
}

Server& RequestHandler::matchServer(const HttpRequest& req, int listenPort) {
	std::string host = req.getHeader("host");

	// 🔹 Access servers via ServerManager
	const std::vector<Server>& servers = _serverManager.getServers();

	// 🔹 Search for matching host & port
	for (size_t i = 0; i < servers.size(); ++i) {
		const Server& srv = servers[i];
		if (srv.getListenPort() == listenPort) {
			if (std::find(srv.getServerNames().begin(),
						  srv.getServerNames().end(), host) != srv.getServerNames().end()) {
				return _serverManager.getServer(i);
			}
		}
	}
	// 🔹 Default server for this port
	for (size_t i = 0; i < servers.size(); ++i) {
		const Server& srv = servers[i];
		if (srv.getListenPort() == listenPort && srv.isDefault()) {
			return _serverManager.getServer(i);
		}
	}
	// 🔹 Fallback
	return _serverManager.getServer(0);
}

HttpMethod RequestHandler::getMethod() const { return stringToMethod(_request.getMethod()); }
const HttpRequest& RequestHandler::getRequest() const { return _request; }

bool sendAll(int clientFd, const std::string &data) {
	size_t totalSent = 0;
	ssize_t sent = 0;
	size_t dataSize = data.size();

	while (totalSent < dataSize) {
		sent = send(clientFd, data.c_str() + totalSent, dataSize - totalSent, 0);
		if (sent < 0) {
			Logger::log(ERROR, std::string("send() failed: ") + std::strerror(errno));
			return false;
		}
		if (sent == 0) {
			Logger::log(ERROR, "connection closed by client");
			return false;
		}
		totalSent += static_cast<size_t>(sent);
	}
	// Logger::log(TRACE, std::string("bytes sent: ") + std::to_string(totalSent));
	return true;
}

void RequestHandler::sendResponse(const HttpResponse& other) {

	HttpResponse res = other;

	if (_keepAlive == false) {
		// Logger::log(INFO, "connection closed by client request fd=" + std::to_string(_clientFd));
		res.setHeader("Connection", "close");
	}

	std::string serialized = res.serialize();
	// Logger::log(DEBUG, std::string("response size: ") + std::to_string(serialized.size()));

	bool success = sendAll(_clientFd, serialized);
	if (!success)
		_keepAlive = false;

	if (!_keepAlive) {
		close(_clientFd);
		_serverManager.cleanupClient(_clientFd);
	}
}

HttpResponse RequestHandler::makeErrorResponse(const Server& srv, int code) {
	std::string filePath;
	// 🔹 Check if server has custom error page
	auto it = srv.getErrorPages().find(code);
	if (it != srv.getErrorPages().end())
		filePath = srv.getRoot() + "/" + it->second;
	else
	// 🔹 Fallback default error folder
		filePath = srv.getRoot() + "/errors/" + std::to_string(code) + ".html";

	std::ifstream file(filePath.c_str(), std::ios::binary);
	std::ostringstream buffer;
	// 🔹 Check if file exist and not epmty
	if (file.good()) {
		std::ostringstream tmp;
		tmp << file.rdbuf();
		std::string content = tmp.str();

	// 🔹 Check html exists
		if (!content.empty() &&
			(content.find("<html") != std::string::npos ||
			content.find("<body") != std::string::npos)) {
			buffer.str(content);
			Logger::log(INFO, "Custom HTML error page used: " + filePath);
		} else {
			Logger::log(WARNING, "Invalid error page content, using fallback");
		}
	}
	if (buffer.str().empty()) {
	// 🔹 Minimal inline fallback
		Logger::log(INFO, "Fallback error page triggered");
		buffer.str("");
		buffer << "<html><body><h1>" << code << " "
			<< HttpResponse::statusMessageForCode(code)
			<< "</h1></body></html>";
	}
	std::string body = buffer.str();
	HttpResponse res(code, body);
	res.setHeader("Content-Type", "text/html");
	res.setHeader("Content-Length", std::to_string(body.size()));
	return res;
}

// GET, POST, DELETE methods
void RequestHandler::handleGet(Server& srv, Location& loc) {
	if (auto res = serveGetStatic(_request, srv, loc, *this)) {
		sendResponse(*res);
		return;
	}
	// If static handler didn't produce a response, fall back to an error for now.
	sendResponse(makeErrorResponse(srv, 404));
}
void RequestHandler::handlePost(Server& srv, Location& loc) {
	if (auto res = servePostStatic(_request, srv, loc, *this)) {
		sendResponse(*res);
		return;
	}
	// If static handler didn't produce a response, fall back to an error for now.
	sendResponse(makeErrorResponse(srv, 404));
}

void RequestHandler::handleDelete(Server& srv, Location& loc) {
	// TODO: implement deleting file/resource
	(void)loc;
	(void)srv;
	std::string body = "<h1>DELETE received</h1>";
	HttpResponse res(200, body);
	res.setHeader("Content-Type", "text/html");
	sendResponse(res);
}


// CGI / File Handling (if implemented)

// CGI script fails → 502 Bad Gateway

// File not writable → 500 Internal Server Error

// File already exists on POST (if overwrite not allowed) → 409 Conflict

// ADD botton for location /too_large

// add to config 
	// 	location /too_large/ {
	// 	client_max_body_size 1B;
	// }
