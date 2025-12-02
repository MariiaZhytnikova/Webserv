#include "HttpRequest.hpp"
#include "Logger.hpp"
#include <algorithm>
#include "utils.hpp"

HttpRequest::HttpRequest(const std::string& raw)
: _method(), _path(), _version(),  _body(), _headers()
{
	std::istringstream stream(raw);
	parseRequestLine(stream);
	parseHeaders(stream);
	parseCookies();
	parseBody(stream);

}

void HttpRequest::parseRequestLine(std::istringstream& stream) {
	std::string line;
	if (!std::getline(stream, line) || line.empty())
		return;
	if (!line.empty() && line.back() == '\r')
		line.pop_back();

	std::istringstream firstLine(line);
	firstLine >> _method >> _path >> _version;

	if (_path.empty())
		_path = "/";

	// split query and path
	size_t qmark = _path.find('?');
	if (qmark != std::string::npos) {
		_queryString = _path.substr(qmark + 1);
		_path = _path.substr(0, qmark);
	} else {
		_queryString.clear();
	}

	Logger::log(INFO, "requested path: '" + _path + "'");
	// Logger::log(DEBUG, "query string:" + _queryString);
}

void HttpRequest::parseHeaders(std::istringstream& stream) {
	std::string line;

	while (std::getline(stream, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (line.empty())
			break; // end of headers

		size_t pos = line.find(':');
		if (pos == std::string::npos) {
			_headers.insert(std::make_pair("malformed", line));
			continue;
		}

		std::string rawKey = line.substr(0, pos);
		std::string rawVal = line.substr(pos + 1);

		// trim key
		std::string key = trim(rawKey);
		std::transform(key.begin(), key.end(), key.begin(),
					   [](unsigned char c){ return std::tolower(c); });

		if (key.empty()) {
			_headers.insert(std::make_pair("malformed", line));
			continue;
		}

		// trim value
		std::string value = trim(rawVal);

		// check value for control chars
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

	// for (auto &pairs : _headers)
		// Logger::log(
		// TRACE,
		// std::string("from parseHeaders: ") + pairs.first + " : " + pairs.second);
	
}

void HttpRequest::parseBody(std::istringstream& stream) {
	size_t len = 0;

	std::multimap<std::string, std::string>::const_iterator it =
		_headers.find("content-length");

	if (it != _headers.end()) {
		len = static_cast<size_t>(atoi(it->second.c_str()));
	}

	if (len == 0) {
		_body.clear();
		return;
	}

	_body.resize(len);
	stream.read(&_body[0], len);
}

std::string HttpRequest::getHeader(const std::string& key) const {
	auto it = _headers.find(key);
	if (it != _headers.end())
		return it->second;
	return "";
}

void HttpRequest::parseCookies() {
	auto range = _headers.equal_range("cookie");
	// Logger::log(DEBUG, "cookie received: " + getHeader("cookie"));

	for (auto it = range.first; it != range.second; ++it) {
		const std::string& headerValue = it->second;

		// split by ";"
		std::istringstream s(headerValue);
		std::string kv;

		while (std::getline(s, kv, ';')) {
			size_t eq = kv.find('=');
			if (eq == std::string::npos)
				continue;

			std::string key = trim(kv.substr(0, eq));
			std::string val = trim(kv.substr(eq + 1));

			if (!key.empty())
				_cookies[key] = val;
			// Logger::log(WARNING, "cookie received: " + key + " = " + val);
		}
	}
}

const std::string& HttpRequest::getMethod() const { return _method; }
const std::string& HttpRequest::getPath() const { return _path; }
const std::string& HttpRequest::getVersion() const { return _version; }
const std::multimap<std::string, std::string>& HttpRequest::getHeaders() const {
	return _headers; }
const std::string& HttpRequest::getBody() const { return _body; }
const std::string	HttpRequest::getQueryString() const { return _queryString; }

bool HttpRequest::isHeaderValue(const std::string& key,
								const std::string& value) const {
	auto range = _headers.equal_range(key);
	for (auto it = range.first; it != range.second; ++it) {
		std::istringstream iss(it->second);
		std::string token;
		while (std::getline(iss, token, ',')) {
			token = trim(token);
			if (token == value)
				return true;
		}
	}
	return false;
}

std::string HttpRequest::getCookie(const std::string& key) const {
	auto it = _cookies.find(key);
	if (it != _cookies.end())
		return it->second;
	return "";
}

std::string HttpRequest::returnHeaderValue(const std::string& key,
										   const std::string& subkey) const {
	auto range = _headers.equal_range(key);
	for (auto it = range.first; it != range.second; ++it) {
		std::istringstream iss(it->second);
		std::string token;
		while (std::getline(iss, token, ';')) {
			token = trim(token);

			if (token.size() > subkey.size() &&
				token.compare(0, subkey.size(), subkey) == 0 &&
				token[subkey.size()] == '=') {
				return token.substr(subkey.size() + 1);
			}
		}
	}
	return "";
}

