#include "RequestHandler.hpp"
#include "Session.hpp"
#include "Logger.hpp"
#include "CgiHandler.hpp"
#include "StaticFiles.hpp"
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

	try {
		std::string sessionId = _request.getCookies("session_id");
	
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
		if (!preCheckRequest(srv, loc))
			return;

		// 🔹 Detect CGI <- Janeeeeeeee!!!!!
		std::string path = _request.getPath();
		std::string ext = getFileExtension(path);

		// If extension matches a CGI handler in this location
		if (loc.getCgiExtensions().count(ext)) {
			CgiHandler cgi(_request);
			HttpResponse res = cgi.execute(srv.getRoot() + path);
			sendResponse(res);
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

bool RequestHandler::preCheckRequest(Server& srv, Location& loc) {

	// 🔹 Headers check
	if (!checkHeaders(srv))
		return false;

	// 🔹 Body size check
	if (_request.getBody().size() > srv.getClientMaxBodySize()) {
		sendResponse(makeErrorResponse(srv, 413));
		Logger::log(ERROR, std::string("413 payload Too Large"));
		return false;
	}

	// 🔹 Redirection (return directive)
	if (loc.hasReturn()) {
		HttpResponse res(loc.getReturnCode());
		res.setHeader("Location", loc.getReturnTarget());
		sendResponse(res);
		Logger::log(ERROR, std::string("redirect to ") + loc.getReturnTarget());
		// error page for 301 with redirection in 5 sec
		return false;
	}

	// 🔹 Version (only HTTP/1.1 allowed)
	if (_request.getVersion() != "HTTP/1.1") {
		sendResponse(makeErrorResponse(srv, 505));
		Logger::log(ERROR, std::string("505 HTTP version not supported: ") + _request.getVersion());
		return false;
	}

	// 🔹 Unknown methods
	HttpMethod method = stringToMethod(_request.getMethod());
	if (method == METHOD_UNKNOWN) {
		sendResponse(makeErrorResponse(srv, 501));
		Logger::log(ERROR, std::string("501 method not implemented: ") + _request.getMethod());
		return false;
	}

	// 🔹 Content-Length method for POST
	std::string length = _request.getHeader("content-length");
	std::string type = _request.getHeader("content-type");
	if (method == METHOD_POST)
		if (length.empty()) {
		sendResponse(makeErrorResponse(srv, 411));
		Logger::log(ERROR, std::string("411 content-length is required"));
		return false;
		if (type.empty()) {
			sendResponse(makeErrorResponse(srv, 400));
			Logger::log(ERROR, std::string("400 bad request: invalid Content-type header count"));
			return false;
		}
		size_t bodySize = 0;
		try {
			bodySize = std::stoul(length);
		} catch (const std::exception&) {
			sendResponse(makeErrorResponse(srv, 400));
			Logger::log(ERROR, "400 Content-Length is not a valid number");
			return false;
		}
	}

	// 🔹 Allowed methods
	if (!isMethodAllowed(loc.getMethods())) {
		HttpResponse res = makeErrorResponse(srv, 405);
		std::string allowHeader;
		for (size_t i = 0; i < loc.getMethods().size(); ++i) {
			if (i) allowHeader += ", ";
			allowHeader += loc.getMethods()[i];
		}
		res.setHeader("Allow", allowHeader);
		Logger::log(ERROR, std::string("405 method not allowed: ") + allowHeader);
		sendResponse(res);
		return false;
	}

	// 🔹 URI length check
	std::string uri = _request.getPath();
	if (uri.length() > MAX_URI_LENGTH) {
		sendResponse(makeErrorResponse(srv, 414));
		Logger::log(ERROR, std::string("414 URI too long: ") + std::to_string(uri.length()));
		return false;
	}

	// 🔹 URI encoding errors
	for (size_t i = 0; i < uri.size(); ++i) {
		if (uri[i] == '%') {
			// Must have two characters after %
			if (i + 2 >= uri.size()) return false;

			// Check both characters are hex digits
			char c1 = uri[i + 1];
			char c2 = uri[i + 2];
			auto isHex = [](char c) {
				return (c >= '0' && c <= '9') ||
					(c >= 'A' && c <= 'F') ||
					(c >= 'a' && c <= 'f');
			};
			if (!isHex(c1) || !isHex(c2)){
				sendResponse(makeErrorResponse(srv, 400));
				Logger::log(ERROR, std::string("400 URI encoding error: ") + uri);
				return false;
			}
			i += 2; // skip the two hex digits
		}
	}

	// 🔹 URI traversal protection
	if (uri.find("..") != std::string::npos) {
		sendResponse(makeErrorResponse(srv, 403));
		Logger::log(ERROR, std::string("403 fobidden path: ") + uri);
		return false;
	}
	return true;
}

bool RequestHandler::checkHeaders(Server& srv) {

	// 🔹 Malformed header
	std::string str = _request.getHeader("malformed");
	if (!str.empty()) {
		sendResponse(makeErrorResponse(srv, 400));
		Logger::log(ERROR, std::string("400 bad request: malformed header"));
		return false;
	}

	// 🔹 Duplicate Host header
	auto range_host = _request.getHeaders().equal_range("host");
	if (std::distance(range_host.first, range_host.second) != 1) {
		sendResponse(makeErrorResponse(srv, 400));
		Logger::log(ERROR, std::string("400 bad request: invalid Host header count"));
		return false;
	}

	// 🔹 Duplicate Content-Type header
	auto range_cont = _request.getHeaders().equal_range("content-type");
	if (std::distance(range_cont.first, range_cont.second) > 1) {
		sendResponse(makeErrorResponse(srv, 400));
		Logger::log(ERROR, std::string("400 bad request: invalid Content-type header count"));
		return false;
	}

	return true;
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

bool RequestHandler::isMethodAllowed(const std::vector<std::string>& allowed) const {
	if (allowed.empty()) return true; // allow all if not specified
	return std::find(allowed.begin(), allowed.end(), _request.getMethod()) != allowed.end();
}

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
	Logger::log(DEBUG, std::string("bytes sent: ") + std::to_string(totalSent));
	return true;
}

void RequestHandler::sendResponse(const HttpResponse& other) {

	HttpResponse res = other;

	if (_keepAlive == false) {
		Logger::log(INFO, "connection closed by client request fd=" + std::to_string(_clientFd));
		res.setHeader("Connection", "close");
	}

	std::string serialized = res.serialize();
	Logger::log(DEBUG, std::string("response size: ") + std::to_string(serialized.size()));

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
	(void)loc;
	(void)srv;
	// TODO: implement file upload or form handling
	std::string body = "<h1>POST received</h1>";
	HttpResponse res(200, body);
	res.setHeader("Content-Type", "text/html");
	sendResponse(res);
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



