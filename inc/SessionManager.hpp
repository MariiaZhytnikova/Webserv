#pragma once

#include "Session.hpp"

class SessionManager {
private:
	std::map<std::string, Session> _sessions;

public:
	SessionManager();
	SessionManager(const SessionManager& other) = default;
	SessionManager& operator=(const SessionManager& other) = default;
	~SessionManager() = default;

	Session& getOrCreate(const std::string& sessionId);
	bool exists(const std::string& sessionId) const;
	void remove(const std::string& sessionId);
	void cleanupExpired();
};
