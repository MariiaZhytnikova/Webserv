#include "StaticDelete.hpp"
#include "RequestHandler.hpp"
#include "Logger.hpp"
#include "utils.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <errno.h>

std::optional<HttpResponse> serveDeleteStatic(const HttpRequest& req, const Server& srv, const Location& loc) {
	(void)req; (void)srv; (void)loc;
	return std::nullopt;
}