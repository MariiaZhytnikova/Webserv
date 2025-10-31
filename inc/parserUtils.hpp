#pragma once

#include <string>
#include <vector>
#include <map>
#include <utility>
#include <algorithm>
#include <sstream>

bool isDirective(const std::string& line);
std::string trimLine(const std::string& raw);
std::string parseValue(const std::string& line);
std::vector<std::string> parseMethods(const std::string& line);
bool parseOnOff(const std::string& line);
std::pair<std::string, std::string> parseCgi(const std::string& line);
std::string extractPath(const std::string& line);
size_t parseSize(const std::string& value);
std::pair<int, std::string> parseReturn(const std::string& line);

std::string getFileExtension(const std::string& path);