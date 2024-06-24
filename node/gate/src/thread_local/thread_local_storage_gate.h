#pragma once

#include <memory>

#include "util/game_registry.h"
#include "util/consistent_hash_node.h"
#include "network/gate_session.h"
#include "util/snow_flake.h"

using SessionList = std::unordered_map<Guid, Session>;
using SessionIdGen = NodeSequence<uint64_t, 32>;

class ThreadLocalStorageGate
{
public:
	ThreadLocalStorageGate();
    entt::registry login_node_registry;
    ConsistentHashNode<uint64_t,entt::entity>& login_consisten_node() { return login_consisten_node_; }
    SessionList& sessions() { return session_list_; }
    SessionIdGen& session_id_gen() { return sesseion_id_gen_; }
private:
    ConsistentHashNode<uint64_t,entt::entity> login_consisten_node_;
    SessionList session_list_;
    SessionIdGen sesseion_id_gen_;
};

extern thread_local ThreadLocalStorageGate tls_gate;

