#include "HttpRequest.hpp"
#include "Logger.hpp"
#include <algorithm>

HttpRequest::HttpRequest(const std::string& raw) {
	std::istringstream stream(raw);
	parseRequestLine(stream);
	parseHeaders(stream);
	parseBody(stream);
}

void HttpRequest::parseRequestLine(std::istringstream& stream) {
	std::string line;
	if (!std::getline(stream, line) || line.empty())
		return;
	if (!line.empty() && line.back() == '\r') line.pop_back();

	std::istringstream firstLine(line);
	firstLine >> _method >> _path >> _version;
	if (_path.empty()) _path = "/";
	Logger::log(DEBUG, std::string("path in request:") + _path);
}

void HttpRequest::parseHeaders(std::istringstream& stream) {
	std::string line;

	while (std::getline(stream, line)) {
		// Remove trailing '\r' if present
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		// End of headers
		if (line.empty())
			break;

		// Look for ':'
		size_t pos = line.find(':');
		if (pos == std::string::npos) {
			_headers.insert(std::make_pair("malformed", line));
			continue;
		}

		// Extract and lowercase key
		std::string key = line.substr(0, pos);
		std::transform(key.begin(), key.end(), key.begin(),
					   [](unsigned char c){ return std::tolower(c); });

		// Trim spaces from key
		key.erase(0, key.find_first_not_of(" \t"));
		key.erase(key.find_last_not_of(" \t") + 1);

		if (key.empty()) {
			_headers.insert(std::make_pair("malformed", line));
			continue;
		}

		// Extract value and trim leading spaces
		std::string value = line.substr(pos + 1);
		value.erase(0, value.find_first_not_of(" \t"));

		// Check value for control characters
		bool valueMalformed = false;
		for (size_t i = 0; i < value.size(); ++i) {
			unsigned char c = value[i];
			if ((c < 32 && c != '\t') || c == 127) {
				valueMalformed = true;
				break;
			}
		}
		if (valueMalformed) {
			_headers.insert(std::make_pair("malformed", line));
			continue;
		}

		_headers.insert(std::make_pair(key, value));
	}

	for (auto &pairs : _headers)
		Logger::log(TRACE, std::string("from parseHeaders: ") + pairs.first + " : " + pairs.second);

}

void HttpRequest::parseBody(std::istringstream& stream) {
	std::string remaining;
	std::string line;
	while (std::getline(stream, line))
		remaining += line + "\n";
	if (!remaining.empty() && remaining.back() == '\n') remaining.pop_back();
	_body = remaining;
}

std::string HttpRequest::getHeader(const std::string& key) const {
	auto it = _headers.find(key);
	if (it != _headers.end())
		return it->second;
	return "";
}

const std::string& HttpRequest::getMethod() const { return _method; }
const std::string& HttpRequest::getPath() const { return _path; }
const std::string& HttpRequest::getVersion() const { return _version; }
const std::multimap<std::string, std::string>& HttpRequest::getHeaders() const { return _headers; }
const std::string& HttpRequest::getBody() const { return _body; }

bool HttpRequest::isHeaderValue(const std::string& key, const std::string& value) const {
	auto range = _headers.equal_range(key);
	for (auto it = range.first; it != range.second; ++it) {
		std::string headerValue = it->second;

		// Split comma-separated tokens
		std::istringstream iss(headerValue);
		std::string token;
		while (std::getline(iss, token, ',')) {
			// Remove leading/trailing spaces
			token.erase(0, token.find_first_not_of(" \t"));
			token.erase(token.find_last_not_of(" \t") + 1);

			if (token == value) return true;
		}
	}
	return false;
}

std::string HttpRequest::returnHeaderValue(const std::string& key, const std::string& subkey) const {
	auto range = _headers.equal_range(key); // all headers with this key
	for (auto it = range.first; it != range.second; ++it) {
		std::string headerValue = it->second; // e.g., "foo=bar; session_id=abcdef123; x=y"

		std::istringstream iss(headerValue);
		std::string token;
		while (std::getline(iss, token, ';')) {
			// trim spaces
			token.erase(0, token.find_first_not_of(" \t"));
			token.erase(token.find_last_not_of(" \t") + 1);

			if (token.compare(0, subkey.size(), subkey) == 0 && token[subkey.size()] == '=') {
				return token.substr(subkey.size() + 1); // return by value
			}
		}
	}

	return ""; // empty string if not found
}