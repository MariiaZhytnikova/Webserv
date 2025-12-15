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
	if (access(scriptPath.c_str(), F_OK) != 0) {
		throw std::runtime_error("CGI script does not exist");
	}

	std::map<std::string, std::string> env = buildEnv(scriptPath, serverRoot);

	// Run the script and capture stdout, stderr, and exit code
	std::string output, errorOutput;
	int exitCode = runProcess(scriptPath, env, interpreterPath, output, errorOutput);

	if (exitCode != 0) {
		throw std::runtime_error("CGI script execution failed");
	}

	size_t headerSize = output.find("\r\n\r\n");
	if (headerSize == std::string::npos)
		headerSize = output.find("\n\n");

	if (headerSize == std::string::npos) {
		throw std::runtime_error("Invalid CGI output");
	}

	std::string headers = output.substr(0, headerSize);
	std::string body = output.substr(headerSize + ((output[headerSize] == '\r') ? 4 : 2));

	HttpResponse res(200, body);

	std::istringstream hs(headers);
	std::string line;
	bool hasContentType = false;
	while (std::getline(hs, line) && !line.empty()) {
		size_t sep = line.find(':');
		if (sep != std::string::npos) {
			std::string key = line.substr(0, sep);
			std::string val = line.substr(sep + 1);
			res.setHeader(key, val);
			if (key == "Content-Type")
				hasContentType = true;
		}
	}

	if (!hasContentType) {
		throw std::runtime_error("Invalid CGI output");
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

int CgiHandler::runProcess(
	const std::string& scriptPath,
	const std::map<std::string, std::string>& env,
	const std::string& interpreterPath,
	std::string& stdoutStr,
	std::string& stderrStr
) {
	const int CGI_TIMEOUT = 10;

	int inPipe[2], outPipe[2], errPipe[2];
	if (pipe(inPipe) < 0 || pipe(outPipe) < 0 || pipe(errPipe) < 0)
		throw std::runtime_error("pipe failed");

	pid_t pid = fork();
	if (pid < 0)
		throw std::runtime_error("fork failed");

	if (pid == 0) {
		// Child
		dup2(inPipe[0], STDIN_FILENO);
		dup2(outPipe[1], STDOUT_FILENO);
		dup2(errPipe[1], STDERR_FILENO);

		close(inPipe[1]);
		close(outPipe[0]);
		close(errPipe[0]);

		std::vector<std::string> envStrings;
		std::vector<char*> envp;
		for (auto& it : env)
			envStrings.push_back(it.first + "=" + it.second);
		for (size_t i = 0; i < envStrings.size(); ++i)
			envp.push_back(const_cast<char*>(envStrings[i].c_str()));
		envp.push_back(nullptr);

		char* args[] = {
			const_cast<char*>(interpreterPath.c_str()),
			const_cast<char*>(scriptPath.c_str()),
			nullptr
		};
		execve(interpreterPath.c_str(), args, envp.data());
		perror("execve failed");
		_exit(127);
	}

	// Parent
	close(inPipe[0]);
	close(outPipe[1]);
	close(errPipe[1]);

	if (_request.getMethod() == "POST")
		write(inPipe[1], _request.getBody().c_str(), _request.getBody().size());
	close(inPipe[1]);

	fcntl(outPipe[0], F_SETFL, O_NONBLOCK);
	fcntl(errPipe[0], F_SETFL, O_NONBLOCK);

	stdoutStr.clear();
	stderrStr.clear();

	int status = -1;
	time_t start = time(NULL);

	char buffer[2048];

	while (true) {
		pid_t result = waitpid(pid, &status, WNOHANG);
		if (result == pid) {
			ssize_t n;
			while ((n = read(outPipe[0], buffer, sizeof(buffer))) > 0) stdoutStr.append(buffer, n);
			while ((n = read(errPipe[0], buffer, sizeof(buffer))) > 0) stderrStr.append(buffer, n);
			break;
		}

		// Timeout check
		if (difftime(time(NULL), start) > CGI_TIMEOUT) {
			kill(pid, SIGKILL);
			waitpid(pid, &status, 0);
			throw std::runtime_error("CGI script timed out");
		}

		ssize_t n;
		while ((n = read(outPipe[0], buffer, sizeof(buffer))) > 0) stdoutStr.append(buffer, n);
		while ((n = read(errPipe[0], buffer, sizeof(buffer))) > 0) stderrStr.append(buffer, n);

		usleep(100 * 1000); // sleep 100ms
	}

	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}