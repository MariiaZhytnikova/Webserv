#include "Server.hpp"

Server::Server()
	: _host("127.0.0.1"),
	  _port(80),
	  _clientMaxBodySize(0),
	  _root(""),
	  _index("index.html"),
	  _isDefault(false),
	  _autoindex(false),
	  _hasListen(false),
	  _hasRoot(false)  { }

// =====================
// Getters
// =====================
int Server::getListenPort() const { return _port; }
const std::vector<std::string>& Server::getServerNames() const { return _serverNames; }
const std::vector<Location>& Server::getLocations() const { return _locations; }
const std::string& Server::getHost() const { return _host; }
const std::map<int, std::string>& Server::getErrorPages() const { return _errorPages; }

size_t Server::getClientMaxBodySize() const { return _clientMaxBodySize; }
const std::string& Server::getRoot() const { return _root; }
const std::string& Server::getIndex() const { return _index; }
bool Server::isDefault() const { return _isDefault; }
bool Server::getAutoindex() const { return _autoindex; }
const std::vector<std::string>& Server::getMethods() const { return _methods; }

// =====================
// Setters
// =====================

void Server::setListenPort(int port) { _port = port; }
void Server::setServerName(const std::string& name) { _serverNames.push_back(name); }
void Server::setHost(const std::string& host) { _host = host; }
void Server::setRoot(const std::string& root) { _root = root; }
void Server::setIndex(const std::string& index) { _index = index; }
void Server::setErrorPage(int code, const std::string& path) { _errorPages[code] = path; }
void Server::setClientMaxBodySize(size_t size) { _clientMaxBodySize = size; }
void Server::setDefault(bool value) { _isDefault = value; }
void Server::setAutoindex(bool value) { _autoindex = value; }
void Server::setMethod(const std::vector<std::string>& methods) { _methods = methods; }
void Server::setListenFlag() { _hasListen = true; }
void Server::setRootFlag() { _hasRoot = true; }

void Server::addLocation(const Location& loc) { _locations.push_back(loc); }
void Server::addLocation(Location&& loc) { _locations.push_back(std::move(loc)); }

bool Server::hasListen() const { return _hasListen; }
bool Server::hasRoot() const { return _hasRoot; }

#include <iostream>

Location Server::findLocation(const std::string& path) const {

	// No locations
	if (_locations.empty()) {
		return Location();
	}

	Location const* bestMatch = NULL;

	// Exact match
	for (std::vector<Location>::const_iterator it = _locations.begin();
		it != _locations.end(); ++it)
	{
		if (it->getPath() == path)
			return *it; // exact match wins
	}

	// Longest prefix match
	for (std::vector<Location>::const_iterator it = _locations.begin();
		it != _locations.end(); ++it)
	{
		const std::string& locPath = it->getPath();

		if (path.compare(0, locPath.size(), locPath) == 0) {
			// Only first match
			if (bestMatch == NULL || locPath.size() > bestMatch->getPath().size()) {
				bestMatch = &(*it);
			}
		}
	}

	// If nothing matched, return default "/"
	if (bestMatch != NULL) {
		return *bestMatch;
	}

	// fallback to root "/" explicitly
	for (std::vector<Location>::const_iterator it = _locations.begin();
		it != _locations.end(); ++it)
	{
		if (it->getPath() == "/")
			return *it;
	}

	// Should not happen
	return _locations.front();
}
