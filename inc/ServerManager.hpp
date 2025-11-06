#pragma once

#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "SessionManager.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <poll.h>

class ServerManager {
private:
	std::vector<Server>						_servers;
	std::vector<struct pollfd>				_fds;
	std::map<int, int>						_portSocketMap;	// key = socket fd, value = port
	std::unordered_map<int, std::string>	_clientBuffers; // client fd → received data
	std::map<int,int>						_clientToListenFd;
	SessionManager							_sessionManager;

	void setupSockets();				// create/bind/listen
	void acceptNewClient(int listenFd);
	void readFromClient(int clientFd, size_t index);
	void handleRequest(int clientFd);

public:
	ServerManager() = delete;
	ServerManager(const std::vector<Server>& servers);
	ServerManager(const ServerManager& other) = delete;
	ServerManager& operator=(const ServerManager& other) = delete;
	~ServerManager();

	const std::vector<Server>& getServers() const;
	const Server&	getServer(size_t index) const;
	Server&			getServer(size_t index);
	SessionManager& getSessionManager();
	void cleanupClient(int clientFd);

	void run();
};
