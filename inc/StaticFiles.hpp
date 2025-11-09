#pragma once

#include "RequestHandler.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Server.hpp"
#include "Location.hpp"
#include <optional>

// Serve static files. Return an optional HttpResponse: if empty, the caller should
// continue with other handlers (CGI, 404, etc.).
std::optional<HttpResponse> serveGetStatic(const HttpRequest& req, const Server& srv, const Location& loc, RequestHandler& handler);
std::optional<HttpResponse> servePostStatic(const HttpRequest& req, const Server& srv, const Location& loc, RequestHandler& handler);
std::optional<HttpResponse> serveDeleteStatic(const HttpRequest& req, const Server& srv, const Location& loc, RequestHandler& handler);
