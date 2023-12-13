#pragma once

#include "src/network/gate_node.h"
#include "src/network/login_session.h"
#include "src/redis_client/redis_client.h"

class LoginThreadLocalStorage
{
public:
	GateNodes& gate_nodes() { return gate_nodes_; }
	LoginConnectionMap& session_list() { return session_list_; }
	MessageSyncRedisClient& redis(){return redis_;}
private:
	GateNodes gate_nodes_;
	LoginConnectionMap session_list_;
	MessageSyncRedisClient redis_;
};

extern thread_local LoginThreadLocalStorage login_tls;

