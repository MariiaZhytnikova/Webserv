#pragma once
#include <string>
#include <map>
#include <ctime>

class Session {
private:
	std::string							_id;
	std::map<std::string, std::string>	_data;

	time_t								_lastAccess;
	time_t								_expirySeconds;

public:
	Session();
	Session(const std::string& id);
	Session(const Session& other) = default;
	Session& operator=(const Session& other) = default;
	~Session() = default;

	// -------------------- Getters --------------------
	std::string			getSession(const std::string& key) const;
	const std::string&	getId() const ;
	const std::map<std::string, std::string>& getData() const;

	// -------------------- Setters --------------------
	void				set(const std::string& key, const std::string& value);

	void touch();
	bool expired() const;
	bool has(const std::string& key) const;
	static std::string	generateSessionId();
};