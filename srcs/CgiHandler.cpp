#include "CgiHandler.hpp"
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "utils.hpp"
#include "Logger.hpp"

CgiHandler::CgiHandler(const HttpRequest& req) : _request(req) {}

HttpResponse CgiHandler::execute(
	const std::string& scriptPath,
	const std::string& interpreterPath,
	const std::string& serverRoot
	) {

	std::map<std::string, std::string> env;

	if (access(scriptPath.c_str(), F_OK) != 0) {
		throw std::runtime_error("CGI script does not exist");
	}
	env = buildEnv(scriptPath, serverRoot);
	std::string output = runProcess(scriptPath, env, interpreterPath);

	// Parse CGI output: split headers from body
	size_t headerSize = output.find("\r\n\r\n");
	if (headerSize == std::string::npos)
		headerSize = output.find("\n\n");
	std::string headers = output.substr(0, headerSize);
	std::string body = output.substr(headerSize + ((output[headerSize] == '\r') ? 4 : 2));

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

std::map<std::string, std::string> CgiHandler::buildEnv(
	const std::string& scriptPath,
	const std::string& serverRoot
	) const {

	std::map<std::string, std::string> env;
	std::string fullPath = _request.getPath();
	std::string scriptName = scriptPath.substr(scriptPath.find_last_of('/') + 1);

	// SCRIPT_NAME: the executed CGI file
	env["SCRIPT_NAME"] = "/" + scriptName;

	// PATH_INFO = extra path after script name
	std::string pathInfo;
	size_t scriptPos = fullPath.find(scriptName);
	if (scriptPos != std::string::npos) {
		pathInfo = fullPath.substr(scriptPos + scriptName.size());
	}
	env["PATH_INFO"] = pathInfo;

	//  Logger::log(
	// 	DEBUG,
	// 	"Enviroment in CGI: PATH_INFO: " + pathInfo + " SCRIPT_NAME: /" + scriptName);

	// Standard CGI varszz
	env["GATEWAY_INTERFACE"]	= "CGI/1.1";
	env["SCRIPT_FILENAME"]		= scriptPath;
	env["REQUEST_METHOD"]		= _request.getMethod();
	env["QUERY_STRING"]			= _request.getQueryString();
	env["CONTENT_LENGTH"]		= std::to_string(_request.getBody().size());
	env["CONTENT_TYPE"]			= _request.getHeader("Content-Type");
	env["SERVER_PROTOCOL"]		= "HTTP/1.1";
	env["SERVER_SOFTWARE"]		= "MyWebServ/1.0";
	env["REDIRECT_STATUS"]		= "200";

	char cwd[4096];
	getcwd(cwd, sizeof(cwd));
	std::string absRoot = std::string(cwd) + "/" + serverRoot;
	env["SERVER_ROOT"] = absRoot;

	return env;
}

std::string CgiHandler::runProcess(
	const std::string& scriptPath,
	const std::map<std::string, std::string>& env,
	const std::string& interpreterPath
	) {

	int inPipe[2], outPipe[2];
	if (pipe(inPipe) < 0 || pipe(outPipe) < 0)
		throw std::runtime_error("pipe failed");

	pid_t pid = fork();
	if (pid < 0)
		throw std::runtime_error("fork failed");

	if (pid == 0) {
		// Child
		dup2(inPipe[0], STDIN_FILENO);
		dup2(outPipe[1], STDOUT_FILENO);
		close(inPipe[1]);
		close(outPipe[0]);

		std::vector<std::string> envStrings;
		std::vector<char*> envp;
		for (std::map<std::string, std::string>::const_iterator it = env.begin(); it != env.end(); ++it) {
			envStrings.push_back(it->first + "=" + it->second);
		}
		for (size_t i = 0; i < envStrings.size(); ++i)
			envp.push_back(const_cast<char*>(envStrings[i].c_str()));
		envp.push_back(NULL);
		char* args[] = {
			const_cast<char*>(interpreterPath.c_str()),
			const_cast<char*>(scriptPath.c_str()),
			NULL
		};
		execve(interpreterPath.c_str(), args, envp.data());
		perror("execve failed");
		_exit(1);
	}

	// Parent
	close(inPipe[0]);
	close(outPipe[1]);

	if (_request.getMethod() == "POST")
		write(inPipe[1], _request.getBody().c_str(), _request.getBody().size());
	close(inPipe[1]);

	std::ostringstream output;
	char buffer[1024];
	ssize_t bytes;
	while ((bytes = read(outPipe[0], buffer, sizeof(buffer))) > 0)
		output.write(buffer, bytes);

	close(outPipe[0]);
	waitpid(pid, NULL, 0);

	return output.str();
}
