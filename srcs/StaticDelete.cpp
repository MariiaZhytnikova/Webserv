#include "StaticDelete.hpp"
#include "RequestHandler.hpp"
#include "Logger.hpp"
#include "utils.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <errno.h>


std::optional<HttpResponse> serveDeleteStatic(
	const HttpRequest& req,
	const Server& srv,
	const Location& loc,
	RequestHandler& handler)
{
	// 🔹 1. Decode path
	std::string uri = urlDecode(req.getPath());   // /uploads/logo (1).png
	// Logger::log(DEBUG, "RAW PATH = " + req.getPath());
	// Logger::log(DEBUG, "DECODED = " + uri);

	// 🔹2. Normalize locPath to ALWAYS end with /
	std::string locPath = loc.getPath();
	if (locPath.empty())
		return std::nullopt;
	if (locPath.back() != '/')
		locPath += '/';

	// 🔹 3. Check prefix and strip it
	if (uri.find(locPath) != 0) {
		Logger::log(DEBUG, "PREFIX NOT MATCH");
		return std::nullopt;
	}
	uri.erase(0, locPath.size());      // "logo (1).png"
	Logger::log(DEBUG, "AFTER PREFIX REMOVE = " + uri);

	// 🔹 4 Build filesystem path
	std::string baseRoot = loc.getRoot().empty() ? srv.getRoot() : loc.getRoot();
	if (baseRoot.empty()) baseRoot = "./www";
	// remove trailing slash
	while (!baseRoot.empty() && baseRoot.back() == '/')
		baseRoot.pop_back();
	std::string fullPath = baseRoot + locPath + uri;
	Logger::log(DEBUG, "FULLPATH = " + fullPath);

	// 🔹 5. Make sure file exists
	struct stat st;
	if (stat(fullPath.c_str(), &st) != 0)
		return handler.makeErrorResponse(srv, 404);
	if (S_ISDIR(st.st_mode))
		return handler.makeErrorResponse(srv, 403);

	// 🔹6. Delete
	if (std::remove(fullPath.c_str()) != 0) {
		Logger::log(ERROR, "DELETE failed: " + fullPath);
		return handler.makeErrorResponse(srv, 500);
	}
	// Success
	return HttpResponse(204, "");
}