#pragma once

#include <memory>

class GateThreadLocalStorage
{
public:
	GateThreadLocalStorage();
private:
};

extern thread_local GateThreadLocalStorage gate_tls;

