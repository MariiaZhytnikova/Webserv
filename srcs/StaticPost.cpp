#include "StaticPost.hpp"
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

	// Logger::log(DEBUG, "PREFIX = " + prefix);
	// Logger::log(DEBUG, "reqPath = " + reqPath);
	// Logger::log(DEBUG, "baseRoot = " + baseRoot);
	// Logger::log(DEBUG, "fullPath = " + fullPath);

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

	// 🔹 1) MULTIPART/FORM-DATA (Browser)
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
			// {
			//     size_t fn = headers.find("filename=\"");
			//     if (fn != std::string::npos) {
			//         size_t q1 = headers.find('"', fn + 10);
			//         size_t q2 = headers.find('"', q1 + 1);
			//         fileName = headers.substr(q1 + 1, q2 - q1 - 1);
			//     }
			// }
			std::istringstream hl(headers);
			std::string hline;

			while (std::getline(hl, hline)) {

				// remove CR
				if (!hline.empty() && hline.back() == '\r')
					hline.pop_back();

				std::string low = hline;
				std::transform(low.begin(), low.end(), low.begin(), ::tolower);

				// Search filename in ANY header line
				size_t fpos = low.find("filename=");
				if (fpos != std::string::npos) {

					// Case 1: filename="Makefile"
					size_t q1 = hline.find('"', fpos);
					if (q1 != std::string::npos) {
						size_t q2 = hline.find('"', q1 + 1);
						if (q2 != std::string::npos) {
							fileName = hline.substr(q1 + 1, q2 - q1 - 1);
							break;
						}
					}
					// Case 2: filename=Makefile (no quotes)
					size_t start = low.find("filename=") + 9;
					size_t end = hline.find(';', start);
					if (end == std::string::npos) end = hline.size();
					fileName = hline.substr(start, end - start);
					break;
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
	// 🔹 X-WWW-FORM-URLENCODED (curl -d)
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
	// 🔹 3) RAW BODY (curl --data-binary, no content-type)
	else
	{
		fileName = "raw_" + std::to_string(time(NULL)) + ".bin";
		fileData = body;
	}
	// 🔹 Resolve filename inside fullPath
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
	// 🔹 Ensure directory exists
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
	// 🔹 Write file
	std::ofstream out(fullPath.c_str(), std::ios::binary);
	if (!out.is_open())
		return handler.makeErrorResponse(srv, 500);

	out.write(fileData.c_str(), fileData.size());
	out.close();
	Logger::log(INFO, "POST: saved " + fullPath);

	//🔹 Response
   return handler.makeSuccessResponse(
	srv,
	{
		{"filename", fullPath},
		{"size", std::to_string(fileData.size())}
	}
);
}
