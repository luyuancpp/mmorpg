#pragma once
class RegisterPlayerEvent;
class PlayerUpgradeEvent;
class InitializePlayerComponentsEvent;

class PlayerEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void RegisterPlayerEventHandler(const RegisterPlayerEvent& event);
    static void PlayerUpgradeEventHandler(const PlayerUpgradeEvent& event);
    static void InitializePlayerComponentsEventHandler(const InitializePlayerComponentsEvent& event);
};
