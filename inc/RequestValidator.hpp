#pragma once

#include "RequestHandler.hpp"

class RequestValidator {
	private:
		static bool isMethodAllowed(RequestHandler& handl, const std::vector<std::string>& allowed);
		static bool	checkHeaders(RequestHandler& handl, Server& srv);
		static bool checkUri(RequestHandler& handl, const Server& srv);
		static bool handleRedirect(RequestHandler& handl, Server& srv, Location& loc);
		static bool checkPost(RequestHandler& handl, Server& srv);

	public:
		static bool check(RequestHandler& handl, Server& srv, Location& loc);
};