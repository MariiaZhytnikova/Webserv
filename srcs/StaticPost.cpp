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
	// ==========================================
	// STEP 1: Resolve the upload directory path
	// ==========================================
	// Build the filesystem path where the file will be saved
	// Example: POST to "/uploads/" → "./www/uploads/"
	
	std::string reqPath = req.getPath();          // Request URI: "/uploads/somefile.txt"
	std::string baseRoot = resolveRoot(srv, loc); // Base directory: "./www"
	std::string cleanReq = trimLeadingSlash(reqPath);
	std::string fullPath;

	// Build full filesystem path
	if (reqPath.empty() || reqPath == "/") {
		fullPath = baseRoot + "/";
	} else {
		fullPath = baseRoot + "/" + cleanReq;  // "./www/uploads/somefile.txt"
	}

	//Logger::log(DEBUG, "fullPath = " + fullPath);

	const std::string &body = req.getBody();
	const std::string contentType = req.getHeader("content-type");

	std::string fileName;  // Will hold the extracted filename
	std::string fileData;  // Will hold the actual file content

	// ==========================================
	// STEP 2: Parse request body based on Content-Type
	// ==========================================
	// HTTP POST can send data in different formats:
	// 1. multipart/form-data (browser file uploads)
	// 2. application/x-www-form-urlencoded (HTML forms)
	// 3. Raw binary data (curl --data-binary)

	// ─────────────────────────────────────────
	// FORMAT 1: MULTIPART/FORM-DATA (Browser uploads)
	// ─────────────────────────────────────────
	// Structure:
	//   --boundary
	//   Content-Disposition: form-data; name="file"; filename="image.png"
	//   Content-Type: image/png
	//   
	//   [binary file data here]
	//   --boundary--
	
	if (contentType.find("multipart/form-data") != std::string::npos)
	{
		size_t bpos = contentType.find("boundary=");
		if (bpos != std::string::npos)
		{
			// Extract the boundary string from Content-Type header
			// Example: "boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW"
			std::string boundary = contentType.substr(bpos + 9);
			
			// Remove quotes if present (some clients send boundary="value")
			if (!boundary.empty() && boundary.front() == '"')
				boundary.erase(0, 1);
			if (!boundary.empty() && boundary.back() == '"')
				boundary.pop_back();
			
			// Multipart boundaries are prefixed with "--" in the body
			boundary = "--" + boundary;

			// Find the first boundary marker in the body
			size_t pos = body.find(boundary);
			if (pos == std::string::npos)
				return handler.makeErrorResponse(srv, 400);

			pos += boundary.size() + 2; // Skip past "boundary\r\n"

			// Find the end of the part headers (blank line "\r\n\r\n")
			size_t hdrEnd = body.find("\r\n\r\n", pos);
			if (hdrEnd == std::string::npos)
				return handler.makeErrorResponse(srv, 400);

			// Extract and parse the multipart headers
			std::string headers = body.substr(pos, hdrEnd - pos);
			std::istringstream hl(headers);
			std::string hline;

			// Search through headers for the filename
			while (std::getline(hl, hline)) {

				// Remove carriage return (Windows line ending)
				if (!hline.empty() && hline.back() == '\r')
					hline.pop_back();

				// Convert to lowercase for case-insensitive search
				std::string low = hline;
				std::transform(low.begin(), low.end(), low.begin(), ::tolower);

				// Look for "filename=" in headers
				// Example: Content-Disposition: form-data; name="file"; filename="photo.jpg"
				size_t fpos = low.find("filename=");
				if (fpos != std::string::npos) {

					// Case 1: filename="Makefile" (quoted)
					size_t q1 = hline.find('"', fpos);
					if (q1 != std::string::npos) {
						size_t q2 = hline.find('"', q1 + 1);
						if (q2 != std::string::npos) {
							fileName = hline.substr(q1 + 1, q2 - q1 - 1);
							if (!fileName.empty())  // Only accept non-empty filenames
								break;
						}
					}
					// Case 2: filename=Makefile (unquoted, less common)
					if (fileName.empty()) {
						size_t start = low.find("filename=") + 9;
						size_t end = hline.find(';', start);
						if (end == std::string::npos) end = hline.size();
						fileName = hline.substr(start, end - start);
						if (!fileName.empty())
							break;
					}
				}
			}

			// Extract the actual file data
			size_t contentStart = hdrEnd + 4;  // Skip past "\r\n\r\n"
			
			// Find the ending boundary to know where file data ends
			// Look for "\r\n--boundary" to avoid false matches in binary data
			std::string endBoundary = "\r\n" + boundary;
			size_t nextBoundary = body.find(endBoundary, contentStart);
			if (nextBoundary == std::string::npos) {
				// Fallback: try without CRLF for malformed requests
				nextBoundary = body.find(boundary, contentStart);
				if (nextBoundary == std::string::npos) {
					Logger::log(ERROR, "Malformed multipart: missing end boundary");
					return handler.makeErrorResponse(srv, 400);
				}
			}

			// Extract file content between start and boundary
			fileData = body.substr(contentStart, nextBoundary - contentStart);

			// Remove trailing CRLF before boundary if present
			if (fileData.size() >= 2 &&
				fileData.substr(fileData.size()-2) == "\r\n")
				fileData.erase(fileData.size()-2);
		}
	}
	// ─────────────────────────────────────────
	// FORMAT 2: X-WWW-FORM-URLENCODED (HTML forms)
	// ─────────────────────────────────────────
	// Structure: "key1=value1&key2=value2"
	// Example: "filename=test.txt&content=Hello+World"
	
	else if (contentType.find("application/x-www-form-urlencoded") != std::string::npos)
	{
		// Check if body contains key=value pairs
		if (body.find('=') == std::string::npos)
		{
			// No '=' found, treat entire body as raw data
			fileName = "raw_" + std::to_string(time(NULL)) + ".txt";
			fileData = body;
		}
		else
		{
			// Parse URL-encoded parameters
			std::map<std::string,std::string> params;
			size_t pos = 0;
			while (pos < body.size()) {
				size_t amp = body.find('&', pos);  // Find parameter separator
				std::string pair = body.substr(pos, amp - pos);
				size_t eq = pair.find('=');
				if (eq != std::string::npos)
					params[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq+1));
				if (amp == std::string::npos) break;
				pos = amp + 1;
			}

			// Extract filename and content from parsed parameters
			fileName = params.count("filename") ?
					   params["filename"] :
					   "upload_" + std::to_string(time(NULL)) + ".txt";

			// If "content" parameter exists, use it; otherwise save the raw body
			fileData = params.count("content") ? params["content"] : body;
		}
	}
	// ─────────────────────────────────────────
	// FORMAT 3: RAW BINARY DATA (curl --data-binary)
	// ─────────────────────────────────────────
	// No Content-Type or unknown type: treat entire body as file
	else
	{
		fileName = "raw_" + std::to_string(time(NULL)) + ".bin";
		fileData = body;
	}

	// ==========================================
	// STEP 3: Sanitize and validate filename
	// ==========================================
	// Remove dangerous characters (/, \, control chars, etc.)
	fileName = sanitizeFilename(fileName);
	
	// Ensure we have a valid filename after sanitization
	if (fileName.empty()) {
		fileName = "upload_" + std::to_string(time(NULL)) + ".bin";
	}

	// ==========================================
	// STEP 4: Determine final file path
	// ==========================================
	// Handle different scenarios:
	// 1. fullPath is an existing directory → append filename
	// 2. fullPath ends with / → treat as directory
	// 3. fullPath has no extension → treat as directory
	// 4. fullPath has extension → use as-is (file already specified)
	
	struct stat st;
	
	if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
		// Path exists and is a directory
		fullPath = ensureTrailingSlash(fullPath) + fileName;
	}
	else if (fullPath.back() == '/') {
		// Path ends with '/', treat as directory
		fullPath += fileName;
	}
	else {
		// Check if the last component has a file extension
		size_t slash = fullPath.find_last_of('/');
		std::string last = fullPath.substr(slash + 1);
		if (last.find('.') == std::string::npos) {
			// No extension found, treat as directory
			fullPath = ensureTrailingSlash(fullPath) + fileName;
		}
		// else: fullPath already contains a filename with extension, use as-is
	}

	// ==========================================
	// STEP 5: Create directory structure if needed
	// ==========================================
	// Recursively create all parent directories
	// Example: "./www/uploads/2024/images/" → creates each level
	
	size_t slash = fullPath.find_last_of('/');
	std::string dirPath = fullPath.substr(0, slash);  // Get directory portion

	if (stat(dirPath.c_str(), &st) != 0)
	{
		// Directory doesn't exist, create it recursively
		size_t p = 0;
		while (p < dirPath.size()) {
			size_t next = dirPath.find('/', p);
			if (next == 0) { // Skip leading slash in absolute paths
				p = 1;
				continue;
			}
			std::string part = dirPath.substr(0, next);
			if (stat(part.c_str(), &st) != 0) {
				// Directory doesn't exist, create it
				if (mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) {
					Logger::log(ERROR, "Failed to create directory: " + part);
					return handler.makeErrorResponse(srv, 500);
				}
			}
			if (next == std::string::npos) break;
			p = next + 1;
		}
	}

	// ==========================================
	// STEP 6: Write file to disk
	// ==========================================
	// Open file in binary mode to preserve exact content
	std::ofstream out(fullPath.c_str(), std::ios::binary);
	if (!out.is_open())
		return handler.makeErrorResponse(srv, 500);

	// Write raw file data (binary-safe)
	out.write(fileData.c_str(), fileData.size());
	out.close();
	Logger::log(INFO, "POST: saved " + fullPath);

	// ==========================================
	// STEP 7: Return success response
	// ==========================================
	// Send 200 OK with template variables for success page
	return handler.makeSuccessResponse(
	srv,
	{
		{"filename", fullPath},
		{"size", std::to_string(fileData.size())}
	}
);
}
