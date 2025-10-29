#pragma once

#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <poll.h>

class ServerManager {
private:
	std::vector<Server>	_servers;
	std::map<int, int>	_portSocketMap;	// key = port, value = socket fd
	std::unordered_map<int, std::string> _clientBuffers; // client fd → received data

	void setupSockets();				// create/bind/listen
	void acceptNewClient(int listenFd, std::vector<pollfd>& fds);
	void readFromClient(int clientFd, std::vector<pollfd>& fds, size_t index);
	void handleRequest(int clientFd);

	Server& matchServer(const HttpRequest& req, int listenPort);

public:
	ServerManager() = delete;
	ServerManager(const std::vector<Server>& servers);
	ServerManager(const ServerManager& other) = delete;
	ServerManager& operator=(const ServerManager& other) = delete;
	~ServerManager();

	void run();							// main event loop
};
