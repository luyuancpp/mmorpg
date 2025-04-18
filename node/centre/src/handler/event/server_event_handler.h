#pragma once

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
	static void OnConnect2CentreHandler(const OnConnect2Centre& event);
	static void OnConnect2GameHandler(const OnConnect2Game& event);
	static void OnConnect2GateHandler(const OnConnect2Gate& event);
	static void OnConnect2LoginHandler(const OnConnect2Login& event);
	static void OnServerStartHandler(const OnServerStart& event);
};
