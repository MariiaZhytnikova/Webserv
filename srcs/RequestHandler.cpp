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
#include <fcntl.h>

RequestHandler::RequestHandler(ServerManager& manager, 
						const std::string& rawRequest, int clientFd)
	: _serverManager(manager),
	_request(rawRequest),
	_clientFd(clientFd),
	_keepAlive(true) {}

void RequestHandler::handle(int listenPort) {
	Server& srv = matchServer(_request, listenPort);
	Logger::log(INFO, "host: " + _request.getHeader("host"));
	_processed = false;
	_keepAlive = true;

	try {
		std::string sessionId = _request.getCookie("session_id");

		_newSession = false;

		// 🔹 Close connection if server request it
		if (_request.isHeaderValue("connection", "close"))
			_keepAlive = false;

		// 🔹 If no cookie was sent, make a new one
		if (sessionId.empty()) {
			sessionId = Session::generateSessionId();
			_newSession = true;
		}
		_session = &_serverManager.getSessionManager().getOrCreate(sessionId);
		_session->touch();
		handleVisitCounter();

		// Logger::log(DEBUG, std::string("Sesson ID: ") + sessionId);

		// 🔹 Find matching location
		Location loc = srv.findLocation(_request.getPath());

		// 🔹 Check request
		if (RequestValidator::check(*this, srv,loc) == false)
			return;
		std::string path = _request.getPath();
		std::string ext = getFileExtension(path);

		// If extension matches a CGI handler in this location
		if (loc.getCgiExtensions().count(ext)) {
			std::string interpreter = loc.getCgiExtensions().at(ext);
			CgiHandler cgi(_request);
			HttpResponse res = cgi.execute(srv.getRoot() + path, interpreter, srv.getRoot());
			sendResponse(res);
			return;
		}

		// 🔹 Dispatch to correct handler
		HttpMethod method = stringToMethod(_request.getMethod());
		switch (method) {
			case METHOD_GET:	handleGet(srv, loc); break;
			case METHOD_POST:	handlePost(srv, loc); break;
			case METHOD_DELETE:	handleDelete(srv, loc); break;
			case METHOD_PUT:	handlePut(srv, loc); break;
			case METHOD_HEAD:	handleHead(srv, loc); break;
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
bool RequestHandler::keepAlive() const { return _keepAlive; }
void RequestHandler::setKeepAlive(bool val) { _keepAlive = val; }
void RequestHandler::markProcessed() { _processed = true; }
bool RequestHandler::processed() const { return _processed; }

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
	if (_newSession) {
		res.setCookie("session_id", _session->getId());
	}

	if (_session) {
		// Only send session-owned cookies
		if (_session->has("visits")) {
			res.setCookie("visits", _session->getSession("visits"), "Path=/");
		}
		// Session ID only if new
		if (_newSession) {
			res.setCookie("session_id", _session->getId(), "Path=/");
		}
	}

	if (_keepAlive == false) {
		// Logger::log(INFO, "connection closed by client request fd=" + std::to_string(_clientFd));
		res.setHeader("Connection", "close");
	}

	std::string serialized = res.serialize();
	// Logger::log(DEBUG, std::string("response size: ") + std::to_string(serialized.size()));

	// Logger::log(DEBUG, std::string("Response ") + serialized);

	bool success = sendAll(_clientFd, serialized);
	if (!success) {
		res.setHeader("Connection", "close");
		_keepAlive = false;
	}
	_processed = true;
	// Logger::log(
	// 	DEBUG,
	// 	"sendResponse: KeepAlive=" + std::string(_keepAlive ? "true" : "false") +
	// 	" processed=" + std::string(_processed ? "true" : "false")
	// );
}

HttpResponse RequestHandler::makeErrorResponse(const Server& srv, int code, bool fatal) {
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
			Logger::log(INFO, "Default HTML error page used: " + filePath);
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
	if (fatal || code == 505) {
		res.setHeader("Connection", "close");
		_keepAlive = false;
	} else {
		res.setHeader("Connection", "keep-alive");
	}
	// Logger::log(
	// 	DEBUG,
	// 	"makeErrorResponse: KeepAlive=" + std::string(_keepAlive ? "true" : "false") +
	// 	" processed=" + std::string(_processed ? "true" : "false")
	// );
	return res;
}

HttpResponse RequestHandler::makeSuccessResponse(
		const Server& srv,
		const std::map<std::string, std::string>& vars)
{
	// Always load a single HTML file — no config
	std::string filePath = srv.getRoot() + "/pages/202.html";

	std::ifstream file(filePath.c_str(), std::ios::binary);
	std::ostringstream buffer;

	// Load file if exists
	if (file.good()) {
		std::ostringstream tmp;
		tmp << file.rdbuf();
		std::string content = tmp.str();

		if (!content.empty() &&
			(content.find("<html") != std::string::npos ||
			 content.find("<body") != std::string::npos))
		{
			buffer.str(content);
			Logger::log(INFO, "Success page used: " + filePath);
		}
		else {
			Logger::log(WARNING, "Invalid success page content, using fallback");
		}
	}
	// Fallback if file missing or invalid
	if (buffer.str().empty()) {
		Logger::log(INFO, "Fallback success page triggered");
		buffer.str("");
		buffer << "<html><body><h1>Success</h1></body></html>";
	}

	// Template replacement: {{key}}
	std::string html = buffer.str();
	for (const auto &p : vars) {
		std::string tag = "{{" + p.first + "}}";
		size_t pos;
		while ((pos = html.find(tag)) != std::string::npos) {
			html.replace(pos, tag.length(), p.second);
		}
	}
	HttpResponse res(200, html);
	res.setHeader("Content-Type", "text/html");
	res.setHeader("Content-Length", std::to_string(html.size()));
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
	if (auto res = serveDeleteStatic(_request, srv, loc, *this)) {
		sendResponse(*res);
		return;
	}

	sendResponse(makeErrorResponse(srv, 404));
}

void RequestHandler::handlePut(const Server& srv, const Location& loc)
{
	(void)srv;
	(void)loc;
	std::string relPath = _request.getPath().substr(loc.getPath().length());
	if (!relPath.empty() && relPath[0] == '/')
		relPath.erase(0, 1);

	std::string filePath = loc.getRoot() + "/" + relPath;

	bool existed = (access(filePath.c_str(), F_OK) == 0);

	int fd = open(filePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		HttpResponse res(403, "Forbidden\n");
		sendResponse(res);
		return;
	}

	const std::string& body = _request.getBody();
	write(fd, body.c_str(), body.size());
	close(fd);

	HttpResponse res;
	if (existed)
		res = HttpResponse(204);    // No Content
	else
		res = HttpResponse(201);    // Created

	sendResponse(res);
}

void RequestHandler::handleHead(const Server& srv, const Location& loc)
{
	// Try serving the file using GET logic
	if (auto res = serveGetStatic(_request, srv, loc, *this))
	{
		// Correct Content-Length based on actual body
		size_t len = res->getBody().size();
		res->setHeader("Content-Length", std::to_string(len));

		// Remove body for HEAD
		res->setBody("");

		// MUST disable keep-alive BEFORE sendResponse()
		_keepAlive = false;
		res->setHeader("Connection", "close");

		sendResponse(*res);
		return;
	}

	// Error version
	HttpResponse err = makeErrorResponse(srv, 404);
	size_t len = err.getBody().size();
	err.setHeader("Content-Length", std::to_string(len));
	err.setBody("");

	_keepAlive = false;
	err.setHeader("Connection", "close");

	sendResponse(err);
}

void RequestHandler::handleVisitCounter() {
	std::string str = _request.getPath();
	Logger::log(DEBUG, "path in handleVisitCounter:" + str);
	if (str == "/") {
		std::string visits = _session->getSession("visits");
		if (visits.empty())
			visits = "0";

		int count = std::atoi(visits.c_str()) + 1;
		_session->set("visits", std::to_string(count));
		Logger::log(DEBUG, "number of visits:" + std::to_string(count));
	}
}
