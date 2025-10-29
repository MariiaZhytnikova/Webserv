#include "HttpRequest.hpp"

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
}

void HttpRequest::parseHeaders(std::istringstream& stream) {
	std::string line;
	while (std::getline(stream, line) && line != "\r") {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		size_t pos = line.find(":");
		if (pos != std::string::npos) {
			std::string key = line.substr(0, pos);
			std::string value = line.substr(pos + 1);
			while (!value.empty() && value[0] == ' ') value.erase(0, 1);
			_headers[key] = value;
		}
	}
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
	std::map<std::string, std::string>::const_iterator it = _headers.find(key);
	if (it != _headers.end())
		return it->second;
	return ""; 
}

const std::string& HttpRequest::getMethod() const { return _method; }
const std::string& HttpRequest::getPath() const { return _path; }
const std::string& HttpRequest::getVersion() const { return _version; }
const std::map<std::string, std::string>& HttpRequest::getHeaders() const { return _headers; }
const std::string& HttpRequest::getBody() const { return _body; }