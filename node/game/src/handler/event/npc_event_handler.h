#pragma once

class InitializeNpcComponents;

class NpcEventHandler
{
public:
	static void Register();
	static void UnRegister();

	static void InitializeNpcComponentsHandler(const InitializeNpcComponents& event);
};
