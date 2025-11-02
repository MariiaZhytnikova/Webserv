#include "RequestHandler.hpp"
#include <algorithm>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <fstream>
#include "parserUtils.hpp"

RequestHandler::RequestHandler(ServerManager& manager, 
						const std::string& rawRequest, int clientFd)
	: _serverManager(manager),
	_request(rawRequest),
	_clientFd(clientFd) {}

void RequestHandler::handle(int listenPort) {
	Server& srv = matchServer(_request, listenPort);

	try {
		// 🔹 Find matching location
		Location loc = srv.findLocation(_request.getPath());
		if (!preCheckRequest(srv, loc))
			return;

		// 🔹 Detect CGI <- Janeeeeeeee!!!!!
		std::string path = _request.getPath();
		std::string ext = getFileExtension(path);

		// If extension matches a CGI handler in this location
		if (loc.getCgiExtensions().count(ext)) {
			// TO DO handleCgi(srv, loc, path ... ????????);
			return;
		}

		// 🔹 Dispatch to correct handler
		HttpMethod method = stringToMethod(_request.getMethod());
		switch (method) {
			case METHOD_GET:     handleGet(srv, loc); break;
			case METHOD_POST:    handlePost(srv, loc); break;
			case METHOD_DELETE:  handleDelete(srv, loc); break;
			default:             sendResponse(makeErrorResponse(srv, 400)); break;
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error handling request: " << e.what() << std::endl;
		sendResponse(makeErrorResponse(srv, 500));
	}
}

bool RequestHandler::preCheckRequest(Server& srv, Location& loc) {
	// 🔹 Body size check
	if (_request.getBody().size() > srv.getClientMaxBodySize()) {
		sendResponse(makeErrorResponse(srv, 413));
		return false;
	}

	// 🔹 Redirection (return directive)
	if (loc.hasReturn()) {
		HttpResponse res(loc.getReturnCode());
		res.setHeader("Location", loc.getReturnTarget());
		sendResponse(res);
		return false;
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
		sendResponse(res);
		return false;
	}

	// --- 🔹 Path traversal protection ---
	if (_request.getPath().find("..") != std::string::npos) {
		sendResponse(makeErrorResponse(srv, 403));
		return false;
	}
	return true;
}

Server& RequestHandler::matchServer(const HttpRequest& req, int listenPort) {
	std::string host = req.getHeader("Host");

	// 🔹 Access servers via ServerManager
	const std::vector<Server>& servers = _serverManager.getServers(); // you need a getter

	// 🔹 Search for matching host & port
	for (size_t i = 0; i < servers.size(); ++i) {
		const Server& srv = servers[i];
		if (srv.getListenPort() == listenPort) {
			if (std::find(srv.getServerNames().begin(),
						  srv.getServerNames().end(), host) != srv.getServerNames().end()) {
				return _serverManager.getServer(i); // or implement getter by index
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

void RequestHandler::sendResponse(const HttpResponse& res) {
	std::string serialized = res.serialize();
	send(_clientFd, serialized.c_str(), serialized.size(), 0);
	close(_clientFd);
}

HttpResponse RequestHandler::makeErrorResponse(Server& srv, int code) {
	std::string filePath;

	// 🔹  Check if server has custom error page
	auto it = srv.getErrorPages().find(code);
	if (it != srv.getErrorPages().end()) {
		filePath = srv.getRoot() + "/" + it->second; // combine root + relative path
	} else {
	// 🔹  Fallback default error folder
		filePath = srv.getRoot() + "/errors/" + std::to_string(code) + ".html";
	}

	std::ifstream file(filePath.c_str());
	std::ostringstream buffer;

	if (file.is_open()) {
		buffer << file.rdbuf();
		file.close();
	} else {
	// 🔹  Minimal inline fallback
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

//////////////////////// MARINA ////////////////////////////////////////////////////////////////
#include <fstream>   // for std::ifstream
#include <sstream>   // for std::ostringstream

/*
This function is responsible for serving files or directories.
It should:

		Build the correct filesystem path using the server’s and location’s 
		root and the request’s path.

		If the path is a directory, check:

		If autoindex is enabled → generate and return an HTML directory listing.

		Otherwise, try to serve the index file (from Location or Server).

		If the file doesn’t exist → return a 404 error.

		If the file exists but is not readable → return a 403 error.

		If the file is valid → read its contents, set the proper Content-Type, and 
		return it with status 200.
*/

void RequestHandler::handleGet(Server& srv, Location& loc) {
	(void)loc;
	(void)srv;
	std::string root = srv.getRoot();         // server root
	std::string index = srv.getIndex();       // usually "index.html"
	std::string fullPath = root + "/" + index;

	std::ifstream file(fullPath.c_str(), std::ios::in | std::ios::binary);
	if (!file) {
		HttpResponse res(404, "<h1>404 Not Found</h1>");
		res.setHeader("Content-Type", "text/html");
		res.setHeader("Connection", "close");
		sendResponse(res);
		return;
	}

	std::cout << "------->" << fullPath << std::endl;

	std::ostringstream buffer;
	buffer << file.rdbuf();
	std::string body = buffer.str();

	HttpResponse res(200, body);
	res.setHeader("Content-Type", "text/html");
	res.setHeader("Connection", "close");
	sendResponse(res);
}

/*
This one handles uploads or form submissions.
It should:

		If upload_path is defined in the Location, save the body of the request as a new file 
		there and respond with status 201 (Created).

		If the path corresponds to a CGI script, execute the CGI program and return its output 
		as the response.

		If neither applies, simply acknowledge the POST request (for example, returning a 
		confirmation page or echoing the data).

		If the body exceeds client_max_body_size, that error should already have been caught 
		before reaching this point.
*/

void RequestHandler::handlePost(Server& srv, Location& loc) {
	(void)loc;
	(void)srv;
	// TODO: implement file upload or form handling
	std::string body = "<h1>POST received</h1>";
	HttpResponse res(200, body);
	res.setHeader("Content-Type", "text/html");
	res.setHeader("Connection", "close");
	sendResponse(res);
}

/*
This function manages resource deletion.
It should:

		Build the full filesystem path for the requested file.

		If the file doesn’t exist → return a 404 error.

		If the file exists but the process lacks permission to delete it → return a 403 error.

		If deletion succeeds → return a 204 (No Content) response.

		(Optionally) Prevent deleting outside of allowed directories (security check).
*/

void RequestHandler::handleDelete(Server& srv, Location& loc) {
	// TODO: implement deleting file/resource
	(void)loc;
	(void)srv;
	std::string body = "<h1>DELETE received</h1>";
	HttpResponse res(200, body);
	res.setHeader("Content-Type", "text/html");
	res.setHeader("Connection", "close");
	sendResponse(res);
}