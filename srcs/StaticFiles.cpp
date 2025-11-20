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
	Server serv = srv;
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

	struct stat st;
	if (stat(fullPath.c_str(), &st) != 0) {
		Logger::log(ERROR, std::string("404 Not Found 1") + req.getPath());
		HttpResponse response = handler.makeErrorResponse(serv, 404);
		return handler.makeErrorResponse(serv, 404);
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
			if (!dir) {
				Logger::log(ERROR, std::string("forbidden path: ") + req.getPath());
				return handler.makeErrorResponse(serv, 403);
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
			return handler.makeErrorResponse(serv, 404);
		}
	}

	std::string body;
	if (!readFile(fullPath, body)) {
		Logger::log(ERROR, std::string("403 Forbidden") + fullPath);
		return handler.makeErrorResponse(serv, 403);
	}

	HttpResponse res(200, body);
	res.setHeader("Content-Type", detectMime(fullPath));
	res.setHeader("Content-Length", std::to_string(body.size()));
	return res;
}


// s
std::optional<HttpResponse> servePostStatic(
    const HttpRequest& req,
    const Server& srv,
    const Location& loc,
    RequestHandler& handler)
{
    // ---------- Resolve base root ----------
    std::string baseRoot = loc.getRoot().empty() ? srv.getRoot() : loc.getRoot();
    if (baseRoot.empty()) baseRoot = "./www";
    if (baseRoot.back() == '/') baseRoot.pop_back();

    // ---------- Resolve request path ----------
    std::string reqPath = req.getPath();
    std::string prefix = loc.getPath();
    if (!prefix.empty() && reqPath.find(prefix) == 0)
        reqPath.erase(0, prefix.size());
    if (!reqPath.empty() && reqPath.front() != '/')
        reqPath.insert(reqPath.begin(), '/');

    std::string fullPath = baseRoot + reqPath;

    Logger::log(DEBUG, "PREFIX = " + prefix);
    Logger::log(DEBUG, "reqPath = " + reqPath);
    Logger::log(DEBUG, "baseRoot = " + baseRoot);
    Logger::log(DEBUG, "fullPath = " + fullPath);

    // ---------- Helpers ----------
    auto sanitizeFilename = [](const std::string &n) {
        std::string out;
        for (char c : n)
            if (c > 31 && c != '/' && c != '\\')
                out.push_back(c);
        return out.empty() ? "upload.bin" : out;
    };

    auto decodeUrl = [](const std::string &s) {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '+') out.push_back(' ');
            else if (s[i] == '%' && i + 2 < s.size()) {
                char c = std::stoi(s.substr(i+1,2), nullptr, 16);
                out.push_back(c);
                i += 2;
            } else out.push_back(s[i]);
        }
        return out;
    };

    const std::string &body = req.getBody();
    const std::string contentType = req.getHeader("content-type");

    std::string fileName;
    std::string fileData;

    // ===========================================================
    // 1) MULTIPART/FORM-DATA (Browser)
    // ===========================================================
    if (contentType.find("multipart/form-data") != std::string::npos)
    {
        size_t bpos = contentType.find("boundary=");
        if (bpos != std::string::npos)
        {
            std::string boundary = "--" + contentType.substr(bpos + 9);

            // Find the first part
            size_t pos = body.find(boundary);
            if (pos == std::string::npos)
                return handler.makeErrorResponse(srv, 400);

            pos += boundary.size() + 2; // skip boundary + CRLF

            // Find headers
            size_t hdrEnd = body.find("\r\n\r\n", pos);
            if (hdrEnd == std::string::npos)
                return handler.makeErrorResponse(srv, 400);

            std::string headers = body.substr(pos, hdrEnd - pos);

            // parse filename + name
            {
                size_t fn = headers.find("filename=\"");
                if (fn != std::string::npos) {
                    size_t q1 = headers.find('"', fn + 10);
                    size_t q2 = headers.find('"', q1 + 1);
                    fileName = headers.substr(q1 + 1, q2 - q1 - 1);
                }
            }

            // Content start/end
            size_t contentStart = hdrEnd + 4;
            size_t nextBoundary = body.find(boundary, contentStart);
            if (nextBoundary == std::string::npos)
                nextBoundary = body.size();

            fileData = body.substr(contentStart, nextBoundary - contentStart);

            if (fileData.size() >= 2 &&
                fileData.substr(fileData.size()-2) == "\r\n")
                fileData.erase(fileData.size()-2);
        }
    }

    // ===========================================================
    // 2) X-WWW-FORM-URLENCODED (curl -d)
    // ===========================================================
    else if (contentType.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        // A) RAW data: no '=' in body → treat as raw upload
        if (body.find('=') == std::string::npos)
        {
            fileName = "raw_" + std::to_string(time(NULL)) + ".txt";
            fileData = body;
        }
        else
        {
            // B) Normal key=value form
            std::map<std::string,std::string> params;
            size_t pos = 0;
            while (pos < body.size()) {
                size_t amp = body.find('&', pos);
                std::string pair = body.substr(pos, amp - pos);
                size_t eq = pair.find('=');
                if (eq != std::string::npos)
                    params[decodeUrl(pair.substr(0, eq))] = decodeUrl(pair.substr(eq+1));
                if (amp == std::string::npos) break;
                pos = amp + 1;
            }

            fileName = params.count("filename") ?
                       params["filename"] :
                       "upload_" + std::to_string(time(NULL)) + ".txt";

            fileData = params.count("content") ? params["content"] : "";
        }
    }

    // ===========================================================
    // 3) RAW BODY (curl --data-binary, no content-type)
    // ===========================================================
    else
    {
        fileName = "raw_" + std::to_string(time(NULL)) + ".bin";
        fileData = body;
    }

    // ===========================================================
    // Resolve filename inside fullPath
    // ===========================================================
    fileName = sanitizeFilename(fileName);

    // fullPath might be a folder, e.g. "/uploads/"
    if (fullPath.back() == '/')
        fullPath += fileName;
    else
    {
        // If req path ends WITHOUT extension → treat as directory
        size_t slash = fullPath.find_last_of('/');
        std::string last = fullPath.substr(slash + 1);
        if (last.find('.') == std::string::npos)
            fullPath += "/" + fileName;
    }

    // ===========================================================
    // Ensure directory exists
    // ===========================================================
    size_t slash = fullPath.find_last_of('/');
    std::string dirPath = fullPath.substr(0, slash);

    struct stat st;
    if (stat(dirPath.c_str(), &st) != 0)
    {
        std::string cur;
        size_t p = 0;
        while (p < dirPath.size()) {
            size_t next = dirPath.find('/', p);
            std::string part = dirPath.substr(0, next);
            if (stat(part.c_str(), &st) != 0)
                mkdir(part.c_str(), 0755);
            if (next == std::string::npos) break;
            p = next + 1;
        }
    }

    // ===========================================================
    // Write file
    // ===========================================================
    std::ofstream out(fullPath.c_str(), std::ios::binary);
    if (!out.is_open())
        return handler.makeErrorResponse(srv, 500);

    out.write(fileData.c_str(), fileData.size());
    out.close();

    Logger::log(INFO, "POST: saved " + fullPath);

    // ===========================================================
    // Response
    // ===========================================================
    std::ostringstream html;
    html << "<html><body><h1>File uploaded successfully</h1>"
         << "<p>Saved as: " << fullPath << "</p>"
         << "<p>Size: " << fileData.size() << " bytes</p>"
         << "</body></html>";

    HttpResponse res(201, html.str());
    res.setHeader("Content-Type", "text/html");
    return res;
}

// std::optional<HttpResponse> servePostStatic(
//     const HttpRequest& req,
//     const Server& srv,
//     const Location& loc,
//     RequestHandler& handler)
// {
//     // ---------- BASE PATH ----------
//     std::string baseRoot = loc.getRoot().empty() ? srv.getRoot() : loc.getRoot();
//     if (baseRoot.empty()) baseRoot = "./www";
//     if (!baseRoot.empty() && baseRoot.back() == '/')
//         baseRoot.pop_back();

//     // Ensure upload directory exists
//     struct stat st;
//     if (stat(baseRoot.c_str(), &st) != 0) {
//         if (mkdir(baseRoot.c_str(), 0755) != 0 && errno != EEXIST) {
//             return handler.makeErrorResponse(srv, 500);
//         }
//     }

//     const std::string& body = req.getBody();
//     const std::string contentType = req.getHeader("content-type");

//     std::string fileName;
//     std::string fileData;

//     // -----------------------------------------
//     // 1) MULTIPART/FORM-DATA
//     // -----------------------------------------
//     if (contentType.find("multipart/form-data") != std::string::npos) {
//         size_t bpos = contentType.find("boundary=");
//         if (bpos == std::string::npos)
//             return handler.makeErrorResponse(srv, 400);

//         std::string boundary = "--" + contentType.substr(bpos + 9);

//         size_t fnamePos = body.find("filename=\"");
//         if (fnamePos != std::string::npos) {
//             size_t q1 = body.find('"', fnamePos + 10);
//             size_t q2 = body.find('"', q1 + 1);
//             fileName = body.substr(q1 + 1, q2 - q1 - 1);
//         }

//         size_t headersEnd = body.find("\r\n\r\n");
//         if (headersEnd != std::string::npos) {
//             size_t contentStart = headersEnd + 4;
//             size_t nextB = body.find(boundary, contentStart);
//             fileData = body.substr(contentStart, nextB - contentStart);

//             if (fileData.size() >= 2 &&
//                 fileData.substr(fileData.size() - 2) == "\r\n")
//                 fileData.erase(fileData.size() - 2);
//         }
//     }

//     // ---------------------------------------------------------
//     // 2) X-WWW-FORM-URLENCODED (filename=...&content=...)
//     // ---------------------------------------------------------
//     else if (contentType.find("application/x-www-form-urlencoded") != std::string::npos) {
//         std::map<std::string,std::string> params;
//         size_t p = 0;
//         while (p < body.size()) {
//             size_t amp = body.find('&', p);
//             std::string pair = body.substr(p, amp - p);

//             size_t eq = pair.find('=');
//             if (eq != std::string::npos) {
//                 params[pair.substr(0, eq)] = pair.substr(eq + 1);
//             }

//             if (amp == std::string::npos) break;
//             p = amp + 1;
//         }

//         if (params.count("filename"))
//             fileName = params["filename"];

//         if (params.count("content"))
//             fileData = params["content"];
//     }

//     // -----------------------------------------
//     // 3) RAW BODY (curl --data-binary)
//     // -----------------------------------------
//     else {
//         fileData = body;
//     }

//     // -----------------------------------------
//     // FILENAME SELECTION LOGIC
//     // -----------------------------------------
//     auto chooseFilename = [&](const std::string &reqPath) -> std::string {
//         if (!fileName.empty())
//             return fileName;

//         size_t slash = reqPath.find_last_of('/');
//         if (slash != std::string::npos) {
//             std::string tail = reqPath.substr(slash + 1);
//             if (!tail.empty() && tail.find('.') != std::string::npos)
//                 return tail;
//         }

//         return "upload_" + std::to_string(time(NULL)) + ".bin";
//     };

//     std::string finalName = chooseFilename(req.getPath());
//     std::string fullPath = baseRoot + "/" + finalName;

//     // -----------------------------------------
//     // WRITE FILE
//     // -----------------------------------------
//     std::ofstream out(fullPath.c_str(), std::ios::binary);
//     if (!out.is_open()) {
//         Logger::log(ERROR, "Cannot open for write: " + fullPath);
//         return handler.makeErrorResponse(srv, 500);
//     }

//     out.write(fileData.c_str(), fileData.size());
//     out.close();

//     Logger::log(INFO, "POST: saved " + fullPath);

//     // -----------------------------------------
//     // RESPONSE
//     // -----------------------------------------
//     std::ostringstream html;
//     html << "<html><body><h1>File uploaded</h1>"
//          << "<p>Saved as: " << finalName << "</p>"
//          << "<p>Size: " << fileData.size() << " bytes</p>"
//          << "</body></html>";

//     HttpResponse res(201, html.str());
//     res.setHeader("Content-Type", "text/html");
//     res.setHeader("Content-Length", std::to_string(html.str().size()));
//     return res;
// }



std::optional<HttpResponse> serveDeleteStatic(const HttpRequest& req, const Server& srv, const Location& loc) {
	(void)req; (void)srv; (void)loc;
	return std::nullopt;
}

// For POST/DELETE will be implemented later
// std::optional<HttpResponse> servePostStatic(
// 	const HttpRequest& req,
// 	const Server& srv,
// 	const Location& loc,
// 	RequestHandler& handler)
// {
// 	std::string baseRoot = loc.getRoot().empty() ? srv.getRoot() : loc.getRoot();
// 	if (baseRoot.empty()) baseRoot = "./www";
// 	if (!baseRoot.empty() && baseRoot.back() == '/') baseRoot.pop_back();

// 	// 🔹 Determine target file path
// 	std::string reqPath = req.getPath();
// 	if (reqPath.empty() || reqPath == "/")
// 		reqPath = "/upload.txt"; // fallback name
// 	std::string fullPath = baseRoot + reqPath;

// 	// 🔹 Create directory if needed
// 	struct stat st;
// 	std::string dirPath = fullPath.substr(0, fullPath.find_last_of('/'));
// 	if (stat(dirPath.c_str(), &st) != 0) {
// 		mkdir(dirPath.c_str(), 0755);
// 	}

// 	// 🔹 Write request body to file
// 	const std::string& body = req.getBody();
// 	std::ofstream out(fullPath.c_str(), std::ios::binary);
// 	if (!out.is_open()) {
// 		Logger::log(ERROR, "POST failed: cannot open file " + fullPath);
// 		return handler.makeErrorResponse(srv, 500);
// 	}
// 	out.write(body.c_str(), body.size());
// 	out.close();

// 	// 🔹 Prepare response
// 	std::ostringstream html;
// 	html << "<html><body><h1>File uploaded successfully</h1>"
// 	     << "<p>Saved as: " << fullPath << "</p>"
// 	     << "<p>Size: " << body.size() << " bytes</p>"
// 	     << "</body></html>";

// 	HttpResponse res(201, html.str());
// 	res.setHeader("Content-Type", "text/html");
// 	res.setHeader("Content-Length", std::to_string(html.str().size()));

// 	Logger::log(INFO, "POST: saved " + fullPath);
// 	return res;
// }