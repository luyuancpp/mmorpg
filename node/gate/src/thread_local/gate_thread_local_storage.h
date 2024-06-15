#pragma once

#include <memory>

#include "util/game_registry.h"
#include "util/consistent_hash_node.h"

class GateThreadLocalStorage
{
public:
	GateThreadLocalStorage();
    entt::registry login_node_registry;
    ConsistentHashNode<uint64_t,entt::entity>& login_consisten_node() { return login_consisten_node_; }
private:
    ConsistentHashNode<uint64_t,entt::entity> login_consisten_node_;
};

extern thread_local GateThreadLocalStorage gate_tls;

