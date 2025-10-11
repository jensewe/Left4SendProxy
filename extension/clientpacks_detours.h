#ifndef _CLIENTPACKS_DETOURS_H
#define _CLIENTPACKS_DETOURS_H

#include "extension.h"

class ClientPacksDetour
{
public:
	static bool Init(IGameConfig *pGameConf);
	static void Shutdown();
	static void Clear();
	static int GetCurrentClientIndex();
	static void OnEntityHooked(int entity);
	static void OnEntityUnhooked(int entity);
	static void OnClientDisconnect(int client);
};

#endif