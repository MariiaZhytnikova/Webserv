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

// Helper: read file into string (binary-safe)
static bool readFile(const std::string& path, std::string& out) {
	std::ifstream f(path.c_str(), std::ios::in | std::ios::binary);
	if (!f) return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return true;
}

static std::string detectMime(const std::string& path) {
	std::string ext = getFileExtension(path);
	if (ext == ".html" || ext == ".htm") return "text/html";
	if (ext == ".css") return "text/css";
	if (ext == ".js") return "application/javascript";
	if (ext == ".json") return "application/json";
	if (ext == ".png") return "image/png";
	if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
	if (ext == ".gif") return "image/gif";
	if (ext == ".txt") return "text/plain";
	if (ext == ".svg") return "image/svg+xml";
	if (ext == ".ico") return "image/x-icon";
	return "application/octet-stream";
}

std::string resolveRoot(const Server& srv, const Location& loc){
	//choose location root if defined; else server root
	std::string root = loc.getRoot().empty()? srv.getRoot() : loc.getRoot();
	//remove trailing slashes
	while(!root.empty() && root.back() == '/')
		root.pop_back();

	//fallback
	if (root.empty())
		root = "./www";
	return root;
}

std::string ensureTrailingSlash(const std::string &s)
{
	if (s.empty()) return "/";

	//check last char of the string
	char last = s[s.size() - 1];

	//if last char is already '/', nothing to change
	if (last == '/') return s;

	//otherwise, append '/' and return new string
	std::string result = s + "/";

	return result;
}

std::string trimLeadingSlash(const std::string &s) {
		// If the string is empty, just return it as is.
	if (s.empty()) {
		return "";
	}
		// We will count how many leading '/' characters the string has.
	size_t index = 0;

	// Move index forward while:
	//  - it is still inside the string
	//  - the current character is '/'
	while (index < s.size() && s[index] == '/') {
		index++;
	}
	// Now 'index' points to the first non-'/' character
	// Example:
	//   s = "///uploads"
	//   index becomes 3
	//
	// We return the substring starting from index to the end.
	return s.substr(index);
}

// 
std::optional<HttpResponse> handleDirectoryRequest(
	const HttpRequest& req,
	const Server& srv,
	const Location& loc,
	RequestHandler& handler,
	std::string fullPath)
{
	std::string reqPath = req.getPath();
	fullPath = ensureTrailingSlash(fullPath);

	// AUTOINDEX FINAL LOGIC (no extra helpers)
	bool autoindex = loc.getAutoindex()
					 ? true          // location ON
					 : srv.getAutoindex();   // location OFF → use server value

	// 1. Special case: /uploads/
	if (loc.getPath() == "/uploads" || loc.getPath() == "/uploads/") {

		if (!autoindex)    // if autoindex off → forbidden
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
		Logger::log(ERROR,
	"AUTOINDEX loc=" + std::string(loc.getAutoindex() ? "ON" : "OFF") +
	" srv=" + std::string(srv.getAutoindex() ? "ON" : "OFF") +
	" path=" + loc.getPath()
	);
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
