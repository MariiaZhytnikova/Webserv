#include "Logger.hpp"
#include <fstream>

// 🔹 Define static members
std::ofstream Logger::_accessFile;
std::ofstream Logger::_errorFile;

std::string Logger::getTimestamp() {
	time_t now = time(NULL);
	char buf[32];
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
	return std::string(buf);
}

std::string Logger::levelToString(LogLevel level) {
	switch (level) {
		case INFO:		return "INFO ";
		case TRACE:		return "TRACE";
		case DEBUG:		return "DEBUG";
		case WARNING:	return "WARNING";
		case ERROR:		return "ERROR";
		default:		return "LOG";
	}
}

std::string Logger::colorForLevel(LogLevel level) {
	switch (level) {
		case INFO:		return "\033[32m"; // green
		case TRACE:		return "\033[34m"; // blue
		case DEBUG:		return "\033[1;36m"; // cyan
		case WARNING:	return "\033[33m"; // yellow
		case ERROR:		return "\033[31m"; // red
		default:		return "\033[0m";
	}
}

void Logger::log(LogLevel level, const std::string& msg) {
	//  🔹  Terminal
	std::string color = colorForLevel(level);
	std::string reset = "\033[0m";
	std::cout << "[" << getTimestamp() << "] "
			  << color << "[" << levelToString(level) << "] " << reset
			  << msg << std::endl;

	//  🔹  Log files
	std::string timestamp = getTimestamp();
	std::string levelStr = levelToString(level);
	std::string line = "[" + timestamp + "] " + levelStr + ": " + msg + "\n";

	if (level == ERROR || level == WARNING)
		_errorFile << line << std::flush;
	else if (level == INFO)
		_accessFile << line << std::flush;
}

void Logger::init(const std::string& accessPath, const std::string& errorPath) {
	_accessFile.open(accessPath.c_str(), std::ios::app);
	_errorFile.open(errorPath.c_str(), std::ios::app);

	if (!_accessFile.is_open() || !_errorFile.is_open()) {
		throw std::runtime_error("failed to open log files");
	}
}