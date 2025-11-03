#include "utils.hpp"

bool isDirective(const std::string& line) {
	// If line ends with ';' → it's a directive
	return !line.empty() && line.back() == ';';
}

std::string trimLine(const std::string& raw) {
	std::string line = raw;

	// skip empty lines or comments
	size_t commentPos = line.find('#');				// comments not at beggining
	if (commentPos != std::string::npos) {
		line = line.substr(0, commentPos);
	}

	line.erase(line.begin(), std::find_if(line.begin(), line.end(),
					[](unsigned char ch){ return !std::isspace(ch); }));
	line.erase(std::find_if(line.rbegin(), line.rend(),
					[](unsigned char ch){ return !std::isspace(ch); }).base(), line.end());

	return line;
}

// =====================
// Location Helpers
// =====================

std::string extractPath(const std::string& line) {
	size_t start = line.find(' ') + 1;
	size_t end = line.find('{', start);
	std::string path = line.substr(start, end - start);
	// trim whitespace if needed
	path.erase(path.begin(), std::find_if(path.begin(), path.end(),
										 [](unsigned char ch){ return !std::isspace(ch); }));
	path.erase(std::find_if(path.rbegin(), path.rend(),
							[](unsigned char ch){ return !std::isspace(ch); }).base(), path.end());
	return path;
}

std::vector<std::string> parseMethods(const std::string& line) {
	std::vector<std::string> methods; // "allow_methods GET POST DELETE;" -> ["GET", "POST", "DELETE"]
	size_t pos = line.find(' ');
	if (pos == std::string::npos) return methods;

	std::string rest = line.substr(pos + 1);
	// remove trailing semicolon
	if (!rest.empty() && rest.back() == ';')
		rest.pop_back();

	// split by spaces
	std::istringstream iss(rest);
	std::string method;
	while (iss >> method) {
		methods.push_back(method);
	}

	return methods;
}

std::string parseValue(const std::string& line) {
	size_t pos = line.find(' ');	//"root /var/www/html;" -> "/var/www/html"
	if (pos == std::string::npos) return "";

	std::string value = line.substr(pos + 1);

	// remove trailing semicolon
	if (!value.empty() && value.back() == ';')
		value.pop_back();

	// trim leading whitespace
	value.erase(value.begin(),
				std::find_if(value.begin(), value.end(),
							 [](unsigned char ch){ return !std::isspace(ch); }));

	// trim trailing whitespace
	value.erase(std::find_if(value.rbegin(), value.rend(),
							 [](unsigned char ch){ return !std::isspace(ch); }).base(),
				value.end());

	return value;
}

bool parseOnOff(const std::string& line) {
	std::string value = parseValue(line); // on/off -> true/false

	// convert to lowercase for safety
	std::transform(value.begin(), value.end(), value.begin(), ::tolower);

	if (value == "on")
		return true;
	else if (value == "off")
		return false;
	else
		throw std::runtime_error("Invalid on/off value: " + value);
}

std::pair<std::string, std::string> parseCgi(const std::string& line) {
	std::string value = parseValue(line); // ".php /usr/bin/php-cgi" -> { ".php", "/usr/bin/php-cgi" }

	std::istringstream iss(value);
	std::string ext, path;
	if (!(iss >> ext >> path)) {
		throw std::runtime_error("Invalid cgi_extension format: " + line);
	}

	return std::make_pair(ext, path);
}

size_t parseSize(const std::string& value) {
	if (value.empty()) return 0;

	char suffix = value.back();
	size_t num;
	std::string numStr = value;

	if (suffix == 'K' || suffix == 'M' || suffix == 'G') {
		numStr = value.substr(0, value.size() - 1);
	} else {
		suffix = 0;
	}

	try {
		num = std::stoul(numStr);
	} catch (...) {
		throw std::runtime_error("Invalid size: " + value);
	}

	switch (suffix) {
		case 'K': return num * 1024;
		case 'M': return num * 1024 * 1024;
		case 'G': return num * 1024 * 1024 * 1024;
		default:  return num;
	}
}

std::pair<int, std::string> parseReturn(const std::string& line) {
	std::istringstream iss(parseValue(line));
	std::string first, second;

	if (!(iss >> first))
		throw std::runtime_error("Invalid return directive: " + line);

	// Case 1: "return /path;" -> default to 302
	if (first[0] == '/') {
		return std::make_pair(302, first);
	}

	// Case 2: "return 301 /path;"
	if (!(iss >> second))
		throw std::runtime_error("Invalid return directive: " + line);

	int code;
	try {
		code = std::stoi(first);
	} catch (...) {
		throw std::runtime_error("Invalid return code in: " + line);
	}

	return std::make_pair(code, second);
}

// Extracts the file extension from the request path or filename
std::string getFileExtension(const std::string& path) {
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos)
		return ""; // no extension
	return path.substr(dot); // includes the dot, e.g. ".php"
}

