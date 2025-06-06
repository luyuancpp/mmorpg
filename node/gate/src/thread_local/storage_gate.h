#pragma once

#include "comp/gate_session.h"
#include "util/consistent_hash_node.h"

#include "type_alias/session_id_gen.h"

using SessionList = std::unordered_map<Guid, Session>;

class ThreadLocalStorageGate
{
public:
	ThreadLocalStorageGate();
    SessionList& sessions() { return session_list_; }
    SessionIdGenerator& session_id_gen() { return session_id_gen_; }
private:
    SessionList session_list_;
    SessionIdGenerator session_id_gen_;
};

extern thread_local ThreadLocalStorageGate tls_gate;

