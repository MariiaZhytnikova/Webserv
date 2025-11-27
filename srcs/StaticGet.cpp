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
#include <errno.h>

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
	return "application/octet-stream";
}

std::optional<HttpResponse> serveGetStatic(const HttpRequest& req, const Server& srv, const Location& loc, RequestHandler& handler) {
	
	std::string reqPath = req.getPath(); // e.g., /css/style.css

	 //Select root directory
	std::string baseRoot = loc.getRoot().empty() ? srv.getRoot() : loc.getRoot();

	// Remove trailing slashes from root path
	while (!baseRoot.empty() && baseRoot.back() == '/')
		baseRoot.pop_back();

	// Fallback to default web root if none specified
	if (baseRoot.empty())
		 baseRoot = "./www";

	std::string fullPath = baseRoot;

	// Handle root URL ("/") - serve index file
	if (reqPath.empty() || reqPath == "/") {
		std::string indexName = loc.getIndex().empty() ? srv.getIndex() : loc.getIndex();
		if (indexName.empty()) indexName = "index.html";
		fullPath += "/" + indexName;
	} else {
		// Normalize path: remove all leading slashes
		std::string rp = reqPath;
		while (!rp.empty() && rp.front() == '/')
			rp.erase(0,1);
		fullPath += "/" + rp;
	}
	
	struct stat st;
	 
	// Check if the requested path exists and get file information
	if (stat(fullPath.c_str(), &st) != 0) {
		Logger::log(ERROR, std::string("404 Not Found 1") + req.getPath());
		return handler.makeErrorResponse(srv, 404);
	}
	std::cout<<"??????????"<<fullPath.c_str()<<"\n";
	
	// Redirect directories without trailing slash (browser cache must be cleared to test)
	// This ensures consistent URL handling for directories
	if (S_ISDIR(st.st_mode) && req.getPath().back() != '/') {
		HttpResponse redirect(301, "");
		redirect.setHeader("Location", req.getPath() + "/");
		Logger::log(INFO, "Redirecting (missing slash): " + reqPath + " -> " + reqPath + "/");
		return redirect;
	}
	// Handle directory requests
	if (S_ISDIR(st.st_mode)) {
			// Normalize directory fullPath (ensure trailing slash for consistency)
		if (!fullPath.empty() && fullPath.back() != '/')
			fullPath += '/';

		// Special handling for /uploads/ - return plain text listing
		if (loc.getPath() == "/uploads/" || loc.getPath() == "/uploads") {
			 // Check if directory listing (autoindex) is enabled
			if (!loc.getAutoindex() && !srv.getAutoindex()) {
				return handler.makeErrorResponse(srv, 403);
			}
			DIR* dir = opendir(fullPath.c_str());
			if (!dir) {
				return handler.makeErrorResponse(srv, 403);
			}

			std::ostringstream out;
			struct dirent* ent;
			errno = 0;
			while ((ent = readdir(dir)) != NULL) {
				if (ent->d_name[0] == '.') continue; // Skip hidden files
				out << ent->d_name << "\n";
			}
			// IMPROVED: Check for readdir errors
			// readdir returns NULL both on error and end-of-directory
			// errno distinguishes between them: 0 = end, non-zero = error
			if (errno != 0) {
				Logger::log(ERROR, "Error reading directory: " + fullPath);
				closedir(dir);
				return handler.makeErrorResponse(srv, 500);
			}
			///////////////////////////////////////////////////////
			closedir(dir);

			std::string listing = out.str();
			HttpResponse res(200, listing);
			res.setHeader("Content-Type", "text/plain");
			res.setHeader("Content-Length", std::to_string(listing.size()));
			return res;
		}
		 // Standard directory handling: look for index file first
		std::string indexName = loc.getIndex().empty() ? srv.getIndex() : loc.getIndex();
		if (indexName.empty()) indexName = "index.html";
		std::string indexPath = fullPath;
		if (indexPath.back() != '/') indexPath += "/";
		indexPath += indexName;

		// If index file exists and is a regular file, serve it
		if (stat(indexPath.c_str(), &st) == 0 && !S_ISDIR(st.st_mode)) {
			fullPath = indexPath;
		 // Otherwise, check if autoindex (directory listing) is enabled
		} else if (loc.getAutoindex() || srv.getAutoindex()) {
			DIR* dir = opendir(fullPath.c_str());
			if (!dir) {
				Logger::log(ERROR, std::string("forbidden path: ") + req.getPath());
				return handler.makeErrorResponse(srv, 403);
			}
			std::ostringstream html;
			html << "<html><head><meta charset=\"utf-8\"><title>Index of " << req.getPath() << "</title></head><body>";
			html << "<h1>Index of " << req.getPath() << "</h1><ul>";
			struct dirent* ent;
			//////////////////////////////////////
			 errno = 0; // Reset errno before readdir loop
			 //////////////////////////////////////
			while ((ent = readdir(dir)) != NULL) {
				std::string name = ent->d_name;
				if (name == ".") continue; // Skip current directory entry

				// Build href for the link
				std::string href = req.getPath();
				if (href.back() != '/') href += "/";
				href += name;
				html << "<li><a href=\"" << href << "\">" << name << "</a></li>";
			}
			            // IMPROVED: Check for readdir errors
            if (errno != 0) {
                Logger::log(ERROR, "Error reading directory: " + fullPath);
                closedir(dir);
                return handler.makeErrorResponse(srv, 500);
            }
			//////////////////////////////////
			closedir(dir);
			html << "</ul></body></html>";
			std::string body = html.str();
			HttpResponse res(200, body);
			res.setHeader("Content-Type", "text/html");
			res.setHeader("Content-Length", std::to_string(body.size()));
			return res;
		 // No index file found and autoindex is disabled
		} else {
			Logger::log(ERROR, std::string("404 Not Found 2") + fullPath);
			return handler.makeErrorResponse(srv, 404);
		}
	}
	// Serve regular file
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
