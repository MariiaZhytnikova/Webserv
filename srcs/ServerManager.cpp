#include "ServerManager.hpp"
#include "RequestHandler.hpp"
#include "Logger.hpp"
#include <sys/socket.h> // for socket, bind, listen
#include <netinet/in.h> // for sockaddr_in
#include <arpa/inet.h> // for inet_pton, htons
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <algorithm>

extern bool g_running;

ServerManager::ServerManager(const std::vector<Server>& servers)
	: _servers(servers), _sessionManager() { }

ServerManager::~ServerManager() {
	for (auto& pair : _portSocketMap)
		close(pair.first);
	for (auto& client : _clientToListenFd)
		close(client.first);
}

const std::vector<Server>& ServerManager::getServers() const { return _servers; }

const Server& ServerManager::getServer(size_t index) const {
	if (index >= _servers.size())
		throw std::out_of_range("server index out of range");
	return _servers[index];
}

Server& ServerManager::getServer(size_t index) {
	if (index >= _servers.size())
		throw std::out_of_range("server index out of range");
	return _servers[index];
}

SessionManager& ServerManager::getSessionManager() { return _sessionManager; }

void ServerManager::setupSockets() {
	for (size_t i = 0; i < _servers.size(); ++i) {
		Server& srv = _servers[i];
		int port = srv.getListenPort();
		// if (_portSocketMap.find(port) != _portSocketMap.end())
		// 	continue; // socket for this port already exists

		// check if a server is already bound to THIS port
		bool portUsed = false;
		for (std::map<int,int>::iterator it = _portSocketMap.begin();
			it != _portSocketMap.end(); ++it)
		{
			if (it->second == port) {
				portUsed = true;
				Logger::log(DEBUG,
					"Skipping creating socket for port " + std::to_string(port) +
					" (already bound on fd=" + std::to_string(it->first) + ")");
				break;
			}
		}

		if (portUsed) {
			continue;
		}

		Logger::log(DEBUG,
			"Creating NEW socket for port " + std::to_string(port));

		// Address family: IPv4 (AF_INET)
		// SOCK_STREAM: TCP (reliable, connection-oriented)
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
			throw std::runtime_error("failed to create socket: " + std::string(strerror(errno)));

		// Allows restart immediately and reuse the same port safely
		int opt = 1;		// 0/1 -> off/on
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
			throw std::runtime_error("failed to setsockopt: " + std::string(strerror(errno)));
		// Make socket non-blocking
		if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
			throw std::runtime_error("failed to set non-blocking: " + std::string(strerror(errno)));
		// Make socket close-on-exec
		if (fcntl(sock, F_SETFD, FD_CLOEXEC) == -1)
			throw std::runtime_error("failed to set close-on-exec: " + std::string(strerror(errno)));

/*  ----- bind()/listen() -----
	from <netinet/in.h>

		struct sockaddr_in {
			sa_family_t    sin_family;   // address family (AF_INET for IPv4)
			in_port_t      sin_port;     // port number (16-bit), must be in network byte order
			struct in_addr sin_addr;     // IPv4 address
			char           sin_zero[8];  // padding, usually zeroed
		};
*/
		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));

		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);

		if (srv.getHost() == "*" || srv.getHost().empty()) {
			addr.sin_addr.s_addr = INADDR_ANY;
		} else {
			if (inet_pton(AF_INET, srv.getHost().c_str(), &addr.sin_addr) <= 0)
				throw std::runtime_error("invalid host: " + srv.getHost());
		}
		// associates the socket with an IP address and port
		// failure occurs if the port is already in use, you don’t have permission, or the IP is invalid
		if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
			throw std::runtime_error("failed to bind socket: " + std::string(strerror(errno)));

		// start listening for incoming TCP connections on the socket
		// failure returns -1 if the socket is invalid or not bound
		// SOMAXCONN: OS decide how many pending connections can wait
		if (listen(sock, 511) < 0)
			throw std::runtime_error("failed to listen on socket: " + std::string(strerror(errno)));
		else
			Logger::log(INFO, "server started on port: " + std::to_string(port));
		_portSocketMap[sock] = port;
	}
}

/* 
poll() - system call that allows your program to wait for activity on
	multiple fds(sockets) at the same time, without busy-waiting.

	struct pollfd {
		int fd;        // File descriptor
		short events;  // Input to monitor (e.g., POLLIN)
		short revents; // Event occured, filled by the OS
};

	Call poll() — it waits until something happens.
	When poll() returns, check each fds[i].revents.
	If POLLIN → read or accept connection.
	If POLLOUT → send data.
	If POLLERR or POLLHUP → close FD.

*/

void ServerManager::acceptNewClient(int listenFd) {
	sockaddr_in clientAddr;
	socklen_t addrLen = sizeof(clientAddr);
	int clientFd = accept(listenFd, (struct sockaddr*)&clientAddr, &addrLen);

	if (clientFd < 0) {
		Logger::log(ERROR, "failed to accept new client connection: " + std::string(strerror(errno)));
		return;
	}

	// MAX CLIENT LIMIT
	if (_clientBuffers.size() >= 1024) {
		Logger::log(WARNING, "too many clients, rejecting new FD=" 
							+ std::to_string(clientFd));
		close(clientFd);
		return;
	}

	_clientState[clientFd] = {0, time(NULL)};
	char clientIP[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
	int clientPort = ntohs(clientAddr.sin_port);

	Logger::log(INFO, "accepted connection from " +
						std::string(clientIP) + ":" +
						std::to_string(clientPort));
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
		Logger::log(ERROR, "failed to set client socket non-blocking: " + std::string(strerror(errno)));
		close(clientFd);
		return;
	}

	// Add to poll and client buffer map
	_fds.push_back({ clientFd, POLLIN, 0 });
	_clientBuffers[clientFd] = "";
	// Track which listenFd spawned this client
	_clientToListenFd[clientFd] = listenFd;
	
}

void ServerManager::readFromClient(int clientFd) {
	std::string &buf = _clientBuffers[clientFd];

	// Read bytes from socket
	if (!readSocketIntoBuffer(clientFd, buf))
		return;

	if (buf.size() > MAX_HEADER_SIZE) {
		_toClose.push_back(clientFd);
		return;
	}

	// Process all complete requests (pipelining)
	size_t reqEnd;
	while (hasFullRequest(buf, reqEnd)) {

		std::string raw = extractNextRequest(buf, reqEnd);

		Logger::log(DEBUG, "full request received fd=" + std::to_string(clientFd));

		_clientState[clientFd].requestCount++;
		_clientState[clientFd].lastActivity = time(NULL);

		int listenFd = _clientToListenFd[clientFd];
		int listenPort = _portSocketMap[listenFd];

		RequestHandler h(*this, raw, clientFd);
		h.handle(listenPort);

		if (shouldCloseAfterRequest(clientFd, h)) {
			buf.clear();
			_toClose.push_back(clientFd);
			return;
		}

		// If no remaining pipelined data, stop
		if (buf.empty())
			return;
	}
}

void ServerManager::run() {
	setupSockets();

	// Add all listening sockets to poll
	for (std::map<int,int>::iterator it = _portSocketMap.begin();
		it != _portSocketMap.end(); ++it) {
		_fds.push_back({ it->first, POLLIN, 0 });
	}

	// vector::data() returns a raw pointer to the internal array of elements
	while (g_running) {
		int ret = poll(_fds.data(), _fds.size(), -1); // -1 = wait forever
		if (ret < 0) {
			if (errno == EINTR) {
				Logger::log(INFO, "poll interrupted, continuing...");
				continue;
			}
			Logger::log(ERROR, "poll() failed: " + std::string(strerror(errno)));
			break;
		}
		for (size_t i = 0; i < _fds.size(); ++i) {
			// true if there is data to read on this fd
			if (_fds[i].revents & POLLIN) {
				if (_portSocketMap.count(_fds[i].fd)) {
					acceptNewClient(_fds[i].fd);
				} else {
					readFromClient(_fds[i].fd);
				}
			}
		}
		// delayed cleanup
		if (!_toClose.empty()) {
			for (int fd : _toClose) {
				cleanupClient(fd);
			}
			_toClose.clear();
		}
	}

	for (auto& pair : _portSocketMap)
		close(pair.first); // socket fd

	for (auto& client : _clientToListenFd)
		close(client.first); // client fd

	Logger::log(INFO, "Server shutting down...");
}

bool ServerManager::readSocketIntoBuffer(int clientFd, std::string &buf) {
	char tmp[4096];
	ssize_t bytes = recv(clientFd, tmp, sizeof(tmp), 0);

	if (bytes > 0) {
		buf.append(tmp, bytes);
		return true;
	}

	if (bytes == 0) {
		Logger::log(INFO, "client disconnected");
		_toClose.push_back(clientFd);
		return false;
	}

	if (errno != EAGAIN && errno != EWOULDBLOCK) {
		Logger::log(ERROR, "read error: " + std::string(strerror(errno)));
		_toClose.push_back(clientFd);
	}
	return false;
}

bool ServerManager::hasFullRequest(const std::string &buf, size_t &reqEnd) {
	size_t headerEnd = buf.find("\r\n\r\n");
	size_t delim = 4;

	if (headerEnd == std::string::npos) {
		headerEnd = buf.find("\n\n");
		delim = 2;
		if (headerEnd == std::string::npos)
			return false;
	}

	size_t contentLength = 0;
	size_t pos = buf.find("Content-Length:");
	if (pos != std::string::npos) {
		pos += 15;
		while (buf[pos] == ' ' || buf[pos] == '\t') pos++;
		size_t end = buf.find("\r\n", pos);
		contentLength = std::atoi(buf.substr(pos, end - pos).c_str());
	}

	reqEnd = headerEnd + delim + contentLength;
	return buf.size() >= reqEnd;
}

std::string ServerManager::extractNextRequest(std::string &buf, size_t reqEnd) {
	std::string out = buf.substr(0, reqEnd);
	buf.erase(0, reqEnd);
	return out;
}

bool ServerManager::shouldCloseAfterRequest(int fd, const RequestHandler &h) {
	return (!h.keepAlive() || _clientState[fd].requestCount > 10);
}

void ServerManager::cleanupClient(int clientFd) {
	// Close OS socket
	close(clientFd);

	// Remove from all tracking structures
	_clientBuffers.erase(clientFd);
	_clientToListenFd.erase(clientFd);
	_clientState.erase(clientFd);

	for (size_t i = 0; i < _fds.size(); ++i) {
		if (_fds[i].fd == clientFd) {
			_fds.erase(_fds.begin() + i);
			break;
		}
	}

	Logger::log(DEBUG, "cleaned up client fd=" + std::to_string(clientFd));
}
