#include "ConfigParser.hpp"
#include "parserUtils.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>

ConfigParser::ConfigParser(const std::string& path) : _config_path(path) {}

const std::vector<Server>& ConfigParser::getServers() const { return _servers; }

ConfigLineType ConfigParser::getLineType(const std::string& line) {
	if (line == "server {") return BLOCK_START_SERVER;
	if (line.substr(0, 8) == "location") return BLOCK_START_LOCATION;
	if (line == "}") return BLOCK_END;
	if (isDirective(line)) return DIRECTIVE;
	return UNKNOWN;
}

LocationDirective ConfigParser::getLocationDirective(const std::string& line) {
	if (line.rfind("allow_methods", 0) == 0) return ALLOW_METHODS;
	if (line.rfind("root", 0) == 0) return ROOT;
	if (line.rfind("index", 0) == 0) return INDEX;
	if (line.rfind("autoindex", 0) == 0) return AUTOINDEX;
	if (line.rfind("redirect", 0) == 0) return REDIRECT;
	if (line.rfind("upload_path", 0) == 0) return UPLOAD_PATH;
	if (line.rfind("cgi_extension", 0) == 0) return CGI_EXTENSION;
	if (line.rfind("cgi_pass", 0) == 0) return CGI_PASS;
	if (line.rfind("return", 0) == 0) return RETURN;
	return OTHER;
}

ServerDirective ConfigParser::getServerDirective(const std::string& line) {
	if (line.rfind("server_name", 0) == 0) return SERVER_NAME;
	if (line.rfind("listen", 0) == 0) return LISTEN;
	if (line.rfind("error_page", 0) == 0) return ERROR_PAGE;
	if (line.rfind("client_max_body_size", 0) == 0) return CLIENT_MAX_BODY_SIZE;
	if (line.rfind("root", 0) == 0) return ROOT_SERVER;
	if (line.rfind("index", 0) == 0) return INDEX_SERVER;
	if (line.rfind("autoindex", 0) == 0) return AUTOINDEX_SERVER;
	if (line.rfind("allow_methods", 0) == 0) return ALLOW_METHODS_SERVER;
	return UNDEFINED;
}


void ConfigParser::parse() {
	std::ifstream file(_config_path);
	if (!file.is_open()) {
		std::cerr << "Error: cannot open config file '" 
				  << _config_path << "'" << std::endl;
		return;
	}
	std::string raw;

	while (std::getline(file, raw)) {
		std::string trimmed = trimLine(raw);
		if (trimmed.empty()) continue;

		switch (getLineType(trimmed)) {
			case BLOCK_START_SERVER:
				// reads until the closing BLOCK_END
				parseServerBlock(file);
				break;
			case UNKNOWN:
				throw std::runtime_error("Unknown line outside server block: " + trimmed);
			default:
				// other cases are invalid outside server
				throw std::runtime_error("Unexpected line outside server block: " + trimmed);
		}
	}
}

void ConfigParser::parseServerBlock(std::ifstream& file) {

	Server server;
	std::string raw;

	while (std::getline(file, raw)) {
		std::string line = trimLine(raw);
		if (line.empty()) continue;

		switch (getLineType(line)) {
			case BLOCK_START_LOCATION:
				parseLocationBlock(file, server, line);
				break;
			case DIRECTIVE:
				parseServerDirective(line, server);
				break;
			case BLOCK_END:
				_servers.push_back(server);
				return;
			case BLOCK_START_SERVER:
				throw std::runtime_error("Nested server bock is invalid: " + line);
			case UNKNOWN:
				throw std::runtime_error("Invalid line inside server block: " + line);
		}
	}
	// EOF reached without closing }
	throw std::runtime_error("Server block not closed with '}'");
}

void ConfigParser::parseLocationBlock(std::ifstream& file, Server& server, const std::string& line) {

	Location location;
	std::string raw;

	location.setPath(extractPath(line));
	while (std::getline(file, raw)) {
		std::string line = trimLine(raw);
		if (line.empty()) continue;

		switch (getLineType(line)) {
			case DIRECTIVE:
				parseLocationDirective(line, location);
				break;
			case BLOCK_END:
				server.addLocation(location);
				return;
			case BLOCK_START_SERVER:
			case BLOCK_START_LOCATION:
			case UNKNOWN:
				throw std::runtime_error("Invalid line inside location block: " + line);
		}
	}
	// EOF reached without closing }
	throw std::runtime_error("Location block not closed with '}'");
}

void ConfigParser::parseLocationDirective(const std::string& line, Location& loc) {

	switch (getLocationDirective(line)) {
		case ALLOW_METHODS:
			loc.setMethods(parseMethods(line));
			break;
		case ROOT:
			loc.setRoot(parseValue(line));
			break;
		case INDEX:
			loc.setIndex(parseValue(line));
			break;
		case AUTOINDEX:
			loc.setAutoindex(parseOnOff(line));
			break;
		case REDIRECT:
			loc.setRedirect(parseValue(line));
			break;
		case UPLOAD_PATH:
			loc.setUploadPath(parseValue(line));
			break;
		case CGI_EXTENSION: {
			auto cgi = parseCgi(line);
			auto current = loc.getCgiExtensions();
			current[cgi.first] = cgi.second;
			loc.setCgiExtensions(current);
			break;
		}
		case CGI_PASS: {
			std::string prog = parseValue(line);	// "/usr/bin/python3"
			loc.setCgiProgram(prog);
			break;
		}
		case RETURN: {
			std::pair<int, std::string> ret = parseReturn(line);
			loc.setReturn(ret.first, ret.second);
			break;
		}
		case OTHER:
		default:
			throw std::runtime_error("Unknown directive in location block: " + line);
	}
}

void ConfigParser::parseServerDirective(const std::string& line, Server& server) {
	switch (getServerDirective(line)) {
		case LISTEN: {
			std::string value = parseValue(line);	// "127.0.0.1:8080" or "8080"
			size_t colon = value.find(':');
			if (colon != std::string::npos) {
				server.setHost(value.substr(0, colon));
				server.setListenPort(std::stoi(value.substr(colon + 1)));
			} else {
				server.setHost("*");				// bind all interfaces
				server.setListenPort(std::stoi(value));
			}
			break;
		}
		case SERVER_NAME:
			server.setServerName(parseValue(line));
			break;
		case ERROR_PAGE: {
			std::istringstream iss(parseValue(line));
			int code;
			std::string page;
			if (!(iss >> code >> page))
				throw std::runtime_error("Invalid error_page: " + line);

			server.setErrorPage(code, page);
			break;
		}
		case CLIENT_MAX_BODY_SIZE:
			server.setClientMaxBodySize(parseSize(parseValue(line)));
			break;
		case ROOT_SERVER:
			server.setRoot(parseValue(line));
			break;
		case INDEX_SERVER:
			server.setIndex(parseValue(line));
			break;
		case AUTOINDEX_SERVER:
			server.setAutoindex(parseOnOff(line));
			break;
		case ALLOW_METHODS_SERVER:
			server.setMethod(parseMethods(line));
			break;
		case UNDEFINED:
		default:
			throw std::runtime_error("Unknown directive in server block: " + line);
	}
}
