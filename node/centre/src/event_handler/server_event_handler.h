#pragma once
#include "thread_local/thread_local_storage.h"

class OnConnect2Centre;
class OnConnect2Game;
class OnConnect2Gate;
class OnConnect2Login;
class OnServerStart;

class ServerEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void OnConnect2CentreHandler(const OnConnect2Centre& message);
	static void OnConnect2GameHandler(const OnConnect2Game& message);
	static void OnConnect2GateHandler(const OnConnect2Gate& message);
	static void OnConnect2LoginHandler(const OnConnect2Login& message);
	static void OnServerStartHandler(const OnServerStart& message);
};
