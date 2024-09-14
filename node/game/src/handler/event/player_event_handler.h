#pragma once

class RegisterPlayer;
class PlayerUpgrade;

class PlayerEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void RegisterPlayerHandler(const RegisterPlayer& event);
	static void PlayerUpgradeHandler(const PlayerUpgrade& event);
};
