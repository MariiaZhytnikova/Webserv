#pragma once

#include <string>
#include <vector>
#include <map>

class Location {
private:
	std::string							_path;
	std::vector<std::string>			_methods;
	size_t								_clientMaxBodySize;
	std::string							_redirect;
	std::string							_root;
	std::string							_index;
	bool								_autoindex;
	std::map<std::string, std::string>	_cgiExtensions;
	std::string							_uploadPath;
	std::string							_cgiProgram;

	bool								_hasReturn;
	int									_returnCode;
	std::string							_returnTarget;

public:
	Location();
	~Location() = default;
	Location(const Location& other) = default;
	Location& operator=(const Location& other) = default;

	// Getters
	const std::string& getPath() const;
	const std::vector<std::string>& getMethods() const;
	const std::string& getRedirect() const;
	const std::string& getRoot() const;
	const std::string& getIndex() const;
	bool  getAutoindex() const;
	const std::map<std::string, std::string>& getCgiExtensions() const;
	const std::string& getUploadPath() const;
	const std::string& getCgiProgram() const;
	bool  hasReturn() const;
	int   getReturnCode() const;
	const std::string& getReturnTarget() const;

	// Setters
	void setPath(const std::string& p);
	void setMethods(const std::vector<std::string>& m);
	void setClientMaxBodySize(size_t size);
	void setRedirect(const std::string& r);
	void setRoot(const std::string& r);
	void setIndex(const std::string& i);
	void setAutoindex(bool a);
	void setCgiExtensions(const std::map<std::string, std::string>& cgi);
	void setUploadPath(const std::string& path);
	void setCgiProgram(const std::string& p);
	void setReturn(int code, const std::string& target);

};
