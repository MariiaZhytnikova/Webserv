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
	while (std::getline(stream, line) && line != "\r") {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		size_t pos = line.find(':');
		if (pos != std::string::npos) {

			// 🔹 Check key for forbidden characters
			std::string key = line.substr(0, pos);
			std::transform(key.begin(), key.end(), key.begin(),
				[](unsigned char c){ return std::tolower(c); });
			if (key.empty()
				|| key.find(' ') != std::string::npos
				|| key.find_first_not_of(
					"!#$%&'*+-.^_`|~0123456789"
					"abcdefghijklmnopqrstuvwxyz"
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos)
			{
				_headers.insert(std::make_pair("malformed", line));
				continue;
			}

			// 🔹 Check value for forbidden characters
			std::string value = line.substr(pos + 1);
			while (!value.empty() && value[0] == ' ')
				value.erase(0, 1);
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

		} else {

			// 🔹 No colon found
			_headers.insert(std::make_pair("malformed", line));
		}
	}

	for (auto &pairs : _headers)
		Logger::log(TRACE, pairs.first + " : " + pairs.second);

	auto range = _headers.equal_range("cookie");
	for (auto it = range.first; it != range.second; ++it) {
		parseCookies(it->second);
	}
}

void HttpRequest::parseCookies(const std::string& cookieStr) {
	std::istringstream cookieStream(cookieStr);
	std::string pair;

	while (std::getline(cookieStream, pair, ';')) {
		size_t eqPos = pair.find('=');
		if (eqPos != std::string::npos) {
			std::string name = pair.substr(0, eqPos);
			std::string value = pair.substr(eqPos + 1);

			// Trim spaces
			while (!name.empty() && name[0] == ' ')
				name.erase(0, 1);
			while (!value.empty() && value[0] == ' ')
				value.erase(0, 1);

			_cookies[name] = value;
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
const std::multimap<std::string, std::string>& HttpRequest::getHeaders() const { return _headers; }
const std::string& HttpRequest::getBody() const { return _body; }

const std::string& HttpRequest::getCookies(const std::string& which) const {
	static const std::string empty;
	std::map<std::string, std::string>::const_iterator it = _cookies.find(which);
	if (it != _cookies.end())
		return it->second;
	return empty;
}