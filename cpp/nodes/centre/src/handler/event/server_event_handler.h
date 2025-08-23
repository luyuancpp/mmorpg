#pragma once
class OnConnect2CentrePbEvent;
class OnConnect2Login;
class OnServerStart;

class ServerEventHandler
{
public:
    static void Register();
    static void UnRegister();
    static void OnConnect2CentrePbEventHandler(const OnConnect2CentrePbEvent& event);
    static void OnConnect2LoginHandler(const OnConnect2Login& event);
    static void OnServerStartHandler(const OnServerStart& event);
};
