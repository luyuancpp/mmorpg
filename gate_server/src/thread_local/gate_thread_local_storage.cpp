#include "gate_thread_local_storage.h"

thread_local GateThreadLocalStorage gate_tls;

GateThreadLocalStorage::GateThreadLocalStorage()
	:sessions_(std::make_unique<ClientSessions>()),
	 login_nodes_(std::make_unique<LoginNodes>()),
	game_nodes_(std::make_unique<GameNodeList>())
{

}
