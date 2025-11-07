#include "StaticFiles.hpp"
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
	std::string reqPath = req.getPath();
	std::string baseRoot = loc.getRoot().empty() ? srv.getRoot() : loc.getRoot();
	if (baseRoot.empty()) baseRoot = "./www";
	if (!baseRoot.empty() && baseRoot.back() == '/') baseRoot.pop_back();

	std::string fullPath = baseRoot;
	if (reqPath.empty() || reqPath == "/") {
		std::string indexName = loc.getIndex().empty() ? srv.getIndex() : loc.getIndex();
		if (indexName.empty()) indexName = "index.html";
		fullPath += "/" + indexName;
	} else {
		std::string rp = reqPath;
		if (rp.front() == '/') rp.erase(0,1);
		fullPath += "/" + rp;
		if (!fullPath.empty() && fullPath.back() == '/') {
			std::string indexName = loc.getIndex().empty() ? srv.getIndex() : loc.getIndex();
			if (indexName.empty()) indexName = "index.html";
			fullPath += indexName;
		}
	}

	// cheap traversal guard; consider realpath for production
	if (fullPath.find("..") != std::string::npos) {
		Logger::log(ERROR, std::string("forbidden path: ") + req.getPath());
		return HttpResponse(403, "<h1>403 Forbidden</h1>");
	}

	struct stat st;
	if (stat(fullPath.c_str(), &st) != 0) {
		return HttpResponse(404, "<h1>404 Not Found</h1>");
	}

	if (S_ISDIR(st.st_mode)) {
		std::string indexName = loc.getIndex().empty() ? srv.getIndex() : loc.getIndex();
		if (indexName.empty()) indexName = "index.html";
		std::string indexPath = fullPath;
		if (indexPath.back() != '/') indexPath += "/";
		indexPath += indexName;
		if (stat(indexPath.c_str(), &st) == 0 && !S_ISDIR(st.st_mode)) {
			fullPath = indexPath;
		} else if (loc.getAutoindex() || srv.getAutoindex()) {
			DIR* dir = opendir(fullPath.c_str());
			if (!dir) return HttpResponse(403, "<h1>403 Forbidden</h1>");
			std::ostringstream html;
			html << "<html><head><meta charset=\"utf-8\"><title>Index of " << req.getPath() << "</title></head><body>";
			html << "<h1>Index of " << req.getPath() << "</h1><ul>";
			struct dirent* ent;
			while ((ent = readdir(dir)) != NULL) {
				std::string name = ent->d_name;
				if (name == ".") continue;
				std::string href = req.getPath();
				if (href.back() != '/') href += "/";
				href += name;
				html << "<li><a href=\"" << href << "\">" << name << "</a></li>";
			}
			closedir(dir);
			html << "</ul></body></html>";
			std::string body = html.str();
			HttpResponse res(200, body);
			res.setHeader("Content-Type", "text/html");
			res.setHeader("Content-Length", std::to_string(body.size()));
			return res;
		} else {
			//return HttpResponse(404, "<h1>404 Not Found</h1>");
			Server serv = srv;
			Logger::log(ERROR, std::string("404 Not Found") + fullPath);
			return handler.makeErrorResponse(serv, 404);
		}
	}

	std::string body;
	if (!readFile(fullPath, body)) {
		return HttpResponse(403, "<h1>403 Forbidden</h1>");
	}

	HttpResponse res(200, body);
	res.setHeader("Content-Type", detectMime(fullPath));
	res.setHeader("Content-Length", std::to_string(body.size()));
	return res;
}

// For POST/DELETE will be implemented later
std::optional<HttpResponse> servePostStatic(const HttpRequest& req, const Server& srv, const Location& loc) {
	(void)req; (void)srv; (void)loc;
	return std::nullopt;
}

std::optional<HttpResponse> serveDeleteStatic(const HttpRequest& req, const Server& srv, const Location& loc) {
	(void)req; (void)srv; (void)loc;
	return std::nullopt;
}