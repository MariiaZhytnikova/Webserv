#include "ServerManager.hpp"
#include "RequestHandler.hpp"
#include <sys/socket.h> // for socket, bind, listen
#include <netinet/in.h> // for sockaddr_in
#include <arpa/inet.h> // for inet_pton, htons
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <algorithm>

ServerManager::ServerManager(const std::vector<Server>& servers)
	: _servers(servers) { }

ServerManager::~ServerManager() {
	for (auto& pair : _portSocketMap)
		close(pair.second);
}

const std::vector<Server>& ServerManager::getServers() const { return _servers; }

const Server& ServerManager::getServer(size_t index) const {
	if (index >= _servers.size())
		throw std::out_of_range("Server index out of range");
	return _servers[index];
}

Server& ServerManager::getServer(size_t index) {
	if (index >= _servers.size())
		throw std::out_of_range("Server index out of range");
	return _servers[index];
}

void ServerManager::setupSockets() {
	for (size_t i = 0; i < _servers.size(); ++i) {
		Server& srv = _servers[i];
		int port = srv.getListenPort();
		if (_portSocketMap.find(port) != _portSocketMap.end())
			continue; // socket for this port already exists

		// Address family: IPv4 (AF_INET)
		// SOCK_STREAM: TCP (reliable, connection-oriented)
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
			throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));

		// Allows restart immediately and reuse the same port safely
		int opt = 1;		// 0/1 -> off/on
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
			throw std::runtime_error("Failed to setsockopt: " + std::string(strerror(errno)));
		// Make socket non-blocking
		fcntl(sock, F_SETFL, O_NONBLOCK);

		// Make socket close-on-exec
		fcntl(sock, F_SETFD, FD_CLOEXEC);

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
			// IPv4
		addr.sin_family = AF_INET;
			
		addr.sin_addr.s_addr = INADDR_ANY;
			// Host TO Network Short, Converts the port from host byte order to network byte order
		addr.sin_port = htons(port);

		// accept connections on all network interfaces or srv.getHost()
		in_addr_t ip;
		if (srv.getHost() == "*" || srv.getHost().empty()) {
			ip = INADDR_ANY;
		} else {
			// Internet presentation to network”
			// converts an IP address from string to its binary form used by the OS for sockets
			if (inet_pton(AF_INET, srv.getHost().c_str(), &ip) <= 0)
				throw std::runtime_error("Invalid host: " + srv.getHost());
		}
		addr.sin_addr.s_addr = ip;

		// associates the socket with an IP address and port
		// failure occurs if the port is already in use, you don’t have permission, or the IP is invalid
		if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
			throw std::runtime_error("Failed to bind socket: " + std::string(strerror(errno)));

		// start listening for incoming TCP connections on the socket
		// failure returns -1 if the socket is invalid or not bound
		// SOMAXCONN: OS decide how many pending connections can wait
		if (listen(sock, SOMAXCONN) < 0)
			throw std::runtime_error("Failed to listen on socket: " + std::string(strerror(errno)));
		else
			std::cout << "Listening on port " << port << " (fd=" << sock << ")" << std::endl;
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


// CHECK error handling
void ServerManager::acceptNewClient(int listenFd, std::vector<pollfd>& fds) {
	sockaddr_in clientAddr;
	socklen_t addrLen = sizeof(clientAddr);
	int clientSock = accept(listenFd, (struct sockaddr*)&clientAddr, &addrLen);
	if (clientSock < 0) {
		perror("accept");
		return;
	}

	std::cout << "New connection accepted (fd=" << clientSock << ")\n";
	fcntl(clientSock, F_SETFL, O_NONBLOCK);

	// Add to poll and client buffer map
	fds.push_back({ clientSock, POLLIN, 0 });
	_clientBuffers[clientSock] = "";
	// Track which listenFd spawned this client
	_clientToListenFd[clientSock] = listenFd;
}


// CHECK error handling
void ServerManager::readFromClient(int clientFd, std::vector<pollfd>& fds, size_t index) {
	char buffer[4096];
	ssize_t bytes = read(clientFd, buffer, sizeof(buffer));

	if (bytes <= 0) {
		if (bytes == 0)
			std::cout << "Client disconnected: fd=" << clientFd << std::endl;
		else
			perror("read");

		close(clientFd);
		_clientBuffers.erase(clientFd);
		fds.erase(fds.begin() + index);
		return;
	}
	_clientBuffers[clientFd].append(buffer, bytes);
	std::cout << "Received " << bytes << " bytes from fd=" << clientFd << std::endl;

	std::cout << "\n=========== Client buffers =============\n\n";
	for (const auto& pair : _clientBuffers) {
		std::cout << pair.second << "\n";
	}
	std::cout << "========================================\n";

	if (_clientBuffers[clientFd].find("\r\n\r\n") != std::string::npos) {
		handleRequest(clientFd);
		fds.erase(fds.begin() + index); // remove client from poll
	}
}

void ServerManager::handleRequest(int clientFd) {
	std::string raw = _clientBuffers[clientFd];
	_clientBuffers.erase(clientFd);

	HttpRequest request(raw);
	int listenFd = _clientToListenFd[clientFd];
	int listenPort = _portSocketMap[listenFd];
	_clientToListenFd.erase(clientFd);

	try {
		RequestHandler handler(*this, raw, clientFd);
		handler.handle(listenPort);
	}
	catch (const std::exception& e) {
		std::cerr << "Error handling request: " << e.what() << std::endl;
		HttpResponse errorRes(500, "<h1>500 Internal Server Error</h1>");
		std::string serialized = errorRes.serialize();
		send(clientFd, serialized.c_str(), serialized.size(), 0);
		close(clientFd);
	}
}


void ServerManager::run() {
	setupSockets();

	std::vector<struct pollfd> fds;

	// Add all listening sockets to poll
	for (std::map<int,int>::iterator it = _portSocketMap.begin();
		it != _portSocketMap.end(); ++it) {
		fds.push_back({ it->first, POLLIN, 0 });
	}

	// vector::data() returns a raw pointer to the internal array of elements
	while (true) {
		int ret = poll(fds.data(), fds.size(), -1); // -1 = wait forever
		if (ret < 0) {
			throw std::runtime_error("poll failed: " + std::string(strerror(errno)));
		}
		for (size_t i = 0; i < fds.size(); ++i) {
			// true if there is data to read on this fd
			if (fds[i].revents & POLLIN) {
				if (_portSocketMap.count(fds[i].fd)) {
					std::cout << "\n=========== acceptNewClient ============\n\n";
					acceptNewClient(fds[i].fd, fds);
				} else {
					std::cout << "\n=========== readFromClient =============\n\n";
					readFromClient(fds[i].fd, fds, i);
				}
			}
		}
	}
}
