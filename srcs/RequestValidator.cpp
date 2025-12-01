#include "RequestValidator.hpp"
#include "Logger.hpp"
#include <cstring>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>

std::string resolveFullPath(
	const HttpRequest& req,
	const Server& srv,
	const Location& loc)
{
	// Compute root
	std::string baseRoot = loc.getRoot().empty()
		? srv.getRoot()
		: loc.getRoot();

	if (baseRoot.empty())
		baseRoot = "./www";

	// normalize: remove trailing slash
	while (!baseRoot.empty() && baseRoot.back() == '/')
		baseRoot.pop_back();

	// Normalize location path
	std::string locPath = loc.getPath();
	if (!locPath.empty() && locPath.front() == '/')
		locPath.erase(0, 1);
	if (!locPath.empty() && locPath.back() == '/')
		locPath.pop_back();

	// Convert request path → local path
	std::string localPath = req.getPath();
	if (!localPath.empty() && localPath.front() == '/')
		localPath.erase(0, 1);

	// Remove location prefix
	if (!locPath.empty() && localPath.rfind(locPath, 0) == 0) {
		localPath.erase(0, locPath.length());
		if (!localPath.empty() && localPath.front() == '/')
			localPath.erase(0, 1);
	}

	// Build full filesystem path 
	std::string full = baseRoot;
	if (!localPath.empty())
		full += "/" + localPath;

	return full;
}

bool RequestValidator::check(RequestHandler& handl, Server& srv, Location& loc) {
	HttpRequest req = handl.getRequest();

	// 🔹 Redirection
	if (loc.hasReturn())
		return handleRedirect(handl, srv, loc);

	// 🔹 Headers check
	if (!checkHeaders(handl, srv)) {
		return false;
	}

	// 🔹 URI check
	if (!checkUri(handl, srv))
		return false;

	// 🔹 Transfer-Encoding: chunked — reject
	if (handl.getRequest().isHeaderValue("transfer-encoding", "chunked")) {
		Logger::log(ERROR, "400 bad request: chunked request body not supported");
		handl.sendResponse(handl.makeErrorResponse(srv, 400));
		return false;
	}

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
	return true;
}

bool RequestValidator::checkHeaders(RequestHandler& handl, Server& srv) {
	(void)srv;
	const std::multimap<std::string, std::string>& headers = handl.getRequest().getHeaders();

	// 🔹 Malformed header
	std::string str = handl.getRequest().getHeader("malformed");
	if (!str.empty()) {
		handl.sendResponse(handl.makeErrorResponse(srv, 400, true));
		Logger::log(ERROR, std::string("400 bad request: malformed header"));
		return false;
	}

	// 🔹 Duplicate Host header
	auto range_host = headers.equal_range("host");
	if (std::distance(range_host.first, range_host.second) != 1) {
		handl.sendResponse(handl.makeErrorResponse(srv, 400, true));
		Logger::log(ERROR, std::string("400 bad request: invalid Host header count"));
		return false;
	}

	// 🔹 Duplicate Content-Type header
	auto range_cont = headers.equal_range("content-type");
	if (std::distance(range_cont.first, range_cont.second) > 1) {
		handl.sendResponse(handl.makeErrorResponse(srv, 400, true));
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
				handl.sendResponse(handl.makeErrorResponse(srv, 400, true));
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

bool RequestValidator::isMethodAllowed(RequestHandler& handl, const std::vector<std::string>& allowed) {
	if (allowed.empty()) return true; // allow all if not specified
	return std::find(allowed.begin(), allowed.end(), handl.getRequest().getMethod()) != allowed.end();
}

bool RequestValidator::checkPost(RequestHandler& handl, Server& srv) {
	std::string length = handl.getRequest().getHeader("content-length");
	std::string type   = handl.getRequest().getHeader("content-type");

	// Content-Length must exist
	if (length.empty()) {
		handl.sendResponse(handl.makeErrorResponse(srv, 411));
		Logger::log(ERROR, "411 content-length is required");
		return false;
	}

	// Parse Content-Length
	size_t bodySize = 0;
	try {
		bodySize = std::stoul(length);
	}
	catch (...) {
		handl.sendResponse(handl.makeErrorResponse(srv, 400, true));
		Logger::log(ERROR, "400 Content-Length is not a valid number");
		return false;
	}

	// Require Content-Type only if body is non-empty
	if (bodySize > 0 && type.empty()) {
		handl.sendResponse(handl.makeErrorResponse(srv, 400, true));
		Logger::log(ERROR, "400 Content-Type required for non-empty POST body");
		return false;
	}

	return true;
}

bool RequestValidator::handleRedirect(
	RequestHandler& handl,
	Server& srv,
	Location& loc)
{
	int code = loc.getReturnCode();
	std::string target = loc.getReturnTarget();
	std::string ua = handl.getRequest().getHeader("user-agent");

	bool isBrowser =
		(!ua.empty() &&
		 (ua.find("Mozilla") != std::string::npos ||
		  ua.find("Chrome")  != std::string::npos ||
		  ua.find("Safari")  != std::string::npos ||
		  ua.find("Firefox") != std::string::npos ||
		  ua.find("Edge")    != std::string::npos));

	// Fancy Browser redirection
	if (isBrowser)
	{
		std::string path = srv.getRoot() + "/errors/301.html";
		std::ifstream file(path.c_str());
		std::string buffer;

		if (file) {
			std::ostringstream ss;
			ss << file.rdbuf();
			buffer = ss.str();

			// Replace placeholder
			std::string target = loc.getReturnTarget();
			size_t pos = buffer.find("{{REDIRECT_URL}}");
			while (pos != std::string::npos) {
				buffer.replace(pos, std::string("{{REDIRECT_URL}}").length(), target);
				pos = buffer.find("{{REDIRECT_URL}}", pos + target.length());
			}


			if (!buffer.empty())
			{
				HttpResponse res(200, buffer);
				res.setHeader("Content-Type", "text/html");
				res.setHeader("Content-Length", std::to_string(buffer.size()));
				handl.sendResponse(res);

				Logger::log(WARNING, "Custom 301 HTML for browser requests");
				return false;
			}
		}
	}

	// Real plain 301
	HttpResponse res(code);
	res.setHeader("Location", target);
	res.setHeader("Content-Length","0");
	res.setHeader("Connection", "close");
	handl.setKeepAlive(false);

	handl.sendResponse(res);

	Logger::log(INFO, "Non-browser → safe real redirect " + target);
	return false;
}
