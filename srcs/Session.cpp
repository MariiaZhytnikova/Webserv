#include "Session.hpp"
#include <cstdlib>

std::string Session::generateSessionId() {
	static const char chars[] =
		"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	std::string id;
	for (int i = 0; i < 16; ++i)
		id += chars[rand() % (sizeof(chars) - 1)];
	return id;
}

Session::Session()
	: _id(generateSessionId()), _lastAccess(time(NULL)), _expirySeconds(3600) {}

Session::Session(const std::string& id)
	: _id(id), _lastAccess(time(NULL)), _expirySeconds(3600) {}

const std::string& Session::getId() const { return _id; }

void Session::set(const std::string& key, const std::string& value) {
	_data[key] = value;
	touch();
}

std::string Session::getSession(const std::string& key) const {
	std::map<std::string, std::string>::const_iterator it = _data.find(key);
	return (it != _data.end()) ? it->second : "";
}

const std::map<std::string, std::string>& Session::getData() const {
	return _data;
}

void Session::touch() { _lastAccess = time(NULL); }

bool Session::expired() const {
	return (difftime(time(NULL), _lastAccess) > _expirySeconds);
}

bool Session::has(const std::string& key) const {
	return _data.count(key);
}