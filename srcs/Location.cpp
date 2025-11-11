#include "Location.hpp"

Location::Location()
	: _path("/"),
	_redirect(""),
	_root(""),
	_index("index.html"),
	_autoindex(false),
	_uploadPath(""),
	_hasReturn(false),
	_returnCode(0),
	_returnTarget("")
{
}

// =====================
// Getters
// =====================
const std::string& Location::getPath() const { return _path; }
const std::vector<std::string>& Location::getMethods() const { return _methods; }
const std::string& Location::getRedirect() const { return _redirect; }
const std::string& Location::getRoot() const { return _root; }
const std::string& Location::getIndex() const { return _index; }
bool Location::getAutoindex() const { return _autoindex; }
const std::map<std::string, std::string>& Location::getCgiExtensions() const { return _cgiExtensions; }
const std::string& Location::getUploadPath() const { return _uploadPath; }

// =====================
// Setters
// =====================
void Location::setPath(const std::string& p) { _path = p; }
void Location::setMethods(const std::vector<std::string>& m) { _methods = m; }
void Location::setClientMaxBodySize(size_t size) { _clientMaxBodySize = size; }
void Location::setRedirect(const std::string& r) { _redirect = r; }
void Location::setRoot(const std::string& r) { _root = r; }
void Location::setIndex(const std::string& i) { _index = i; }
void Location::setAutoindex(bool a) { _autoindex = a; }
void Location::setCgiExtensions(const std::map<std::string, std::string>& cgi) { _cgiExtensions = cgi; }
void Location::setUploadPath(const std::string& path) { _uploadPath = path; }

void Location::setCgiProgram(const std::string& p) { _cgiProgram = p; }
const std::string& Location::getCgiProgram() const { return _cgiProgram; }

void Location::setReturn(int code, const std::string& target) {
	_hasReturn = true;
	_returnCode = code;
	_returnTarget = target;
}

bool Location::hasReturn() const { return _hasReturn; }
int Location::getReturnCode() const { return _returnCode; }
const std::string& Location::getReturnTarget() const { return _returnTarget; }
