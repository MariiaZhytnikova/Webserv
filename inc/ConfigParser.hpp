#pragma once

#include "Server.hpp"
#include "Location.hpp"

enum ConfigLineType {
	BLOCK_START_SERVER,
	BLOCK_START_LOCATION,
	BLOCK_END,
	DIRECTIVE,
	UNKNOWN
};

enum LocationDirective {
	ALLOW_METHODS,
	CLIENT_MAX_BODY_SIZE_LOC,
	ROOT,
	INDEX,
	AUTOINDEX,
	REDIRECT,
	UPLOAD_PATH,
	CGI_EXTENSION,
	CGI_PASS,
	RETURN,
	OTHER
};

enum ServerDirective {
	LISTEN,
	SERVER_NAME,
	ERROR_PAGE,
	CLIENT_MAX_BODY_SIZE,
	ROOT_SERVER,
	INDEX_SERVER,
	AUTOINDEX_SERVER,
	ALLOW_METHODS_SERVER,
	UNDEFINED
};

class ConfigParser {
	private:
		std::string _config_path;
		std::vector<Server> _servers;

		void parseServerBlock(std::ifstream& file);
		void parseLocationBlock(std::ifstream& file, Server& server, const std::string& line);
		void parseServerDirective(const std::string& line, Server& server);
		void parseLocationDirective(const std::string& line, Location& location);

		void setDefaultServers();

		ConfigLineType		getLineType(const std::string& line);
		LocationDirective	getLocationDirective(const std::string& line);
		ServerDirective		getServerDirective(const std::string& line);

	public:
		ConfigParser() = delete;
		ConfigParser(const std::string& path);
		ConfigParser(const ConfigParser& other) = default;
		ConfigParser& operator=(const ConfigParser& other) = default;
		~ConfigParser() = default;

		void parse();

	// -------------------- Getters --------------------
		const std::vector<Server>& getServers() const;
};