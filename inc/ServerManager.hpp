#pragma once

#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "SessionManager.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <poll.h>

const size_t MAX_HEADER_SIZE = 8192;
const time_t CLIENT_TIMEOUT = 10;

struct ClientState {
	int		fd;
	int		requestCount;
	time_t	lastActivity;
};

class RequestHandler;

class ServerManager {
private:
	std::vector<Server>						_servers;
	std::vector<struct pollfd>				_fds;
	std::map<int, int>						_portSocketMap;	// key = socket fd, value = port
	std::unordered_map<int, std::string>	_clientBuffers; // client fd → received data
	std::map<int,int>						_clientToListenFd;
	SessionManager							_sessionManager;
	std::unordered_map<int, ClientState>	_clientState;
	std::vector<int>						_toClose;

	void setupSockets();
	void acceptNewClient(int listenFd);
	void readFromClient(int clientFd);

	bool readSocketIntoBuffer(int clientFd, std::string &buf);
	bool hasFullRequest(const std::string &buf, size_t &reqEnd);
	std::string extractNextRequest(std::string &buf, size_t reqEnd);
	bool shouldCloseAfterRequest(int fd, const RequestHandler &h);
	void checkTimeouts();

public:
	ServerManager() = delete;
	ServerManager(const std::vector<Server>& servers);
	ServerManager(const ServerManager& other) = delete;
	ServerManager& operator=(const ServerManager& other) = delete;
	~ServerManager();

	// -------------------- Getters --------------------
	const std::vector<Server>&	getServers() const;
	const Server&				getServer(size_t index) const;
	Server&						getServer(size_t index);
	SessionManager& 			getSessionManager();

	void run();
	void cleanupClient(int clientFd);
};
