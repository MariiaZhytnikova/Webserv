#include "utils.hpp"

bool isDirective(const std::string& line) {
	// If line ends with ';' → it's a directive
	return !line.empty() && line.back() == ';';
}

std::string trim(const std::string &s) {
	size_t start = s.find_first_not_of(" \t");
	if (start == std::string::npos)
		return ""; // string is all spaces

	size_t end = s.find_last_not_of(" \t");
	return s.substr(start, end - start + 1);
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

std::string listFilesPlain(const std::string& dir) {
	DIR* d = opendir(dir.c_str());
	if (!d) return "";

	std::ostringstream out;
	struct dirent* e;

	while ((e = readdir(d))) {
		// ignore hidden files "." and ".."
		if (e->d_name[0] == '.') continue;

		out << e->d_name << "\n";
	}

	closedir(d);
	return out.str();
}

std::string urlDecode(const std::string &src) {
	std::string ret;
	ret.reserve(src.size());

	for (std::size_t i = 0; i < src.size(); ++i) {

		if (src[i] == '%' && i + 2 < src.size()) {
			int value = 0;
			std::sscanf(src.substr(i + 1, 2).c_str(), "%x", &value);
			ret += static_cast<char>(value);
			i += 2;
		}
		else if (src[i] == '+') {
			ret += ' ';
		}
		else {
			ret += src[i];
		}
	}

	return ret;
}

std::string sanitizeFilename(const std::string &n) {
	std::string out;
	for (char c : n)
		// Remove control chars, slashes, backslashes
		if (c > 31 && c != '/' && c != '\\')
			out.push_back(c);
	return out.empty() ? "upload.bin" : out;
};

bool endsWith(const std::string& str, const std::string& suffix) {
	if (suffix.size() > str.size())
		return false;
	return std::equal(
		suffix.rbegin(), suffix.rend(),
		str.rbegin()
	);
}

//Helper: read file into string (binary-safe)
bool readFile(const std::string& path, std::string& out) {
	std::ifstream f(path.c_str(), std::ios::in | std::ios::binary);
	if (!f) return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return true;
}

std::string detectMime(const std::string& path) {
	std::string ext = getFileExtension(path);
	if (ext == ".html" || ext == ".htm") return "text/html";
	if (ext == ".css") return "text/css";
	if (ext == ".js") return "application/javascript";
	if (ext == ".json") return "application/json";
	if (ext == ".png") return "image/png";
	if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
	if (ext == ".gif") return "image/gif";
	if (ext == ".txt") return "text/plain";
	if (ext == ".svg") return "image/svg+xml";
	if (ext == ".ico") return "image/x-icon";
	return "application/octet-stream";
}

std::string ensureTrailingSlash(const std::string &s) {
	if (s.empty()) return "/";

	//check last char of the string
	char last = s[s.size() - 1];

	//if last char is already '/', nothing to change
	if (last == '/') return s;

	//otherwise, append '/' and return new string
	std::string result = s + "/";

	return result;
}

std::string trimLeadingSlash(const std::string &s) {
		// If the string is empty, just return it as is.
	if (s.empty()) {
		return "";
	}
		// We will count how many leading '/' characters the string has.
	size_t index = 0;

	// Move index forward while:
	//  - it is still inside the string
	//  - the current character is '/'
	while (index < s.size() && s[index] == '/') {
		index++;
	}
	// Now 'index' points to the first non-'/' character
	// Example:
	//   s = "///uploads"
	//   index becomes 3
	//
	// We return the substring starting from index to the end.
	return s.substr(index);
}