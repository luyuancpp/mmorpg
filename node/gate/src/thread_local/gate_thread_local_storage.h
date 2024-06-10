#pragma once

#include <memory>

#include "src/network/gate_session.h"

class GateThreadLocalStorage
{
public:
	GateThreadLocalStorage();
private:
};

extern thread_local GateThreadLocalStorage gate_tls;

