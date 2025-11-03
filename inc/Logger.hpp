#pragma once
#include <iostream>
#include <string>
#include <ctime>

enum LogLevel {
	INFO,
	TRACE,
	DEBUG,
	WARNING,
	ERROR
};

class Logger {

	private:
		static std::string getTimestamp();
		static std::string levelToString(LogLevel level);
		static std::string colorForLevel(LogLevel level);

		static std::ofstream _accessFile;
		static std::ofstream _errorFile;

	public:
		static void log(LogLevel level, const std::string& msg);
		static void init(const std::string& accessPath, const std::string& errorPath);
};
