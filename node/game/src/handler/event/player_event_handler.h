#pragma once

class RegisterPlayer;
class PlayerUpgrade;
class InitializePlayerComponents;

class PlayerEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void RegisterPlayerHandler(const RegisterPlayer& event);
	static void PlayerUpgradeHandler(const PlayerUpgrade& event);
	static void InitializePlayerComponentsHandler(const InitializePlayerComponents& event);
};
