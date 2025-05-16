#pragma once

#include "comp/gate_session.h"
#include "util/consistent_hash_node.h"

#include "type_alias/session_id_gen.h"

using SessionList = std::unordered_map<Guid, Session>;

class ThreadLocalStorageGate
{
public:
	ThreadLocalStorageGate();
    ConsistentHashNode<uint64_t,entt::entity>& login_consistent_node() { return login_consistent_node_; }
    SessionList& sessions() { return session_list_; }
    SessionIdGenerator& session_id_gen() { return session_id_gen_; }
private:
    ConsistentHashNode<uint64_t,entt::entity> login_consistent_node_;
    SessionList session_list_;
    SessionIdGenerator session_id_gen_;
};

extern thread_local ThreadLocalStorageGate tls_gate;

