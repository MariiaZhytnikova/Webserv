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

// std::optional<HttpResponse> serveDeleteStatic(
// 	const HttpRequest& req,
// 	const Server& srv,
// 	const Location& loc,
// 	RequestHandler& handler)
// {

// 	// 1️⃣ Получаем путь из HTTP запроса
//     std::string uri = req.getPath();    // /uploads/logo%20(1).png

//     // 2️⃣ Декодируем URL → пробелы, скобки, UTF-8
//     uri = urlDecode(uri);               // /uploads/logo (1).png
// 	Logger::log(DEBUG, "RAW PATH = " + req.getPath());
// 	Logger::log(DEBUG, "DECODED = " + uri);
// 	// -------------------------------
// 	// 1. ROOT
// 	// -------------------------------
// 	std::string baseRoot = loc.getRoot().empty() ? srv.getRoot() : loc.getRoot();
// 	if (baseRoot.empty()) baseRoot = "./www";

// 	// ensure NO trailing slash
// 	while (!baseRoot.empty() && baseRoot.back() == '/')
// 		baseRoot.pop_back();

// 	// -------------------------------
// 	// 2. Extract request path parts
// 	//    req.getPath():   "/uploads/style.css"
// 	//    loc.getPath():   "/uploads/"
// 	// -------------------------------
// 	std::string reqPath = req.getPath();
// 	std::string locPath = loc.getPath();

// 	// ensure locPath ENDS with "/"
// 	if (!locPath.empty() && locPath.back() != '/')
// 		locPath += '/';

// 	// remove prefix "/uploads/"
// 	if (reqPath.find(locPath) == 0)
// 		reqPath.erase(0, locPath.size());   // -> "style.css"
// 	Logger::log(DEBUG, "AFTER PREFIX REMOVE = " + uri);
// 	// -------------------------------
// 	// 3. Build FULL PATH
// 	// -------------------------------
// 	std::string fullPath = baseRoot + locPath + reqPath;

// 	// Logger::log(DEBUG, "DELETE baseRoot = " + baseRoot);
// 	// Logger::log(DEBUG, "DELETE locPath  = " + locPath);
// 	// Logger::log(DEBUG, "DELETE reqPath  = " + reqPath);
// 	// Logger::log(DEBUG, "DELETE fullPath = " + fullPath);

// 	// -------------------------------
// 	// 4. Check file exists
// 	// -------------------------------
// 	struct stat st;
// 	if (stat(fullPath.c_str(), &st) != 0) {
// 		return handler.makeErrorResponse(srv, 404);
// 	}

// 	// directories cannot be deleted
// 	if (S_ISDIR(st.st_mode)) {
// 		return handler.makeErrorResponse(srv, 403);
// 	}

// 	// -------------------------------
// 	// 5. DELETE the file
// 	// -------------------------------
// 	Logger::log(DEBUG, "FULLPATH = " + fullPath);
// 	if (std::remove(fullPath.c_str()) != 0) {
// 		Logger::log(ERROR, "DELETE failed: " + fullPath);
// 		return handler.makeErrorResponse(srv, 500);
// 	}

// 	// -------------------------------
// 	// 6. SUCCESS
// 	// -------------------------------
// 	return HttpResponse(204, "");
// }
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