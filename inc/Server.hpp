#pragma once

#include <string>
#include <vector>
#include <map>
#include "Location.hpp"

class Server {
private:
	std::string					_host;
	int							_port;
	std::vector<std::string>	_serverNames;
	std::map<int, std::string>	_errorPages;
	size_t						_clientMaxBodySize;
	std::string					_root;
	std::string					_index;
	bool						_isDefault;
	bool						_autoindex;
	std::vector<std::string>	_methods;
	std::vector<Location>		_locations;

public:
	// Constructors
	Server();
	Server(const Server& other) = default;
	Server& operator=(const Server& other) = default;
	~Server() = default;

	// -------------------- Getters --------------------
	const std::string& getHost() const;
	int getListenPort() const;
	const std::vector<std::string>& getServerNames() const;
	const std::map<int, std::string>& getErrorPages() const;
	size_t getClientMaxBodySize() const;
	const std::string& getRoot() const;
	const std::string& getIndex() const;
	bool isDefault() const;
	bool getAutoindex() const;
	const std::vector<std::string>& getMethods() const;
	const std::vector<Location>& getLocations() const;

	// -------------------- Setters --------------------
	void setHost(const std::string& host);
	void setListenPort(int port);
	void setServerName(const std::string& name);
	void setErrorPage(int code, const std::string& path);
	void setClientMaxBodySize(size_t size);
	void setRoot(const std::string& root);
	void setIndex(const std::string& index);
	void setDefault(bool isDefault);
	void setAutoindex(bool value);
	void setMethod(const std::vector<std::string>& methods);

	// -------------------- Locations --------------------
	void addLocation(const Location& loc);
	void addLocation(Location&& loc);
	Location findLocation(const std::string& path) const;
};
