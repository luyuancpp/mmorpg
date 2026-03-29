#pragma once

#include "session/comp/session_info_comp.h"
#include "network/network_utils.h"

using GateSessionMap = std::unordered_map<Guid, SessionInfo>;

class SessionManager
{
public:
	SessionManager() = default;

	SessionManager(const SessionManager&) = delete;
	SessionManager& operator=(const SessionManager&) = delete;

	GateSessionMap& sessions() { return sessionList; }
	SessionIdGenerator& session_id_gen() { return sessionIdGen; }
private:
	GateSessionMap sessionList;
	SessionIdGenerator sessionIdGen;
};

extern thread_local SessionManager tlsSessionManager;
