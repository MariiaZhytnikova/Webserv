#include "RequestHandler.hpp"
#include <algorithm>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>

	//if (!isMethodAllowed(loc.getMethods()))


RequestHandler::RequestHandler(ServerManager& manager,
							const std::string& rawRequest,
							int clientFd)
	: _serverManager(manager),
	_request(rawRequest),
	_clientFd(clientFd) {}

void RequestHandler::handle(int listenPort) {
	try {
		// 1️⃣ Match server
		Server& srv = matchServer(_request, listenPort);

		// 2️⃣ Match location
		Location loc = srv.findLocation(_request.getPath());

		// 3️⃣ Check allowed methods
		HttpMethod method = stringToMethod(_request.getMethod());
		std::vector<std::string> allowed = loc.getMethods();

		bool ok = isMethodAllowed(loc.getMethods());
		if (!ok) {
			HttpResponse res(405, "<h1>405 Method Not Allowed</h1>");
			std::string allowHeader;
			for (size_t i = 0; i < allowed.size(); ++i) {
				if (i) allowHeader += ", ";
				allowHeader += allowed[i];
			}
			res.setHeader("Allow", allowHeader);
			send(_clientFd, res.serialize().c_str(), res.serialize().size(), 0);
			close(_clientFd);
			return;
		}

		// 4️⃣ Handle by method
		switch (method) {
			case METHOD_GET:
				handleGet(srv, loc);
				break;
			case METHOD_POST:
				handlePost(srv, loc);
				break;
			case METHOD_DELETE:
				handleDelete(srv, loc);
				break;
			default: {
				HttpResponse res(400, "<h1>400 Bad Request</h1>");
				send(_clientFd, res.serialize().c_str(), res.serialize().size(), 0);
				close(_clientFd);
				return;
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error handling request: " << e.what() << std::endl;
		HttpResponse res(500, "<h1>500 Internal Server Error</h1>");
		send(_clientFd, res.serialize().c_str(), res.serialize().size(), 0);
		close(_clientFd);
	}
}

Server& RequestHandler::matchServer(const HttpRequest& req, int listenPort) {
	std::string host = req.getHeader("Host");

	// Access servers via ServerManager
	const std::vector<Server>& servers = _serverManager.getServers(); // you need a getter

	// Search for matching host & port
	for (size_t i = 0; i < servers.size(); ++i) {
		const Server& srv = servers[i];
		if (srv.getListenPort() == listenPort) {
			if (std::find(srv.getServerNames().begin(),
						  srv.getServerNames().end(), host) != srv.getServerNames().end()) {
				return _serverManager.getServer(i); // or implement getter by index
			}
		}
	}
	// Default server for this port
	for (size_t i = 0; i < servers.size(); ++i) {
		const Server& srv = servers[i];
		if (srv.getListenPort() == listenPort && srv.isDefault()) {
			return _serverManager.getServer(i);
		}
	}
	// Fallback
	return _serverManager.getServer(0);
}


HttpMethod RequestHandler::getMethod() const { return stringToMethod(_request.getMethod()); }

bool RequestHandler::isMethodAllowed(const std::vector<std::string>& allowed) const {
	if (allowed.empty()) return true; // allow all if not specified
	return std::find(allowed.begin(), allowed.end(), _request.getMethod()) != allowed.end();
}

void RequestHandler::sendResponse(const HttpResponse& res) {
	std::string serialized = res.serialize();
	send(_clientFd, serialized.c_str(), serialized.size(), 0);
	close(_clientFd);
}


// Marinaaaaa it is YOURS!!!!

void RequestHandler::handleGet(Server& srv, Location& loc) {
	(void)loc;
	(void)srv;
	// TODO: implement reading files or directory index
	std::string body = "<h1>GET " + _request.getPath() + "</h1>";
	HttpResponse res(200, body);
	sendResponse(res);
}

void RequestHandler::handlePost(Server& srv, Location& loc) {
	(void)loc;
	(void)srv;
	// TODO: implement file upload or form handling
	std::string body = "<h1>POST received</h1>";
	HttpResponse res(200, body);
	sendResponse(res);
}

void RequestHandler::handleDelete(Server& srv, Location& loc) {
	// TODO: implement deleting file/resource
	(void)loc;
	(void)srv;
	std::string body = "<h1>DELETE received</h1>";
	HttpResponse res(200, body);
	sendResponse(res);
}