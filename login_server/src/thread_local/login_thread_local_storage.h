#pragma once

#include "src/network/gate_node.h"
#include "src/network/session.h"

class LoginThreadLocalStorage
{
public:
	GateNodes& gate_nodes() { return gate_nodes_; }
	ConnectionEntityMap& session_list() { return session_list_; }
private:
	GateNodes gate_nodes_;
	ConnectionEntityMap session_list_;
};

extern thread_local LoginThreadLocalStorage login_tls;

