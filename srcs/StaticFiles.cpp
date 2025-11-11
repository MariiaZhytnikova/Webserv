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
std::optional<HttpResponse> servePostStatic(
	const HttpRequest& req,
	const Server& srv,
	const Location& loc,
	RequestHandler& handler)
{
	// 🔹 Resolve base root
	std::string baseRoot = loc.getRoot().empty() ? srv.getRoot() : loc.getRoot();
	if (baseRoot.empty())
		baseRoot = "./www";
	if (!baseRoot.empty() && baseRoot.back() == '/')
		baseRoot.pop_back();

	// 🔹 Get request path and strip location prefix (e.g. /upload/)
	std::string reqPath = req.getPath();
	std::string prefix = loc.getPath();
	if (!prefix.empty() && reqPath.find(prefix) == 0)
		reqPath.erase(0, prefix.size());

	if (!reqPath.empty() && reqPath.front() != '/')
		reqPath.insert(reqPath.begin(), '/');
	std::cout<<"PREFIX - "<< prefix<<std::endl;
	std::cout<<"reqPAth - "<< reqPath<<std::endl;
	std::cout<<"BaseRoot - "<< baseRoot<<std::endl;
	// 🔹 Build final target path
	std::string fullPath = baseRoot + reqPath;
	std::cout<<"full path - "<< fullPath<<std::endl;
	// Helper: percent-decode for x-www-form-urlencoded
	auto urlDecode = [](const std::string &s) {
		std::string out;
		out.reserve(s.size());
		for (size_t i = 0; i < s.size(); ++i) {
			if (s[i] == '+') out.push_back(' ');
			else if (s[i] == '%' && i + 2 < s.size()) {
				int hi = std::isxdigit(s[i+1]) ? std::stoi(s.substr(i+1,1), nullptr, 16) : 0;
				int lo = std::isxdigit(s[i+2]) ? std::stoi(s.substr(i+2,1), nullptr, 16) : 0;
				char c = (char)((hi << 4) + lo);
				out.push_back(c);
				i += 2;
			} else out.push_back(s[i]);
		}
		return out;
	};

	// If application/x-www-form-urlencoded, parse fields (filename, content)
	std::string cType = req.getHeader("content-type");
	std::string finalContent;
	if (cType.find("application/x-www-form-urlencoded") != std::string::npos) {
		std::string body = req.getBody();
		std::map<std::string,std::string> params;
		size_t pos = 0;
		while (pos < body.size()) {
			size_t amp = body.find('&', pos);
			std::string pair = body.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
			size_t eq = pair.find('=');
			if (eq != std::string::npos) {
				std::string k = urlDecode(pair.substr(0, eq));
				std::string v = urlDecode(pair.substr(eq+1));
				params[k] = v;
			}
			if (amp == std::string::npos) break;
			pos = amp + 1;
		}
		// Expecting fields: filename and content
		std::string fname = params.count("filename") ? params["filename"] : std::string();
		std::string content = params.count("content") ? params["content"] : std::string();
		if (fname.empty()) {
			Logger::log(ERROR, "POST form upload missing filename");
			return handler.makeErrorResponse(srv, 400);
		}
		// ensure fullPath points to that filename inside the baseRoot
		if (!fullPath.empty() && fullPath.back() == '/') fullPath += fname;
		else fullPath += "/" + fname;
		finalContent = content;
	}

	// 🔹 Write request body to file
	const std::string& body = req.getBody();

	// If this is a multipart/form-data upload from a browser, extract the file part
	std::string contentType = req.getHeader("content-type");
	std::string fileName;
	std::string fileData;
	if (contentType.find("multipart/form-data") != std::string::npos) {
		// find boundary
		std::string needle = "boundary=";
		size_t bpos = contentType.find(needle);
		if (bpos != std::string::npos) {
			std::string boundary = contentType.substr(bpos + needle.size());
			// boundary in body is prefixed with "--"
			std::string fullBoundary = "--" + boundary;

			// parse parts
			size_t pos = 0;
			while (true) {
				size_t partStart = body.find(fullBoundary, pos);
				if (partStart == std::string::npos) break;
				partStart += fullBoundary.size();
				// skip optional CRLF
				if (body.substr(partStart, 2) == "\r\n") partStart += 2;

				// headers end
				size_t headersEnd = body.find("\r\n\r\n", partStart);
				if (headersEnd == std::string::npos) break;
				std::string headers = body.substr(partStart, headersEnd - partStart);

				// parse Content-Disposition to find name and filename
				std::string disposition;
				std::istringstream hs(headers);
				std::string line;
				bool isFilePart = false;
				while (std::getline(hs, line)) {
					// remove trailing \r if present
					if (!line.empty() && line.back() == '\r') line.pop_back();
					std::string low = line;
					// lowercase compare not necessary here, look for Content-Disposition
					if (line.find("Content-Disposition:") != std::string::npos) {
						disposition = line;
						// look for filename="..."
						size_t fn = line.find("filename=");
						if (fn != std::string::npos) {
							// extract between quotes
							size_t q1 = line.find('"', fn);
							if (q1 != std::string::npos) {
								size_t q2 = line.find('"', q1 + 1);
								if (q2 != std::string::npos && q2 > q1)
									fileName = line.substr(q1 + 1, q2 - q1 - 1);
							}
						}
						// look for name="file" or other field name
						size_t nm = line.find("name=");
						if (nm != std::string::npos) {
							size_t q1 = line.find('"', nm);
							size_t q2 = line.find('"', q1 + 1);
							if (q1 != std::string::npos && q2 != std::string::npos) {
								std::string fieldName = line.substr(q1 + 1, q2 - q1 - 1);
								if (fieldName == "file") isFilePart = true;
							}
						}
					}
				}

				size_t contentStart = headersEnd + 4; // skip \r\n\r\n
				size_t nextBoundary = body.find(fullBoundary, contentStart);
				if (nextBoundary == std::string::npos) break;
				size_t contentEnd = nextBoundary;
				// strip trailing CRLF if present
				if (contentEnd >= 2 && body.substr(contentEnd - 2, 2) == "\r\n") contentEnd -= 2;

				if (isFilePart) {
					fileData = body.substr(contentStart, contentEnd - contentStart);
					break; // take first file part
				}

				pos = nextBoundary;
			}
		}
	}

	// If multipart and we extracted a filename, ensure fullPath ends with filename
	if (!fileName.empty()) {
		// if fullPath points to a directory (ends with '/'), append filename
		if (!fullPath.empty() && fullPath.back() == '/')
			fullPath += fileName;
		else {
			// if reqPath didn't include a filename, append '/'+filename
			// check if reqPath ends with a known extension-like segment
			size_t lastSlash = fullPath.find_last_of('/');
			std::string lastSegment = (lastSlash == std::string::npos) ? fullPath : fullPath.substr(lastSlash + 1);
			if (lastSegment.find('.') == std::string::npos) {
				// treat as directory -> append '/' + filename
				fullPath += "/" + fileName;
			}
		}
	}

	// If no filename was found and fullPath resolves to a directory (or points to base root),
	// generate a safe unique filename instead of trying to open the directory for writing.
	auto sanitizeFilename = [](const std::string &n) {
		std::string out;
		for (size_t i = 0; i < n.size(); ++i) {
			char c = n[i];
			if (c == '/' || c == '\\' || c == 0) continue;
			if (c < 32 || c == 127) continue;
			out.push_back(c);
		}
		if (out.empty()) out = "upload.bin";
		return out;
	};

	// detect if fullPath appears to be a directory (no extension or ends with baseRoot)
	bool pathLooksLikeDir = false;
	if (!fullPath.empty() && fullPath.back() == '/') pathLooksLikeDir = true;
	else {
		size_t lastSlash = fullPath.find_last_of('/');
		std::string lastSeg = (lastSlash == std::string::npos) ? fullPath : fullPath.substr(lastSlash + 1);
		if (lastSeg.find('.') == std::string::npos) pathLooksLikeDir = true;
	}

	if (pathLooksLikeDir) {
		// choose filename from fileName or fallback
		std::string chosen = fileName.empty() ? "upload_" + std::to_string(time(NULL)) + ".bin" : sanitizeFilename(fileName);
		if (!fullPath.empty() && fullPath.back() == '/') fullPath += chosen;
		else fullPath += "/" + chosen;
	}

	std::string toWrite;
	if (!fileData.empty()) {
		toWrite = fileData;
	} else {
		// fallback: write raw body (used by curl when posting raw data)
		toWrite = body;
	}

	// 🔹 Ensure target directory exists (create recursively as needed)
	size_t slash = fullPath.find_last_of('/');
	std::string dirPath;
	if (slash == std::string::npos || slash == 0) dirPath = ".";
	else dirPath = fullPath.substr(0, slash);

	struct stat st2;
	if (stat(dirPath.c_str(), &st2) != 0) {
		// try to create directories recursively
		std::string cur;
		size_t p = 0;
		if (dirPath.size() && dirPath[0] == '/') { cur = "/"; p = 1; }
		while (p < dirPath.size()) {
			size_t next = dirPath.find('/', p);
			std::string part = dirPath.substr(0, next == std::string::npos ? dirPath.size() : next);
			if (stat(part.c_str(), &st2) != 0) {
				if (mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) {
					Logger::log(ERROR, "POST failed: cannot create directory " + part + " err=" + std::to_string(errno));
					return handler.makeErrorResponse(srv, 500);
				}
			}
			if (next == std::string::npos) break;
			p = next + 1;
		}
	}

	std::ofstream out(fullPath.c_str(), std::ios::binary);
	if (!out.is_open()) {
		Logger::log(ERROR, "POST failed: cannot open file " + fullPath);
		return handler.makeErrorResponse(srv, 500);
	}

	// write either parsed finalContent (urlencoded) or previously prepared toWrite
	if (!finalContent.empty()) out.write(finalContent.c_str(), finalContent.size());
	else out.write(toWrite.c_str(), toWrite.size());
	out.close();

	// 🔹 Prepare response
	std::ostringstream html;
	html << "<html><body><h1>File uploaded successfully</h1>"
	     << "<p>Saved as: " << fullPath << "</p>"
	     << "<p>Size: " << body.size() << " bytes</p>"
	     << "</body></html>";

	HttpResponse res(201, html.str());
	res.setHeader("Content-Type", "text/html");
	res.setHeader("Content-Length", std::to_string(html.str().size()));

	Logger::log(INFO, "POST: saved " + fullPath);
	return res;
}


std::optional<HttpResponse> serveDeleteStatic(const HttpRequest& req, const Server& srv, const Location& loc) {
	(void)req; (void)srv; (void)loc;
	return std::nullopt;
}