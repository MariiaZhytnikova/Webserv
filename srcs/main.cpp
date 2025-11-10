#include "ConfigParser.hpp"
#include "ServerManager.hpp"
#include "Logger.hpp"
#include <csignal>

bool g_running = true; // the only definition

void handleSignal(int) {
	g_running = false;
}

void PrintMe(const std::vector<Server>& servers) {
	for (size_t i = 0; i < servers.size(); ++i) {
		const Server& srv = servers[i];
		std::cout << "-----------------------------------\n";
		std::cout << "Server #" << i + 1 << ":\n";
		std::cout << "  Host: " << srv.getHost() << "\n";
		std::cout << "  Port: " << srv.getListenPort() << "\n";
		std::cout << "  Server Names: ";
		for (const auto& name : srv.getServerNames()) std::cout << name << " ";
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
			std::cout << "      Is redirect: " << std::to_string(loc.hasReturn()) << "\n";
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
}

int main(int argc, char **argv) {

	signal(SIGINT, handleSignal);

	Logger::init("./log/access.log", "./log/error.log");
	Logger::log(INFO, "starting server...");
	std::string config_path = "conf/default.conf";

	if (argc > 2) {
		Logger::log(ERROR, "usage: ./webserv [config_file]");
		return 1;
	}

	if (argc == 2)
		config_path = argv[1];

	Logger::log(INFO, "configuration file: " + config_path);

	try {
		ConfigParser parser(config_path);
		parser.parse();

		// PrintMe(parser.getServers());
		ServerManager manager(parser.getServers());
		g_running = true;
		manager.run();
	}
	catch (const std::exception& e) {
		// std::cerr << "Failed to start Webserv: " << e.what() << std::endl;
		Logger::log(ERROR, std::string(e.what()));
		return 1;
	}
	return 0;
}
