#include "CgiHandler.hpp"
#include <sys/wait.h>
#include <fcntl.h>

CgiHandler::CgiHandler(const HttpRequest& req) : _request(req) {}

HttpResponse CgiHandler::execute(const std::string& scriptPath) {
	std::map<std::string, std::string> env;
	env = buildEnv(scriptPath);
	std::string output = runProcess(scriptPath, env);

	// Parse CGI output: split headers from body
	size_t headerSize = output.find("\r\n\r\n");
	std::string headers = output.substr(0, headerSize);
	std::string body = output.substr(headerSize + 4);

	HttpResponse res(200, body);
	std::istringstream hs(headers);
	std::string line;
	while (std::getline(hs, line) && !line.empty()) {
		size_t sep = line.find(':');
		if (sep != std::string::npos) {
			std::string key = line.substr(0, sep);
			std::string val = line.substr(sep + 1);
			res.setHeader(key, val);
		}
	}
	return res;
}

std::map<std::string, std::string> CgiHandler::buildEnv(const std::string& scriptPath) const {
	
	std::map<std::string, std::string> env;

	// Separate path and query
	std::string fullPath = _request.getPath();
	std::string query;
	size_t qpos = fullPath.find('?');
	if (qpos != std::string::npos) {
		query = fullPath.substr(qpos + 1);
		fullPath = fullPath.substr(0, qpos); // remove query part and leave only clean path
	}

	// Base CGI variables
	env["GATEWAY_INTERFACE"] = "CGI/1.1";
	env["SCRIPT_FILENAME"] = scriptPath;
	env["REQUEST_METHOD"] = _request.getMethod();
	env["QUERY_STRING"] = query;
	env["CONTENT_LENGTH"] = std::to_string(_request.getBody().size());
	env["CONTENT_TYPE"] = _request.getHeader("Content-Type");
	env["SERVER_PROTOCOL"] = "HTTP/1.1";
	env["SERVER_SOFTWARE"] = "MyWebServ/1.0";
	env["REDIRECT_STATUS"] = "200"; // required for PHP-CGI

	return env;
}

std::string CgiHandler::runProcess(const std::string& scriptPath, const std::map<std::string, std::string>& env) {
	int inPipe[2], outPipe[2];
	pipe(inPipe);
	pipe(outPipe);

	pid_t pid = fork();
	if (pid == 0) {
		// --- Child ---
		dup2(inPipe[0], STDIN_FILENO);
		dup2(outPipe[1], STDOUT_FILENO);
		close(inPipe[1]);
		close(outPipe[0]);

		// Prepare environment
		std::vector<std::string> envStrings;
		std::vector<char*> envp;
		for (std::map<std::string, std::string>::const_iterator it = env.begin(); it != env.end(); ++it) {
			envStrings.push_back(it->first + "=" + it->second);
		}
		for (size_t i = 0; i < envStrings.size(); ++i)
			envp.push_back(const_cast<char*>(envStrings[i].c_str()));
		envp.push_back(NULL);

		// Prepare args
		char* args[] = { const_cast<char*>(scriptPath.c_str()), NULL };

		execve(scriptPath.c_str(), args, &envp[0]);
		_exit(1); // if execve fails
	}
	else if (pid > 0) {
		// --- Parent ---
		close(inPipe[0]);
		close(outPipe[1]);

		// Send body to child (for POST)
		if (_request.getMethod() == "POST")
			write(inPipe[1], _request.getBody().c_str(), _request.getBody().size());
		close(inPipe[1]);

		// Read CGI output
		std::ostringstream output;
		char buffer[1024];
		ssize_t bytes;
		while ((bytes = read(outPipe[0], buffer, sizeof(buffer))) > 0) {
			output.write(buffer, bytes);
		}
		close(outPipe[0]);
		waitpid(pid, NULL, 0);
		return output.str();
	}
	else {
		throw std::runtime_error("fork failed");
	}
	return "";
}
