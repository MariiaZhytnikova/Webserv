#include "RequestValidator.hpp"
#include "Logger.hpp"
#include <cstring>
#include <fstream>
#include <algorithm>

bool RequestValidator::check(RequestHandler& handl, Server& srv, Location& loc) {
	HttpRequest req = handl.getRequest();

	// 🔹 Redirection
	if (loc.hasReturn())
		return handleRedirect(handl, srv, loc);

	// 🔹 Headers check
	if (!checkHeaders(handl, srv))
		return false;

	// 🔹 URI check
	if (!checkUri(handl, srv))
		return false;

	// 🔹 Body size check
	if (handl.getRequest().getBody().size() > srv.getClientMaxBodySize()) {
		handl.sendResponse(handl.makeErrorResponse(srv, 413));
		Logger::log(ERROR, std::string("413 payload Too Large"));
		return false;
	}

	// 🔹 Version (only HTTP/1.1 allowed)
	if (handl.getRequest().getVersion() != "HTTP/1.1") {
		handl.sendResponse(handl.makeErrorResponse(srv, 505));
		Logger::log(ERROR, std::string("505 HTTP version not supported: ") + handl.getRequest().getVersion());
		return false;
	}

	// 🔹 Unknown methods
	HttpMethod method = stringToMethod(handl.getRequest().getMethod());
	if (method == METHOD_UNKNOWN) {
		handl.sendResponse(handl.makeErrorResponse(srv, 501));
		Logger::log(ERROR, std::string("501 method not implemented: ") + handl.getRequest().getMethod());
		return false;
	}

	// 🔹 Allowed methods
	if (!isMethodAllowed(handl, loc.getMethods())) {
		HttpResponse res = handl.makeErrorResponse(srv, 405);
		std::string allowHeader;
		for (size_t i = 0; i < loc.getMethods().size(); ++i) {
			if (i) allowHeader += ", ";
			allowHeader += loc.getMethods()[i];
		}
		res.setHeader("Allow", allowHeader);
		Logger::log(ERROR, std::string("405 method not allowed: ") + allowHeader);
		handl.sendResponse(res);
		return false;
	}

	if (method == METHOD_POST){ 
		if (!checkPost(handl, srv))
			return false;
	}
	//Transfer-Encoding — reject chunked if unsupported → 501.

	return true;
}

bool RequestValidator::checkHeaders(RequestHandler& handl, Server& srv) {
	(void)srv;
	const std::multimap<std::string, std::string>& headers = handl.getRequest().getHeaders();

	// for (auto &pairs : headers)
	// 	Logger::log(TRACE, std::string("from checkHeaders: ") + pairs.first + " : " + pairs.second);

	// 🔹 Malformed header
	std::string str = handl.getRequest().getHeader("malformed");
	if (!str.empty()) {
		handl.sendResponse(handl.makeErrorResponse(srv, 400));
		Logger::log(ERROR, std::string("400 bad request: malformed header"));
		return false;
	}

	// 🔹 Duplicate Host header
	
	for (auto &pairs : headers)
		Logger::log(TRACE, std::string("from checkHeaders: ") + pairs.first + " : " + pairs.second);

	auto range_host = headers.equal_range("host");
	if (std::distance(range_host.first, range_host.second) != 1) {
		handl.sendResponse(handl.makeErrorResponse(srv, 400));
		Logger::log(ERROR, std::string("400 bad request: invalid Host header count"));
		return false;
	}

	// 🔹 Duplicate Content-Type header
	auto range_cont = headers.equal_range("content-type");
	if (std::distance(range_cont.first, range_cont.second) > 1) {
		handl.sendResponse(handl.makeErrorResponse(srv, 400));
		Logger::log(ERROR, std::string("400 bad request: invalid Content-type header count"));
		return false;
	}

	return true;
}

bool RequestValidator::checkUri(RequestHandler& handl, const Server& srv){
	// 🔹 URI length check
	std::string uri = handl.getRequest().getPath();
	if (uri.length() > MAX_URI_LENGTH) {
		handl.sendResponse(handl.makeErrorResponse(srv, 414));
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
				handl.sendResponse(handl.makeErrorResponse(srv, 400));
				Logger::log(ERROR, std::string("400 URI encoding error: ") + uri);
				return false;
			}
			i += 2; // skip the two hex digits
		}
	}

	// 🔹 URI traversal protection
	if (uri.find("..") != std::string::npos) {
		handl.sendResponse(handl.makeErrorResponse(srv, 403));
		Logger::log(ERROR, std::string("403 fobidden path: ") + uri);
		return false;
	}
	return true;
}

bool RequestValidator::handleRedirect(RequestHandler& handl, Server& srv, Location& loc) {
	// 🔹 Redirection (return directive)
	std::string path = srv.getRoot() + "/errors/301.html";
	std::ifstream file(path.c_str());
	std::string buffer;

	if (file) {
		std::ostringstream ss;
		ss << file.rdbuf();
		std::string content = ss.str();

		// check minimal HTML structure
		if (!content.empty() &&
			(content.find("<html") != std::string::npos || content.find("<body") != std::string::npos)) {
			buffer = content;
			Logger::log(INFO, "Custom HTML page used: " + path);
			// replace placeholder {{REDIRECT_URL}}
			std::string target = loc.getReturnTarget();
			size_t pos = buffer.find("{{REDIRECT_URL}}");
			while (pos != std::string::npos) {
				buffer.replace(pos, std::string("{{REDIRECT_URL}}").length(), target);
				pos = buffer.find("{{REDIRECT_URL}}", pos + target.length());
			}
		} else {
			Logger::log(WARNING, "invalid page content, using direct redirect");
		}
	}
	if (buffer.empty()) {
		HttpResponse res(loc.getReturnCode());
		res.setHeader("Location", loc.getReturnTarget());
		handl.sendResponse(res);
		Logger::log(ERROR, std::string("redirect to ") + loc.getReturnTarget());
		return false;
	}

	// build response
	HttpResponse res(loc.getReturnCode(), buffer);
	res.setHeader("Location", loc.getReturnTarget());
	res.setHeader("Content-Type", "text/html");
	res.setHeader("Content-Length", std::to_string(buffer.size()));

	handl.sendResponse(res);
	Logger::log(INFO, "301 redirecting to " + loc.getReturnTarget());
	return false;
}

bool RequestValidator::isMethodAllowed(RequestHandler& handl, const std::vector<std::string>& allowed) {
	if (allowed.empty()) return true; // allow all if not specified
	return std::find(allowed.begin(), allowed.end(), handl.getRequest().getMethod()) != allowed.end();
}

bool RequestValidator::checkPost(RequestHandler& handl, Server& srv) {
	// 🔹 Content-Length method for POST
	std::string length = handl.getRequest().getHeader("content-length");
	std::string type = handl.getRequest().getHeader("content-type");
	if (length.empty()) {
		handl.sendResponse(handl.makeErrorResponse(srv, 411));
		Logger::log(ERROR, "411 content-length is required");
		return false;
	}
	if (type.empty()) {
		handl.sendResponse(handl.makeErrorResponse(srv, 400));
		Logger::log(ERROR, "400 Content-type header missing");
		return false;
	}
	size_t bodySize = 0;
	(void)bodySize;
	try {
		bodySize = std::stoul(length);
	} catch (const std::exception&) {
		handl.sendResponse(handl.makeErrorResponse(srv, 400));
		Logger::log(ERROR, "400 Content-Length is not a valid number");
		return false;
	}
	return true;
}