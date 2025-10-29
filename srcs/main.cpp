#include <iostream>
#include "ConfigParser.hpp"

int main(int argc, char **argv) {

	std::string config_path = "conf/default.conf";

	if (argc > 2) {
		std::cerr << "Usage: ./webserv [config_file]" << std::endl;
		return 1;
	}

	if (argc == 2)
		config_path = argv[1];

	std::cout << "Using configuration: " << config_path << std::endl;

	try {
		ConfigParser parser(config_path);
		parser.parse();

	std::cout << " Lets pray for our servers! " << std::endl;
	const std::vector<Server>& servers = parser.getServers();

		for (size_t i = 0; i < servers.size(); ++i) {
			const Server& srv = servers[i];
			std::cout << "-----------------------------------\n";
			std::cout << "Server #" << i + 1 << ":\n";
			std::cout << "  Host: " << srv.getHost() << "\n";
			std::cout << "  Port: " << srv.getListenPort() << "\n";
			std::cout << "  Server Names: ";
			for (const auto& name : srv.getServerName()) std::cout << name << " ";
			std::cout << "\n";
			std::cout << "  Root: " << srv.getRoot() << "\n";
			std::cout << "  Index: " << srv.getIndex() << "\n";
			std::cout << "  Autoindex: " << (srv.getAutoindex() ? "on" : "off") << "\n";
			std::cout << "  Client Max Body Size: " << srv.getClientMaxBodySize() << "\n";
			std::cout << "  Allowed Methods: ";
			for (const auto& m : srv.getMethods()) std::cout << m << " ";
			std::cout << "\n";

			const std::vector<Location>& locs = srv.getLocations();
			for (size_t j = 0; j < locs.size(); ++j) {
				const Location& loc = locs[j];
				std::cout << "    Location #" << j + 1 << ":\n";
				std::cout << "      Path: " << loc.getPath() << "\n";
				std::cout << "      Root: " << loc.getRoot() << "\n";
				std::cout << "      Index: " << loc.getIndex() << "\n";
				std::cout << "      Autoindex: " << (loc.getAutoindex() ? "on" : "off") << "\n";
				std::cout << "      Allowed Methods: ";
				for (const auto& m : loc.getMethods()) std::cout << m << " ";
				std::cout << "\n";
				if (loc.hasReturn()) {
				std::cout << "      Return: " << loc.getReturnCode()
						<< " -> " << loc.getReturnTarget() << "\n";
				}
				// CGI extensions
				for (const auto& cgi : loc.getCgiExtensions())
					std::cout << "      CGI: " << cgi.first << " -> " << cgi.second << "\n";

				if (!loc.getUploadPath().empty())
					std::cout << "      Upload Path: " << loc.getUploadPath() << "\n";
			}

			std::cout << "  Error Pages:\n";
			for (const auto& e : srv.getErrorPages())
				std::cout << "    " << e.first << " -> " << e.second << "\n";

			std::cout << "-----------------------------------\n";
		}
		// ServerManager manager(parser.getServers());
		// manager.run();
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to start Webserv: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
