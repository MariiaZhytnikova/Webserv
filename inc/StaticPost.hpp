#pragma once

#include "RequestHandler.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Server.hpp"
#include "Location.hpp"
#include <optional>

std::optional<HttpResponse> servePostStatic(
	const HttpRequest& req,
	const Server& srv, const
	Location& loc,
	RequestHandler& handler
	);
