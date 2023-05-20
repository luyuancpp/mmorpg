#pragma once

#include "src/network/gate_node.h"

class LoginThreadLocalStorage
{
public:
	GateNodes& gate_nodes() { return gate_nodes_; }
private:
	GateNodes gate_nodes_;
};

extern thread_local LoginThreadLocalStorage login_tls;

