#pragma once

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Server.hpp"
#include "Location.hpp"

class CgiHandler {

	public:
		CgiHandler(const HttpRequest& req);
		CgiHandler(const CgiHandler& other) = default;
		CgiHandler& operator=(const CgiHandler& other) = delete;
		~CgiHandler() = default;

		HttpResponse execute(const std::string& scriptPath, const std::string& interpreterPath);

	private:
		const HttpRequest& _request;

		std::map<std::string, std::string> buildEnv(const std::string& scriptPath) const;
		std::string runProcess(
			const std::string& scriptPath,
			const std::map<std::string, std::string>& env,
			const std::string& interpreterPath
		);

};