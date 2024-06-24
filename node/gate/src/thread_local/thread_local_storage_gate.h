#pragma once

#include <memory>

#include "util/game_registry.h"
#include "util/consistent_hash_node.h"
#include "network/gate_session.h"

using SessionList = std::unordered_map<Guid, Session>;

class ThreadLocalStorageGate
{
public:
	ThreadLocalStorageGate();
    entt::registry login_node_registry;
    ConsistentHashNode<uint64_t,entt::entity>& login_consisten_node() { return login_consisten_node_; }
    SessionList& sessions() { return session_list_; }
private:
    ConsistentHashNode<uint64_t,entt::entity> login_consisten_node_;
    SessionList session_list_;
};

extern thread_local ThreadLocalStorageGate tls_gate;

