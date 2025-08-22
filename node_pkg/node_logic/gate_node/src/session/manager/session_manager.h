#pragma once

#include "session/comp/gate_session.h"
#include "util/consistent_hash_node.h"
#include "network/network_utils.h"

using SessionList = std::unordered_map<Guid, Session>;

class SessionManager
{
public:
	SessionManager() = default;

	SessionManager(const SessionManager&) = delete;
	SessionManager& operator=(const SessionManager&) = delete;

	SessionList& sessions() { return sessionList; }
	SessionIdGenerator& session_id_gen() { return sessionIdGen; }
private:
	SessionList sessionList;
	SessionIdGenerator sessionIdGen;
};

extern thread_local SessionManager tlsSessionManager;
