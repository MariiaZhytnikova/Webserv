#include "StaticGet.hpp"
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

std::optional<HttpResponse> handleDirectoryRequest(
	const HttpRequest& req,
	const Server& srv,
	const Location& loc,
	RequestHandler& handler,
	std::string fullPath)
{
	std::string reqPath = req.getPath();
	fullPath = ensureTrailingSlash(fullPath);

	bool autoindex = loc.getAutoindex()
					 ? true					// location ON
					 : srv.getAutoindex();	// location OFF → use server value

	// 1. Special case: /uploads/
	if (loc.getPath() == "/uploads/") {

		if (!autoindex)		// if autoindex off → forbidden
			return handler.makeErrorResponse(srv, 403);

		DIR* dir = opendir(fullPath.c_str());
		if (!dir)
			return handler.makeErrorResponse(srv, 403);

		std::ostringstream out;
		struct dirent* ent;
		while ((ent = readdir(dir)) != NULL) {
			if (ent->d_name[0] == '.') continue;
			out << ent->d_name << "\n";
		}
		closedir(dir);

		std::string listing = out.str();
		HttpResponse res(200, listing);
		res.setHeader("Content-Type", "text/plain");
		res.setHeader("Content-Length", std::to_string(listing.size()));
		return res;
	}
	// 2. Try index.html
	std::string indexName = loc.getIndex().empty() ? srv.getIndex() : loc.getIndex();
	if (indexName.empty()) indexName = "index.html";

	std::string indexPath = fullPath + indexName;

	struct stat st;
	if (stat(indexPath.c_str(), &st) == 0 && !S_ISDIR(st.st_mode)) {

		std::string body;
		if (!readFile(indexPath, body))
			return handler.makeErrorResponse(srv, 403);

		HttpResponse res(200, body);
		res.setHeader("Content-Type", detectMime(indexPath));
		res.setHeader("Content-Length", std::to_string(body.size()));
		return res;
	}

	// 3. Autoindex ON → show directory listing
	if (autoindex) {
		DIR* dir = opendir(fullPath.c_str());
		if (!dir)
			return handler.makeErrorResponse(srv, 403);

		std::ostringstream html;
		html << "<html><head><meta charset=\"utf-8\">";
		html << "<title>Index of " << reqPath << "</title></head><body>";
		html << "<h1>Index of " << reqPath << "</h1><ul>";

		struct dirent* ent;
		while ((ent = readdir(dir)) != NULL) {
			std::string name = ent->d_name;
			if (name == ".") continue;

			std::string href = ensureTrailingSlash(reqPath) + name;
			html << "<li><a href=\"" << href << "\">" << name << "</a></li>";
		}
		closedir(dir);

		html << "</ul></body></html>";

		std::string body = html.str();
		HttpResponse res(200, body);
		res.setHeader("Content-Type", "text/html");
		res.setHeader("Content-Length", std::to_string(body.size()));
		return res;
	}
	// 4. No index file + autoindex OFF → 403
	return handler.makeErrorResponse(srv, 403);
}

std::optional<HttpResponse> serveGetStatic(const HttpRequest& req, const Server& srv, const Location& loc, RequestHandler& handler) {
	
	std::string reqPath = req.getPath(); // e.g., /css/style.css
	std::string baseRoot = resolveRoot(srv, loc); //./www
	std::string cleanReq = trimLeadingSlash(reqPath);
	std::string fullPath;

	// 1. Handle root URL ("/") - return index.html (localhost:8080/)
	if (reqPath.empty() || reqPath == "/") {
		std::string indexName = loc.getIndex().empty() ? srv.getIndex() : loc.getIndex();
		if (indexName.empty()) indexName = "index.html";
		fullPath = baseRoot + "/" + indexName;
	} else {
		// Normalize path: remove all leading slashes
		fullPath = baseRoot + "/" + cleanReq;
	}
	struct stat st;
	// Check if the requested path exists and get file information
	if (stat(fullPath.c_str(), &st) != 0) {
		Logger::log(ERROR, std::string("404 Not Found") + req.getPath());
		return handler.makeErrorResponse(srv, 404);
	}
	// 2. Redirect directories without trailing slash (browser cache must be cleared to test)
	// This ensures consistent URL handling for directories
	if (S_ISDIR(st.st_mode) && !reqPath.empty() && reqPath.back() != '/') {
		HttpResponse redirect(301, "");
		redirect.setHeader("Location", ensureTrailingSlash(reqPath));
		Logger::log(INFO,
			 "Redirecting (missing slash): "
			  + reqPath + " -> " 
			  + ensureTrailingSlash(reqPath));
		return redirect;
	}
	//3. Directory handling
	   if (S_ISDIR(st.st_mode)) {
		return handleDirectoryRequest(req, srv, loc, handler, fullPath);
	}
	// 4. Serve regular file
	std::string body;
	if (!readFile(fullPath, body)) {
		Logger::log(ERROR, std::string("403 Forbidden") + fullPath);
		return handler.makeErrorResponse(srv, 403);
	}
	// Return successful response with file content
	HttpResponse res(200, body);
	res.setHeader("Content-Type", detectMime(fullPath));
	res.setHeader("Content-Length", std::to_string(body.size()));

	return res;
}
