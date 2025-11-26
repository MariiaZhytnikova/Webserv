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
	
	////////////////////////////////////////////////////////////
	//DEBUG
	// std::cout<<"===HEADERS===\n";
	// for (const auto& [key, value] : req.getHeaders()){
	// 	std::cout << key << ": "<< value << "\n";
	// }
	// std::cout<<"===END of HEADERS===\n";
	///////////////////////////////////////////////////////////
	std::string reqPath = req.getPath(); // /css/style.css
	
	// Select root
	std::string baseRoot = loc.getRoot().empty() ? srv.getRoot() : loc.getRoot();
	while (!baseRoot.empty() && baseRoot.back() == '/')
		baseRoot.pop_back();

	if (baseRoot.empty())
		 baseRoot = "./www";
	// std::cout<<"6!!!!!!!!!!!!!!!"<<srv.getRoot()<<", "<<loc.getRoot()<<", "<<baseRoot<<"\n";
	// std::cout<<"7!!!!!!!!!!!!!!!"<<reqPath<<"\n";
	std::string fullPath = baseRoot;
	// If root URL ("/")
	if (reqPath.empty() || reqPath == "/") {
		std::string indexName = loc.getIndex().empty() ? srv.getIndex() : loc.getIndex();
		if (indexName.empty()) indexName = "index.html";
		fullPath += "/" + indexName;
	} else {
		// Normalize path: remove all leading slashes
		std::string rp = reqPath;
		std::cout<<"8!!!!!!!!!!!!!!!"<<reqPath<<"\n";
		while (!rp.empty() && rp.front() == '/')
			rp.erase(0,1);
		std::cout<<"9!!!!!!!!!!!!!!!"<<rp<<"\n";
		fullPath += "/" + rp;
	}
	
	struct stat st;
	
	if (stat(fullPath.c_str(), &st) != 0) {
		Logger::log(ERROR, std::string("404 Not Found 1") + req.getPath());
		HttpResponse response = handler.makeErrorResponse(srv, 404);
		return handler.makeErrorResponse(srv, 404);
	}
	std::cout<<"??????????"<<fullPath.c_str()<<"\n";
	//Redirect directories without trailing slash ( to test it delete browser cache first!!!!)
	if (S_ISDIR(st.st_mode) && req.getPath().back() != '/') {
		HttpResponse redirect(301, "");
		redirect.setHeader("Location", req.getPath() + "/");
		Logger::log(INFO, "Redirecting (missing slash): " + reqPath + " -> " + reqPath + "/");
		return redirect;
	}

	if (S_ISDIR(st.st_mode)) {
			// ALWAYS normalize directory fullPath
	if (!fullPath.empty() && fullPath.back() != '/')
		fullPath += '/';

	// Plain text for /uploads/
	if (loc.getPath() == "/uploads/" || loc.getPath() == "/uploads") {

		if (!loc.getAutoindex() && !srv.getAutoindex()) {
			return handler.makeErrorResponse(srv, 403);
		}
		DIR* dir = opendir(fullPath.c_str());
		if (!dir) {
			return handler.makeErrorResponse(srv, 403);
		}

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
		// Fallback: default HTML autoindex (your existing code)
		std::string indexName = loc.getIndex().empty() ? srv.getIndex() : loc.getIndex();
		if (indexName.empty()) indexName = "index.html";
		std::string indexPath = fullPath;
		if (indexPath.back() != '/') indexPath += "/";
		indexPath += indexName;
		if (stat(indexPath.c_str(), &st) == 0 && !S_ISDIR(st.st_mode)) {
			fullPath = indexPath;
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
			
			Logger::log(ERROR, std::string("404 Not Found 2") + fullPath);
			return handler.makeErrorResponse(srv, 404);
		}
	}

	std::string body;
	if (!readFile(fullPath, body)) {
		Logger::log(ERROR, std::string("403 Forbidden") + fullPath);
		return handler.makeErrorResponse(srv, 403);
	}

	HttpResponse res(200, body);
	res.setHeader("Content-Type", detectMime(fullPath));
	res.setHeader("Content-Length", std::to_string(body.size()));
	return res;
}
