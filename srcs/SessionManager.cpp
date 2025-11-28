#include "SessionManager.hpp"

SessionManager::SessionManager() {}

Session& SessionManager::getOrCreate(const std::string& id) {
	// If ID exists → return the same session
	if (!id.empty()) {
		auto it = _sessions.find(id);
		if (it != _sessions.end()) {
			it->second.touch();
			return it->second;
		}

		// ID does not exist → create session USING SAME ID
		_sessions[id] = Session(id);
		return _sessions[id];
	}

	// No ID provided → generate new one
	std::string newId = Session::generateSessionId();
	_sessions[newId] = Session(newId);
	return _sessions[newId];
}

bool SessionManager::exists(const std::string& id) const {
	return _sessions.find(id) != _sessions.end();
}

void SessionManager::remove(const std::string& id) {
	_sessions.erase(id);
}

void SessionManager::cleanupExpired() {
	for (std::map<std::string, Session>::iterator it = _sessions.begin();
		 it != _sessions.end(); ) {
		if (it->second.expired())
			_sessions.erase(it++);
		else
			++it;
	}
}

