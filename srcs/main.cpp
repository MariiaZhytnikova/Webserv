#include "ConfigParser.hpp"
#include "ServerManager.hpp"
#include "Logger.hpp"
#include <csignal>

bool g_running = true;

void handleSignal(int) {
	g_running = false;
}

int main(int argc, char **argv) {

	signal(SIGINT, handleSignal);

	Logger::init("./log/access.log", "./log/error.log");
	Logger::log(TRACE, "starting server...");
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

		ServerManager manager(parser.getServers());
		g_running = true;
		manager.run();
	}
	catch (const std::exception& e) {
		Logger::log(ERROR, std::string(e.what()));
		return 1;
	}
	return 0;
}
