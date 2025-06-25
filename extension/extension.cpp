/**
 * vim: set ts=4 :
 * =============================================================================
 * SendVar Proxy Manager
 * Copyright (C) 2011-2019 Afronanny & AlliedModders community.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#ifdef _WIN32
#undef GetProp
#ifdef _WIN64
	#define PLATFORM_x64
#else
	#define PLATFORM_x32
#endif
#elif defined __linux__
	#if defined __x86_64__
		#define PLATFORM_x64
	#else
		#define PLATFORM_x32
	#endif
#endif

#include "extension.h"
#include "interfaceimpl.h"
#include "natives.h"

//path: hl2sdk-<your sdk here>/public/<include>.h, "../public/" included to prevent compile errors due wrong directory scanning by compiler on my computer, and I'm too lazy to find where I can change that =D
#include <../public/iserver.h>
#include <../public/iclient.h>

SH_DECL_HOOK1_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, false, edict_t *);
SH_DECL_HOOK1_void(IServerGameDLL, GameFrame, SH_NOATTRIB, false, bool);
SH_DECL_HOOK0(IServer, GetClientCount, const, false, int);

class CGameClient;
class CFrameSnapshot;
class CGlobalEntityList;

CGameClient * g_pCurrentGameClientPtr = nullptr;
int g_iCurrentClientIndexInLoop = -1; //used for optimization
bool g_bCurrentGameClientCallFwd = false;
bool g_bCallingForNullClients = false;
bool g_bFirstTimeCalled = true;
bool g_bSVComputePacksDone = false;
IServer * g_pIServer = nullptr;

SendProxyManager g_SendProxyManager;
SendProxyManagerInterfaceImpl g_pMyInterface;
SMEXT_LINK(&g_SendProxyManager);

CThreadFastMutex g_WorkMutex;

CUtlVector<SendPropHook> g_Hooks;
CUtlVector<SendPropHookGamerules> g_HooksGamerules;
CUtlVector<PropChangeHook> g_ChangeHooks;
CUtlVector<PropChangeHookGamerules> g_ChangeHooksGamerules;

CUtlVector<edict_t *> g_vHookedEdicts;

IServerGameEnts * gameents = nullptr;
IServerGameClients * gameclients = nullptr;
ISDKTools * g_pSDKTools = nullptr;
ISDKHooks * g_pSDKHooks = nullptr;
IGameConfig * g_pGameConf = nullptr;
IGameConfig * g_pGameConfSDKTools = nullptr;

ConVar * sv_parallel_packentities = nullptr;
ConVar * sv_parallel_sendsnapshot = nullptr;

edict_t * g_pGameRulesProxyEdict = nullptr;
int g_iGameRulesProxyIndex = -1;
PackedEntityHandle_t g_PlayersPackedGameRules[SM_MAXPLAYERS] = {INVALID_PACKED_ENTITY_HANDLE};
void * g_pGameRules = nullptr;
bool g_bShouldChangeGameRulesState = false;
bool g_bSendSnapshots = false;

bool g_bEdictChanged[MAX_EDICTS] = {false};

CGlobalVars * g_pGlobals = nullptr;

static CBaseEntity * FindEntityByServerClassname(int, const char *);
static void CallChangeCallbacks(PropChangeHook * pInfo, void * pOldValue, void * pNewValue);
static void CallChangeGamerulesCallbacks(PropChangeHookGamerules * pInfo, void * pOldValue, void * pNewValue);

const char * g_szGameRulesProxy;

static CDetour *CFrameSnapshotManager_DTR_UsePreviouslySentPacket = nullptr;
static CDetour *CFrameSnapshotManager_DTR_CreatePackedEntity = nullptr;
static CDetour *CFrameSnapshotManager_DTR_GetPreviouslySentPacket = nullptr;
static CDetour *CGameServer_DTR_SendClientMessages = nullptr;
static CDetour *CGameClient_DTR_ShouldSendMessages = nullptr;
static CDetour *DTR_SV_ComputeClientPacks = nullptr;

static void *pFnUsePreviouslySentPacket = nullptr;
static void *pFnCreatePackedEntity = nullptr;
static void *pFnGetPreviouslySentPacket = nullptr;
static void *pFnSendClientMessages = nullptr;
static void *pFnShouldSendMessages = nullptr;
static void *pFnSV_ComputeClientPacks = nullptr;

//detours

/*Call stack:
	...
	1. CGameServer::SendClientMessages //function we hooking to send props individually for each client
	2. SV_ComputeClientPacks //function we hooking to set edicts state and to know, need we call callbacks or not, but not in csgo
	3. PackEntities_Normal //if we in multiplayer
	4. SV_PackEntity //also we can hook this instead hooking ProxyFn, but there no reason to do that
	5. SendTable_Encode
	6. SendTable_EncodeProp //here the ProxyFn will be called
	7. ProxyFn //here our callbacks is called
*/

#ifndef DEBUG
// #define _FORCE_DEBUG

#ifdef _FORCE_DEBUG
#define DEBUG
#endif

#endif // #ifndef DEBUG

DETOUR_DECL_MEMBER3(CFrameSnapshotManager_UsePreviouslySentPacket, bool, CFrameSnapshot*, pSnapshot, int, entity, int, entSerialNumber)
{
	if (g_iCurrentClientIndexInLoop == -1
	 || !g_bCurrentGameClientCallFwd
	 || entity != g_iGameRulesProxyIndex)
	{
		return DETOUR_MEMBER_CALL(CFrameSnapshotManager_UsePreviouslySentPacket)(pSnapshot, entity, entSerialNumber);
	}

	if (g_PlayersPackedGameRules[g_iCurrentClientIndexInLoop] == INVALID_PACKED_ENTITY_HANDLE)
		return false;

	CFrameSnapshotManager *framesnapshotmanager = (CFrameSnapshotManager *)this;
	framesnapshotmanager->m_pLastPackedData[entity] = g_PlayersPackedGameRules[g_iCurrentClientIndexInLoop];
	return DETOUR_MEMBER_CALL(CFrameSnapshotManager_UsePreviouslySentPacket)(pSnapshot, entity, entSerialNumber);
}

DETOUR_DECL_MEMBER2(CFrameSnapshotManager_GetPreviouslySentPacket, PackedEntity*, int, entity, int, entSerialNumber)
{
	if (g_iCurrentClientIndexInLoop == -1
	 || !g_bCurrentGameClientCallFwd
	 || entity != g_iGameRulesProxyIndex)
	{
		return DETOUR_MEMBER_CALL(CFrameSnapshotManager_GetPreviouslySentPacket)(entity, entSerialNumber);
	}

	CFrameSnapshotManager *framesnapshotmanager = (CFrameSnapshotManager *)this;
	
#ifdef DEBUG
	char buffer[128];
	smutils->Format(buffer, sizeof(buffer), "GetPreviouslySentPacket (%d / %d)", framesnapshotmanager->m_pLastPackedData[entity], g_PlayersPackedGameRules[g_iCurrentClientIndexInLoop]);
	gamehelpers->TextMsg(g_iCurrentClientIndexInLoop+1, 3, buffer);
#endif

	framesnapshotmanager->m_pLastPackedData[entity] = g_PlayersPackedGameRules[g_iCurrentClientIndexInLoop];
	return DETOUR_MEMBER_CALL(CFrameSnapshotManager_GetPreviouslySentPacket)(entity, entSerialNumber);
}

DETOUR_DECL_MEMBER2(CFrameSnapshotManager_CreatePackedEntity, PackedEntity*, CFrameSnapshot*, pSnapshot, int, entity)
{
	if (g_iCurrentClientIndexInLoop == -1
	 || !g_bCurrentGameClientCallFwd
	 || entity != g_iGameRulesProxyIndex)
	{
		return DETOUR_MEMBER_CALL(CFrameSnapshotManager_CreatePackedEntity)(pSnapshot, entity);
	}

	CFrameSnapshotManager *framesnapshotmanager = (CFrameSnapshotManager *)this;
	PackedEntityHandle_t origHandle = framesnapshotmanager->m_pLastPackedData[entity];

	if (g_PlayersPackedGameRules[g_iCurrentClientIndexInLoop] != INVALID_PACKED_ENTITY_HANDLE)
		framesnapshotmanager->m_pLastPackedData[entity] = g_PlayersPackedGameRules[g_iCurrentClientIndexInLoop];

	PackedEntity *result = DETOUR_MEMBER_CALL(CFrameSnapshotManager_CreatePackedEntity)(pSnapshot, entity);
	g_PlayersPackedGameRules[g_iCurrentClientIndexInLoop] = framesnapshotmanager->m_pLastPackedData[entity];

#ifdef DEBUG
	char buffer[128];
	smutils->Format(buffer, sizeof(buffer), "CreatePackedEntity (%d / %d / %d)", origHandle, g_PlayersPackedGameRules[g_iCurrentClientIndexInLoop], framesnapshotmanager->m_pLastPackedData[entity]);
	gamehelpers->TextMsg(g_iCurrentClientIndexInLoop+1, 3, buffer);
#endif

	return result;
}

// DETOUR_DECL_MEMBER1(CFrameSnapshotManager_RemoveEntityReference, void, PackedEntityHandle_t, handle)
// {
// 	CFrameSnapshotManager *framesnapshotmanager = (CFrameSnapshotManager *)this;

// 	PackedEntity *packedEntity = framesnapshotmanager->m_PackedEntities[handle];
// 	if ( packedEntity->m_ReferenceCount <= 1)
// 	{
// 		for (int i = 0; i < (sizeof(g_PlayersPackedGameRules) / sizeof(g_PlayersPackedGameRules[0])); ++i)
// 		{
// 			if (g_PlayersPackedGameRules[i] == handle)
// 			{
// 				g_PlayersPackedGameRules[i] = INVALID_PACKED_ENTITY_HANDLE;

// 			#ifdef DEBUG
// 				char buffer[128];
// 				for (int client = 1; client <= playerhelpers->GetMaxClients(); client++)
// 				{
// 					IGamePlayer *plr = playerhelpers->GetGamePlayer(client);
// 					if (plr && plr->IsInGame() && !plr->IsFakeClient())
// 					{
// 						smutils->Format(buffer, sizeof(buffer), "RemoveEntityReference: (%d / %d)", handle, i + 1);
// 						gamehelpers->TextMsg(client, 3, buffer);
// 					}
// 				}
// 			#endif
// 			}
// 		}
// 	}

// 	DETOUR_MEMBER_CALL(CFrameSnapshotManager_RemoveEntityReference)(handle);
// }

#ifdef _FORCE_DEBUG
#undef DEBUG
#endif

DETOUR_DECL_MEMBER1(CGameServer_SendClientMessages, void, bool, bSendSnapshots)
{
	if (!bSendSnapshots)
	{
		g_bSendSnapshots = false;
		return DETOUR_MEMBER_CALL(CGameServer_SendClientMessages)(false); //if so, we do not interested in this call
	}
	else
	{
		g_bSendSnapshots = true;
	}

	if (!g_pIServer && g_pSDKTools)
		g_pIServer = g_pSDKTools->GetIServer();

	if (!g_pIServer)
		return DETOUR_MEMBER_CALL(CGameServer_SendClientMessages)(true); //if so, we should stop to process this function! See below

	if (g_bFirstTimeCalled)
	{
#ifdef _WIN32
		//HACK, don't delete this, or server will be crashed on start!
		g_pIServer->GetClientCount();
#endif
		SH_ADD_HOOK(IServer, GetClientCount, g_pIServer, SH_MEMBER(&g_SendProxyManager, &SendProxyManager::GetClientCount), false);
		g_bFirstTimeCalled = false;
	}

	for (int i = 0; i < MAX_EDICTS; ++i)
	{
		g_bEdictChanged[i] = false;

		edict_t *edict = gamehelpers->EdictOfIndex(i);
		if (!edict || !edict->GetUnknown() || edict->IsFree())
			continue;

		if (i > 0 && i <= playerhelpers->GetMaxClients())
		{
			if (!g_pIServer->GetClient(i-1)->IsActive())
				continue;
		}

		if (!edict->HasStateChanged())
			continue;

		g_bEdictChanged[i] = true;
	}

	bool bCalledForNullIClientsThisTime = false;
	for (int iClients = 1; iClients <= playerhelpers->GetMaxClients(); iClients++)
	{
		IGamePlayer * pPlayer = playerhelpers->GetGamePlayer(iClients);
		bool bFake = (pPlayer->IsFakeClient() && !(pPlayer->IsSourceTV()
#if SOURCE_ENGINE != SE_CSGO
		|| pPlayer->IsReplay()
#endif
		));

		volatile IClient * pClient = nullptr; //volatile used to prevent optimizations here for some reason
		if (!pPlayer->IsConnected() || bFake || (pClient = g_pIServer->GetClient(iClients - 1)) == nullptr)
		{
			if (!bCalledForNullIClientsThisTime && !g_bCallingForNullClients)
			{
				g_bCurrentGameClientCallFwd = false;
				g_bCallingForNullClients = true;
				DETOUR_MEMBER_CALL(CGameServer_SendClientMessages)(true);
				g_bCallingForNullClients = false;
			}

			bCalledForNullIClientsThisTime = true;
			continue;
		}

		if (!pPlayer->IsInGame() || bFake) //We should call SV_ComputeClientPacks, but shouldn't call forwards!
		{
			g_bCurrentGameClientCallFwd = false;
		}
		else
		{
			g_bCurrentGameClientCallFwd = true;
		}
			
		g_pCurrentGameClientPtr = (CGameClient *)((char *)pClient - 4);
		g_iCurrentClientIndexInLoop = iClients - 1;
		DETOUR_MEMBER_CALL(CGameServer_SendClientMessages)(true);
	}

	g_bCurrentGameClientCallFwd = false;
	g_iCurrentClientIndexInLoop = -1;
	g_bShouldChangeGameRulesState = false;
}

DETOUR_DECL_MEMBER0(CGameClient_ShouldSendMessages, bool)
{
	if (!g_bSendSnapshots)
		return DETOUR_MEMBER_CALL(CGameClient_ShouldSendMessages)();

	if (g_bCallingForNullClients)
	{
		IClient * pClient = (IClient *)((char *)this + 4);
#if SOURCE_ENGINE == SE_TF2
		//don't remove this code
		int iUserID = pClient->GetUserID();
		IGamePlayer * pPlayer = playerhelpers->GetGamePlayer(pClient->GetPlayerSlot() + 1);
		if (pPlayer->GetUserId() != iUserID) //if so, there something went wrong, check this now!
#endif
		{
			if (pClient->IsHLTV()
#if SOURCE_ENGINE == SE_TF2
			|| pClient->IsReplay()
#endif
			|| (pClient->IsConnected() && !pClient->IsActive()))
				return true; //Also we need to allow connect for inactivated clients, sourcetv & replay
		}
		return false;
	}
	bool bOriginalResult = DETOUR_MEMBER_CALL(CGameClient_ShouldSendMessages)();
	if (!bOriginalResult)
		return false;

	if ((CGameClient *)this == g_pCurrentGameClientPtr)
		return true;

#if defined PLATFORM_x32
	else
	{
		volatile int iToSet = g_iCurrentClientIndexInLoop - 1;
#if SOURCE_ENGINE == SE_TF2
#ifdef _WIN32
		//some little trick to deceive msvc compiler
		__asm _emit 0x5F
		__asm _emit 0x5E
		__asm push edx
		__asm mov edx, iToSet
		__asm _emit 0x3B
		__asm _emit 0xF2
		__asm jge CompFailed
		__asm _emit 0x8B
		__asm _emit 0xF2
		__asm CompFailed:
		__asm pop edx
		__asm _emit 0x56
		__asm _emit 0x57
#elif defined __linux__
		volatile int iTemp;
		asm volatile("movl %%esi, %0" : "=g" (iTemp));
		if (iTemp < iToSet)
			asm volatile(
				"movl %0, %%esi\n\t"
				"movl %%esi, %%edx\n\t"
				"addl $84, %%esp\n\t"
				"popl %%esi\n\t"
				"pushl %%edx\n\t"
				"subl $84, %%esp\n\t"
				: : "g" (iToSet) : "%edx");
#endif
#elif SOURCE_ENGINE == SE_CSGO
#ifdef _WIN32
		volatile int iEax, iEdi, iEsi;
			//save registers
		__asm mov iEdi, edi
		__asm mov iEsi, esi
		__asm mov iEax, eax
		__asm mov eax, ebp
			//load stack ptr
			//we need to pop esi and edi to pop ebp register, we don't care about values in these, we also will use them as variables
		__asm pop esi
		__asm pop edi
		__asm mov edi, iToSet
		__asm mov esp, ebp
		__asm pop ebp
			//load needed info and compare
		__asm mov esi, [ebp-0x7F8] //0x7F8 is an offset of loop variable
		__asm cmp esi, edi
		__asm jge CompFailed
			//good, store our value
		__asm mov [ebp-0x7F8], edi
		__asm CompFailed:
			//push old and restore original registers
		__asm push ebp
		__asm mov ebp, eax
		__asm mov esp, ebp
		__asm sub esp, 0x10
		__asm mov esi, iEsi
		__asm mov edi, iEdi
		__asm mov eax, iEax
		__asm push edi
		__asm push esi
#elif defined __linux__
		volatile int iTemp;
		//we don't need to clubber edi register here, some low level shit
		asm volatile("movl %%edi, %0" : "=g" (iTemp));
		if (iTemp < iToSet)
			asm volatile("movl %0, %%edi" : : "g" (iToSet));
#endif
#endif
	}
#endif
	return false;
}

#if defined __linux__
void __attribute__((__cdecl__)) SV_ComputeClientPacks_ActualCall(int iClientCount, CGameClient ** pClients, CFrameSnapshot * pSnapShot);
#else
void __cdecl SV_ComputeClientPacks_ActualCall(int iClientCount, CGameClient ** pClients, CFrameSnapshot * pSnapShot);
#endif

//the better idea rewrite it with __declspec(naked) for csgo or use __stdcall function as main callback instead of this
DETOUR_DECL_STATIC3(SV_ComputeClientPacks, void, int, iClientCount, CGameClient **, pClients, CFrameSnapshot *, pSnapShot)
{

#if defined _WIN32 && SOURCE_ENGINE == SE_CSGO
	//so, here it is __userpurge call, we need manually get our arguments
	__asm mov iClientCount, ecx
	__asm mov pClients, edx
	__asm mov pSnapShot, ebx
#endif

	g_bSVComputePacksDone = false;
	if (!iClientCount || !g_bSendSnapshots || pClients[0] != g_pCurrentGameClientPtr)
		return SV_ComputeClientPacks_ActualCall(iClientCount, pClients, pSnapShot);

	IClient * pClient = (IClient *)((char *)pClients[0] + 4);
	int iClient = pClient->GetPlayerSlot();

	if (g_iCurrentClientIndexInLoop != iClient)
		return SV_ComputeClientPacks_ActualCall(iClientCount, pClients, pSnapShot);

	//Also here we can change actual values for each client! But for what?
	//Just mark all hooked edicts as changed to bypass check in SV_PackEntity!
	for (int i = 0; i < g_vHookedEdicts.Count(); i++)
	{
		edict_t * pEdict = g_vHookedEdicts[i];
		if (pEdict && !pEdict->IsFree() && pEdict->GetUnknown() && !(pEdict->m_fStateFlags & FL_EDICT_CHANGED))
			pEdict->m_fStateFlags |= FL_EDICT_CHANGED;
	}

	for (int i = 0; i < MAX_EDICTS; ++i)
	{
		if (!g_bEdictChanged[i])
			continue;

		edict_t *pEdict = gamehelpers->EdictOfIndex(i);
		if (pEdict && !(pEdict->m_fStateFlags & FL_EDICT_CHANGED))
			pEdict->m_fStateFlags |= FL_EDICT_CHANGED;
	}

	if (g_bShouldChangeGameRulesState && g_pGameRulesProxyEdict)
	{
		if (!(g_pGameRulesProxyEdict->m_fStateFlags & FL_EDICT_CHANGED))
			g_pGameRulesProxyEdict->m_fStateFlags |= FL_EDICT_CHANGED;
	}

	if (g_bCurrentGameClientCallFwd)
		g_bSVComputePacksDone = true;

	return SV_ComputeClientPacks_ActualCall(iClientCount, pClients, pSnapShot);
}

#if defined _WIN32 && SOURCE_ENGINE == SE_CSGO
__declspec(naked) void __cdecl SV_ComputeClientPacks_ActualCall(int iClientCount, CGameClient ** pClients, CFrameSnapshot * pSnapShot)
{
	//we do not use ebp here
	__asm mov edx, pClients //we don't care about values in edx & ecx
	__asm mov ecx, iClientCount
	__asm mov ebx, pSnapShot
	__asm push ebx
	__asm call SV_ComputeClientPacks_Actual
	__asm add esp, 0x4 //restore our stack
	__asm retn
}
#else
#ifdef __linux__
void __attribute__((__cdecl__)) SV_ComputeClientPacks_ActualCall(int iClientCount, CGameClient ** pClients, CFrameSnapshot * pSnapShot)
#else
void __cdecl SV_ComputeClientPacks_ActualCall(int iClientCount, CGameClient ** pClients, CFrameSnapshot * pSnapShot)
#endif
{
	return DETOUR_STATIC_CALL(SV_ComputeClientPacks)(iClientCount, pClients, pSnapShot);
}
#endif

//hooks

void SendProxyManager::OnEntityDestroyed(CBaseEntity* pEnt)
{
	int idx = gamehelpers->EntityToBCompatRef(pEnt);
	for (int i = 0; i < g_Hooks.Count(); i++)
	{
		if (g_Hooks[i].objectID == idx)
		{
			g_SendProxyManager.UnhookProxy(i);
		}
	}

	for (int i = 0; i < g_ChangeHooks.Count(); i++)
	{
		if (g_ChangeHooks[i].objectID == idx)
			g_ChangeHooks.Remove(i--);
	}
}

void Hook_ClientDisconnect(edict_t * pEnt)
{
	for (int i = 0; i < g_Hooks.Count(); i++)
	{
		if (g_Hooks[i].objectID == gamehelpers->IndexOfEdict(pEnt))
			g_SendProxyManager.UnhookProxy(i);
	}

	for (int i = 0; i < g_ChangeHooks.Count(); i++)
	{
		if (g_ChangeHooks[i].objectID == gamehelpers->IndexOfEdict(pEnt))
			g_ChangeHooks.Remove(i--);
	}

	if (gamehelpers->IndexOfEdict(pEnt) != -1)
		g_PlayersPackedGameRules[gamehelpers->IndexOfEdict(pEnt)] = INVALID_PACKED_ENTITY_HANDLE;

	RETURN_META(MRES_IGNORED);
}

void Hook_GameFrame(bool simulating)
{
	if (simulating)
	{
		for (int i = 0; i < g_ChangeHooks.Count(); i++)
		{
			switch(g_ChangeHooks[i].PropType)
			{
				case PropType::Prop_Int:
				{
					edict_t * pEnt = gamehelpers->EdictOfIndex(g_ChangeHooks[i].objectID);
					if (!pEnt)
						break;

					CBaseEntity * pEntity = gameents->EdictToBaseEntity(pEnt);
					if (!pEntity)
	  					break;

					int iCurrent = *(int *)((unsigned char *)pEntity + g_ChangeHooks[i].Offset);
					if (iCurrent != g_ChangeHooks[i].iLastValue)
					{
						CallChangeCallbacks(&g_ChangeHooks[i], (void *)&g_ChangeHooks[i].iLastValue, (void *)&iCurrent);
						g_ChangeHooks[i].iLastValue = iCurrent;
					}

					break;
				}

				case PropType::Prop_Float:
				{
					edict_t * pEnt = gamehelpers->EdictOfIndex(g_ChangeHooks[i].objectID);
					if (!pEnt)
						break;

					CBaseEntity * pEntity = gameents->EdictToBaseEntity(pEnt);
					if (!pEntity)
	  					break;

					float flCurrent = *(float *)((unsigned char *)pEntity + g_ChangeHooks[i].Offset);
					if (flCurrent != g_ChangeHooks[i].flLastValue)
					{
						CallChangeCallbacks(&g_ChangeHooks[i], (void *)&g_ChangeHooks[i].flLastValue, (void *)&flCurrent);
						g_ChangeHooks[i].flLastValue = flCurrent;
					}
					
					break;
				}

				case PropType::Prop_String:
				{
					edict_t * pEnt = gamehelpers->EdictOfIndex(g_ChangeHooks[i].objectID);
					if (!pEnt)
						break;

					CBaseEntity * pEntity = gameents->EdictToBaseEntity(pEnt);
					if (!pEntity)
	  					break;

					const char * szCurrent = (const char *)((unsigned char *)pEntity + g_ChangeHooks[i].Offset);
					if (strcmp(szCurrent, g_ChangeHooks[i].cLastValue) != 0)
					{
						CallChangeCallbacks(&g_ChangeHooks[i], (void *)g_ChangeHooks[i].cLastValue, (void *)szCurrent);
						memset(g_ChangeHooks[i].cLastValue, 0, sizeof(g_ChangeHooks[i].cLastValue));
						strncpynull(g_ChangeHooks[i].cLastValue, szCurrent, sizeof(g_ChangeHooks[i].cLastValue));
					}

					break;
				}

				case PropType::Prop_Vector:
				{
					edict_t * pEnt = gamehelpers->EdictOfIndex(g_ChangeHooks[i].objectID);
					if (!pEnt)
						break;

					CBaseEntity * pEntity = gameents->EdictToBaseEntity(pEnt);
					if (!pEntity)
	  					break;

					Vector * pVec = (Vector *)((unsigned char *)pEntity + g_ChangeHooks[i].Offset);
					if (*pVec != g_ChangeHooks[i].vecLastValue)
					{
						CallChangeCallbacks(&g_ChangeHooks[i], (void *)&g_ChangeHooks[i].vecLastValue, (void *)pVec);
						g_ChangeHooks[i].vecLastValue = *pVec;
					}

					break;
				}

				default: rootconsole->ConsolePrint("%s: SendProxy report: Unknown prop type (%s).", __func__, g_ChangeHooks[i].pVar->GetName());
			}
		}

		if (!g_pGameRules && g_pSDKTools)
		{
			g_pGameRules = g_pSDKTools->GetGameRules();
			if (!g_pGameRules)
			{
				g_pSM->LogError(myself, "CRITICAL ERROR: Could not get gamerules pointer!");
				return;
			}
		}

		//Gamerules hooks
		for (int i = 0; i < g_ChangeHooksGamerules.Count(); i++)
		{
			switch(g_ChangeHooksGamerules[i].PropType)
			{
				case PropType::Prop_Int:
				{
					int iCurrent = *(int *)((unsigned char *)g_pGameRules + g_ChangeHooksGamerules[i].Offset);
					if (iCurrent != g_ChangeHooksGamerules[i].iLastValue)
					{
						CallChangeGamerulesCallbacks(&g_ChangeHooksGamerules[i], (void *)&g_ChangeHooksGamerules[i].iLastValue, (void *)&iCurrent);
						g_ChangeHooksGamerules[i].iLastValue = iCurrent;
					}

					break;
				}

				case PropType::Prop_Float:
				{
					float flCurrent = *(float *)((unsigned char *)g_pGameRules + g_ChangeHooksGamerules[i].Offset);
					if (flCurrent != g_ChangeHooksGamerules[i].flLastValue)
					{
						CallChangeGamerulesCallbacks(&g_ChangeHooksGamerules[i], (void *)&g_ChangeHooksGamerules[i].flLastValue, (void *)&flCurrent);
						g_ChangeHooksGamerules[i].flLastValue = flCurrent;
					}

					break;
				}

				case PropType::Prop_String:
				{
					const char * szCurrent = (const char *)((unsigned char *)g_pGameRules + g_ChangeHooksGamerules[i].Offset);
					if (strcmp(szCurrent, g_ChangeHooksGamerules[i].cLastValue) != 0)
					{
						CallChangeGamerulesCallbacks(&g_ChangeHooksGamerules[i], (void *)g_ChangeHooksGamerules[i].cLastValue, (void *)szCurrent);
						memset(g_ChangeHooks[i].cLastValue, 0, sizeof(g_ChangeHooks[i].cLastValue));
						strncpynull(g_ChangeHooks[i].cLastValue, szCurrent, sizeof(g_ChangeHooks[i].cLastValue));
					}

					break;
				}

				case PropType::Prop_Vector:
				{
					Vector * pVec = (Vector *)((unsigned char *)g_pGameRules + g_ChangeHooksGamerules[i].Offset);
					if (*pVec != g_ChangeHooksGamerules[i].vecLastValue)
					{
						CallChangeGamerulesCallbacks(&g_ChangeHooksGamerules[i], (void *)&g_ChangeHooksGamerules[i].vecLastValue, (void *)pVec);
						g_ChangeHooksGamerules[i].vecLastValue = *pVec;
					}

					break;
				}

				default: rootconsole->ConsolePrint("%s: SendProxy report: Unknown prop type (%s).", __func__, g_ChangeHooksGamerules[i].pVar->GetName());
			}
		}
	}

	RETURN_META(MRES_IGNORED);
}

int SendProxyManager::GetClientCount() const
{
	if (g_iCurrentClientIndexInLoop != -1)
		RETURN_META_VALUE(MRES_SUPERCEDE, g_iCurrentClientIndexInLoop + 1);

	RETURN_META_VALUE(MRES_IGNORED, 0/*META_RESULT_ORIG_RET(int)*/);
}

//main sm class implementation

bool SendProxyManager::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	char conf_error[255];
	if (!gameconfs->LoadGameConfigFile("sdktools.games", &g_pGameConfSDKTools, conf_error, sizeof(conf_error)))
	{
		if (conf_error[0])
			snprintf(error, maxlength, "Could not read config file sdktools.games.txt: %s", conf_error);

		return false;
	}
	
	g_szGameRulesProxy = g_pGameConfSDKTools->GetKeyValue("GameRulesProxy");
	
	if (!gameconfs->LoadGameConfigFile("sendproxy", &g_pGameConf, conf_error, sizeof(conf_error)))
	{
		if (conf_error[0])
			snprintf(error, maxlength, "Could not read config file sendproxy.txt: %s", conf_error);

		return false;
	}
	
	CDetourManager::Init(smutils->GetScriptingEngine(), nullptr);
	
	bool bDetoursInited = false;
	static struct {
		const char *name;
		void *pFnCallback;
	} detours[] = {
		{"CGameServer::SendClientMessages", pFnSendClientMessages},
		{"CGameClient::ShouldSendMessages", pFnShouldSendMessages},
		{"CFrameSnapshotManager::UsePreviouslySentPacket", pFnUsePreviouslySentPacket},
		{"CFrameSnapshotManager::CreatePackedEntity", pFnCreatePackedEntity},
		{"CFrameSnapshotManager::GetPreviouslySentPacket", pFnGetPreviouslySentPacket},
		{"SV_ComputeClientPacks", pFnSV_ComputeClientPacks}
	};

	for (auto &i : detours) {
		if (!g_pGameConf->GetMemSig(i.name, &i.pFnCallback)) {
			g_pSM->LogError(myself, "Signature for %s not found in gamedata", i.name);
			return false;
		}

		if (i.pFnCallback == nullptr) {
			smutils->LogError(myself, "Sigscan for %s is null.", i.name);
			return false;
		}
	}

	CGameServer_DTR_SendClientMessages = DETOUR_CREATE_MEMBER(CGameServer_SendClientMessages, pFnSendClientMessages);
	if (!CGameServer_DTR_SendClientMessages)
	{
		smutils->LogError(myself, "Could not create detour for CGameServer::SendClientMessages.");
		return false;
	}

	CGameClient_DTR_ShouldSendMessages = DETOUR_CREATE_MEMBER(CGameClient_ShouldSendMessages, pFnShouldSendMessages);
	if (!CGameClient_DTR_ShouldSendMessages)
	{
		smutils->LogError(myself, "Could not create detour for CGameClient::ShouldSendMessages.");
		return false;
	}

	CFrameSnapshotManager_DTR_UsePreviouslySentPacket = DETOUR_CREATE_MEMBER(CFrameSnapshotManager_UsePreviouslySentPacket, pFnUsePreviouslySentPacket);
	if (!CFrameSnapshotManager_DTR_UsePreviouslySentPacket)
	{
		smutils->LogError(myself, "Could not create detour for CFrameSnapshotManager::UsePreviouslySentPacket.");
		return false;
	}

	CFrameSnapshotManager_DTR_CreatePackedEntity = DETOUR_CREATE_MEMBER(CFrameSnapshotManager_CreatePackedEntity, pFnCreatePackedEntity);
	if (!CFrameSnapshotManager_DTR_CreatePackedEntity)
	{
		smutils->LogError(myself, "Could not create detour for CFrameSnapshotManager::CreatePackedEntity.");
		return false;
	}

	CFrameSnapshotManager_DTR_GetPreviouslySentPacket = DETOUR_CREATE_MEMBER(CFrameSnapshotManager_GetPreviouslySentPacket, pFnGetPreviouslySentPacket);
	if (!CFrameSnapshotManager_DTR_GetPreviouslySentPacket)
	{
		smutils->LogError(myself, "Could not create detour for CFrameSnapshotManager::GetPreviouslySentPacket.");
		return false;
	}

	DTR_SV_ComputeClientPacks = DETOUR_CREATE_STATIC(SV_ComputeClientPacks, pFnSV_ComputeClientPacks);
	if (!DTR_SV_ComputeClientPacks)
	{
		smutils->LogError(myself, "Could not create detour for SV_ComputeClientPacks.");
		return false;
	}

	CGameServer_DTR_SendClientMessages->EnableDetour();
	CGameClient_DTR_ShouldSendMessages->EnableDetour();
	CFrameSnapshotManager_DTR_UsePreviouslySentPacket->EnableDetour();
	CFrameSnapshotManager_DTR_CreatePackedEntity->EnableDetour();
	CFrameSnapshotManager_DTR_GetPreviouslySentPacket->EnableDetour();
	DTR_SV_ComputeClientPacks->EnableDetour();

	if (late) //if we loaded late, we need manually to call that
		OnCoreMapStart(nullptr, 0, 0);
	
	sharesys->AddDependency(myself, "sdktools.ext", true, true);
	sharesys->AddDependency(myself, "sdkhooks.ext", true, true);
	
	//g_pMyInterface = new SendProxyManagerInterfaceImpl();
	sharesys->AddInterface(myself, &g_pMyInterface);
	//we should not maintain compatibility with old plugins which uses earlier versions of sendproxy (< 1.3)
	sharesys->RegisterLibrary(myself, "sendproxy13");
	plsys->AddPluginsListener(&g_SendProxyManager);

	return true;
}

void SendProxyManager::SDK_OnAllLoaded()
{
	sharesys->AddNatives(myself, g_MyNatives);
	SM_GET_LATE_IFACE(SDKTOOLS, g_pSDKTools);
	SM_GET_LATE_IFACE(SDKHOOKS, g_pSDKHooks);

	if (g_pSDKHooks)
	{
		g_pSDKHooks->AddEntityListener(this);
	}
}

void SendProxyManager::SDK_OnUnload()
{
	for (int i = 0; i < g_Hooks.Count(); i++)
	{
		g_Hooks[i].pVar->SetProxyFn(g_Hooks[i].pRealProxy);
	}
	
	for (int i = 0; i < g_HooksGamerules.Count(); i++)
	{
		g_HooksGamerules[i].pVar->SetProxyFn(g_HooksGamerules[i].pRealProxy);
	}
	
	SH_REMOVE_HOOK(IServerGameClients, ClientDisconnect, gameclients, SH_STATIC(Hook_ClientDisconnect), false);
	SH_REMOVE_HOOK(IServerGameDLL, GameFrame, gamedll, SH_STATIC(Hook_GameFrame), false);

	if (!g_bFirstTimeCalled)
		SH_REMOVE_HOOK(IServer, GetClientCount, g_pIServer, SH_MEMBER(this, &SendProxyManager::GetClientCount), false);

	DestroyDetours(CFrameSnapshotManager_DTR_UsePreviouslySentPacket);
	DestroyDetours(CFrameSnapshotManager_DTR_CreatePackedEntity);
	DestroyDetours(CFrameSnapshotManager_DTR_GetPreviouslySentPacket);
	DestroyDetours(CGameServer_DTR_SendClientMessages);
	DestroyDetours(CGameClient_DTR_ShouldSendMessages);
	DestroyDetours(DTR_SV_ComputeClientPacks);
	
	gameconfs->CloseGameConfigFile(g_pGameConf);
	gameconfs->CloseGameConfigFile(g_pGameConfSDKTools);
	
	plsys->RemovePluginsListener(&g_SendProxyManager);
	if( g_pSDKHooks )
	{
		g_pSDKHooks->RemoveEntityListener(this);
	}

	//delete g_pMyInterface;
}

void SendProxyManager::OnCoreMapEnd()
{
	for (int i = 0; i < g_HooksGamerules.Count(); i++)
	{
		UnhookProxyGamerules(i);
		i--;
	}

	g_pGameRulesProxyEdict = nullptr;
	g_iGameRulesProxyIndex = -1;
}

void SendProxyManager::OnCoreMapStart(edict_t * pEdictList, int edictCount, int clientMax)
{
	for (int i = 0; i < (sizeof(g_PlayersPackedGameRules) / sizeof(g_PlayersPackedGameRules[0])); ++i)
	{
		g_PlayersPackedGameRules[i] = INVALID_PACKED_ENTITY_HANDLE;
	}

	CBaseEntity *pGameRulesProxyEnt = FindEntityByServerClassname(0, g_szGameRulesProxy);
	if (!pGameRulesProxyEnt)
	{
		smutils->LogError(myself, "Unable to get gamerules proxy ent (1)!");
		return;
	}

	g_pGameRulesProxyEdict = gameents->BaseEntityToEdict(pGameRulesProxyEnt);
	if (!g_pGameRulesProxyEdict)
		smutils->LogError(myself, "Unable to get gamerules proxy ent (2)!");
	
	if (g_pGameRulesProxyEdict)
		g_iGameRulesProxyIndex = gamehelpers->IndexOfEdict(g_pGameRulesProxyEdict);
}

bool SendProxyManager::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_ANY(GetServerFactory, gameents, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);
	GET_V_IFACE_ANY(GetServerFactory, gameclients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	GET_V_IFACE_ANY(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	
	g_pGlobals = ismm->GetCGlobals();
	
	SH_ADD_HOOK(IServerGameDLL, GameFrame, gamedll, SH_STATIC(Hook_GameFrame), false);
	SH_ADD_HOOK(IServerGameClients, ClientDisconnect, gameclients, SH_STATIC(Hook_ClientDisconnect), false);
	
	// to do: Any way to resolve this crash?
	GET_CONVAR(sv_parallel_packentities);
	sv_parallel_packentities->SetValue(0); //If we don't do that the sendproxy extension will crash the server (Post ref: https://forums.alliedmods.net/showpost.php?p=2540106&postcount=324 )
	GET_CONVAR(sv_parallel_sendsnapshot);
	sv_parallel_sendsnapshot->SetValue(0); //If we don't do that, sendproxy will not work correctly and may crash server. This affects all versions of sendproxy manager!
	
	return true;
}

void SendProxyManager::OnPluginUnloaded(IPlugin * plugin)
{
	IPluginContext * pCtx = plugin->GetBaseContext();
	for (int i = 0; i < g_Hooks.Count(); i++)
	{
		if (g_Hooks[i].sCallbackInfo.iCallbackType == CallBackType::Callback_PluginFunction && ((IPluginFunction *)g_Hooks[i].sCallbackInfo.pCallback)->GetParentContext() == pCtx)
		{
			UnhookProxy(i);
			i--;
		}
	}

	for (int i = 0; i < g_HooksGamerules.Count(); i++)
	{
		if (g_HooksGamerules[i].sCallbackInfo.iCallbackType == CallBackType::Callback_PluginFunction && ((IPluginFunction *)g_HooksGamerules[i].sCallbackInfo.pCallback)->GetParentContext() == pCtx)
		{
			UnhookProxyGamerules(i);
			i--;
		}
	}

	for (int i = 0; i < g_ChangeHooks.Count(); i++)
	{
		auto pCallbacks = g_ChangeHooks[i].vCallbacksInfo;
		if (pCallbacks->Count())
		{
			for (int j = 0; j < pCallbacks->Count(); j++)
			{
				if ((*pCallbacks)[j].iCallbackType == CallBackType::Callback_PluginFunction && (IPluginContext *)(*pCallbacks)[j].pOwner == pCtx)
				{
					pCallbacks->Remove(j--);
				}
			}
		}

		//else do not needed here
		if (!pCallbacks->Count())
			g_ChangeHooks.Remove(i);
	}

	for (int i = 0; i < g_ChangeHooksGamerules.Count(); i++)
	{
		auto pCallbacks = g_ChangeHooksGamerules[i].vCallbacksInfo;
		if (pCallbacks->Count())
		{
			for (int j = 0; j < pCallbacks->Count(); j++)
			{
				if ((*pCallbacks)[j].iCallbackType == CallBackType::Callback_PluginFunction && (IPluginContext *)(*pCallbacks)[j].pOwner == pCtx)
				{
					pCallbacks->Remove(j--);
				}
			}
		}

		//else do not needed here
		if (!pCallbacks->Count())
			g_ChangeHooksGamerules.Remove(i);
	}
}

//functions

bool SendProxyManager::AddHookToList(SendPropHook hook)
{
	//Need to make sure this prop isn't already hooked for this entity - we don't care anymore
	bool bEdictHooked = false;
	for (int i = 0; i < g_Hooks.Count(); i++)
	{
		if (g_Hooks[i].objectID == hook.objectID)
		{
			//we don't care anymore
			//if (g_Hooks[i].pVar == hook.pVar)
			//	return false;
			//else
				bEdictHooked = true;
		}
	}

	g_Hooks.AddToTail(hook);
	if (!bEdictHooked)
		g_vHookedEdicts.AddToTail(hook.pEnt);

	return true;
}

bool SendProxyManager::AddHookToListGamerules(SendPropHookGamerules hook)
{
	//Need to make sure this prop isn't already hooked for this entity - we don't care anymore
	/*for (int i = 0; i < g_HooksGamerules.Count(); i++)
	{
		if (g_HooksGamerules[i].pVar == hook.pVar)
			return false;
	}*/
	g_HooksGamerules.AddToTail(hook);
	return true;
}

bool SendProxyManager::AddChangeHookToList(PropChangeHook sHook, CallBackInfo * pInfo)
{
	decltype(&g_ChangeHooks[0]) pHookInfo = nullptr;
	for (int i = 0; i < g_ChangeHooks.Count(); i++)
	{
		if (g_ChangeHooks[i].pVar == sHook.pVar)
		{
			pHookInfo = &g_ChangeHooks[i];
			break;
		}
	}

	if (pHookInfo)
	{
		//just validate it
		switch (sHook.PropType)
		{
		case PropType::Prop_Int:
		case PropType::Prop_Float:
		case PropType::Prop_String:
		case PropType::Prop_Vector:
			break;
		default: return false;
		}

		pHookInfo->vCallbacksInfo->AddToTail(*pInfo);
	}
	else
	{
		edict_t * pEnt = gamehelpers->EdictOfIndex(sHook.objectID);
		if (!pEnt || pEnt->IsFree()) 
			return false; //should never happen

		CBaseEntity * pEntity = gameents->EdictToBaseEntity(pEnt);
		if (!pEntity) 
			return false; //should never happen

		switch (sHook.PropType)
		{
		case PropType::Prop_Int: sHook.iLastValue = *(int *)((unsigned char *)pEntity + sHook.Offset); break;
		case PropType::Prop_Float: sHook.flLastValue = *(float *)((unsigned char*)pEntity + sHook.Offset); break;
		case PropType::Prop_String: strncpynull(sHook.cLastValue, (const char *)((unsigned char *)pEntity + sHook.Offset), sizeof(sHook.cLastValue)); break;
		case PropType::Prop_Vector: sHook.vecLastValue = *(Vector *)((unsigned char *)pEntity + sHook.Offset); break;
		default: return false;
		}

		CallBackInfo sCallInfo = *pInfo;
		sHook.vCallbacksInfo->AddToTail(sCallInfo);
		g_ChangeHooks.AddToTail(sHook);
	}

	return true;
}

bool SendProxyManager::AddChangeHookToListGamerules(PropChangeHookGamerules sHook, CallBackInfo * pInfo)
{
	decltype(&g_ChangeHooksGamerules[0]) pHookInfo = nullptr;
	for (int i = 0; i < g_ChangeHooksGamerules.Count(); i++)
	{
		if (g_ChangeHooksGamerules[i].pVar == sHook.pVar)
		{
			pHookInfo = &g_ChangeHooksGamerules[i];
			break;
		}
	}

	if (pHookInfo)
	{
		//just validate it
		switch (sHook.PropType)
		{
		case PropType::Prop_Int:
		case PropType::Prop_Float:
		case PropType::Prop_String:
		case PropType::Prop_Vector:
			break;
		default: return false;
		}

		pHookInfo->vCallbacksInfo->AddToTail(*pInfo);
	}
	else
	{
		switch (sHook.PropType)
		{
		case PropType::Prop_Int: sHook.iLastValue = *(int *)((unsigned char *)g_pGameRules + sHook.Offset); break;
		case PropType::Prop_Float: sHook.flLastValue = *(float *)((unsigned char*)g_pGameRules + sHook.Offset); break;
		case PropType::Prop_String: strncpynull(sHook.cLastValue, (const char *)((unsigned char *)g_pGameRules + sHook.Offset), sizeof(sHook.cLastValue)); break;
		case PropType::Prop_Vector: sHook.vecLastValue = *(Vector *)((unsigned char *)g_pGameRules + sHook.Offset); break;
		default: return false;
		}

		CallBackInfo sCallInfo = *pInfo;
		sHook.vCallbacksInfo->AddToTail(sCallInfo);
		g_ChangeHooksGamerules.AddToTail(sHook);
	}

	return true;
}

void SendProxyManager::UnhookProxy(int i)
{
	// if there are other hooks for this prop, don't change the proxy, just remove it from our list
	for (int j = 0; j < g_Hooks.Count(); j++)
	{
		if (g_Hooks[j].pVar == g_Hooks[i].pVar && i != j)
		{
			CallListenersForHookID(i);
			g_Hooks.Remove(i); // for others: this not a mistake
			return;
		}
	}

	for (int j = 0; j < g_vHookedEdicts.Count(); j++)
	{
		if (g_vHookedEdicts[j] == g_Hooks[i].pEnt)
		{
			g_vHookedEdicts.Remove(j);
			break;
		}
	}

	CallListenersForHookID(i);
	g_Hooks[i].pVar->SetProxyFn(g_Hooks[i].pRealProxy);
	g_Hooks.Remove(i);
}

void SendProxyManager::UnhookProxyGamerules(int i)
{
	//if there are other hooks for this prop, don't change the proxy, just remove it from our list
	for (int j = 0; j < g_HooksGamerules.Count(); j++)
	{
		if (g_HooksGamerules[j].pVar == g_HooksGamerules[i].pVar && i != j)
		{
			CallListenersForHookIDGamerules(i);
			g_HooksGamerules.Remove(i);
			return;
		}
	}

	CallListenersForHookIDGamerules(i);
	g_HooksGamerules[i].pVar->SetProxyFn(g_HooksGamerules[i].pRealProxy);
	g_HooksGamerules.Remove(i);
}

void SendProxyManager::UnhookChange(int i, CallBackInfo * pInfo)
{
	if (i < 0 || i >= g_ChangeHooks.Count())
		return;

	auto pCallbacks = g_ChangeHooks[i].vCallbacksInfo;
	if (pCallbacks->Count())
	{
		for (int j = 0; j < pCallbacks->Count(); j++)
		{
			if ((*pCallbacks)[j].iCallbackType == pInfo->iCallbackType && (*pCallbacks)[j].pCallback == (void *)pInfo->pCallback)
			{
				pCallbacks->Remove(j--);
			}
		}
	}

	//if there no any callbacks anymore, then remove all info about this hook
	if (!pCallbacks->Count())
		g_ChangeHooks.Remove(i);
}

void SendProxyManager::UnhookChangeGamerules(int i, CallBackInfo * pInfo)
{
	if (i < 0 || i >= g_ChangeHooksGamerules.Count())
		return;

	auto pCallbacks = g_ChangeHooksGamerules[i].vCallbacksInfo;
	if (pCallbacks->Count())
	{
		for (int j = 0; j < pCallbacks->Count(); j++)
		{
			if ((*pCallbacks)[j].iCallbackType == pInfo->iCallbackType && (*pCallbacks)[j].pCallback == (void *)pInfo->pCallback)
			{
				pCallbacks->Remove(j--);
			}
		}
	}

	//if there no any callbacks anymore, then remove all info about this hook
	if (!pCallbacks->Count())
		g_ChangeHooksGamerules.Remove(i);
}

//callbacks

//Change

void CallChangeCallbacks(PropChangeHook * pInfo, void * pOldValue, void * pNewValue)
{
	for (int i = 0; i < pInfo->vCallbacksInfo->Count(); i++)
	{
		auto sCallback = (*pInfo->vCallbacksInfo)[i];
		switch (sCallback.iCallbackType)
		{
		case CallBackType::Callback_CPPCallbackInterface:
		{
			edict_t * pEnt = gamehelpers->EdictOfIndex(pInfo->objectID);
			if (!pEnt)
				break; //???

			ISendProxyChangeCallbacks * pCallbacks = (ISendProxyChangeCallbacks *)sCallback.pCallback;
			pCallbacks->OnEntityPropChange(gameents->EdictToBaseEntity(pEnt), pInfo->pVar, pNewValue, pOldValue, pInfo->PropType, pInfo->Element);
		}
		break;

		case CallBackType::Callback_PluginFunction:
		{
			IPluginFunction * pCallBack = (IPluginFunction *)sCallback.pCallback;
			switch (pInfo->PropType)
			{
			case PropType::Prop_Int:
			{
				pCallBack->PushCell(pInfo->objectID);
				pCallBack->PushString(pInfo->pVar->GetName());
				pCallBack->PushCell(pInfo->iLastValue);
				pCallBack->PushCell(*(int *)pNewValue);
				pCallBack->PushCell(pInfo->Element);
				pCallBack->Execute(0);
			}
			break;

			case PropType::Prop_Float:
			{
				pCallBack->PushCell(pInfo->objectID);
				pCallBack->PushString(pInfo->pVar->GetName());
				pCallBack->PushFloat(pInfo->flLastValue);
				pCallBack->PushFloat(*(float *)pNewValue);
				pCallBack->PushCell(pInfo->Element);
				pCallBack->Execute(0);
			}
			break;

			case PropType::Prop_String:
			{
				pCallBack->PushCell(pInfo->objectID);
				pCallBack->PushString(pInfo->pVar->GetName());
				pCallBack->PushString(pInfo->cLastValue);
				pCallBack->PushString((char *)pNewValue);
				pCallBack->PushCell(pInfo->Element);
				pCallBack->Execute(0);
			}
			break;

			case PropType::Prop_Vector:
			{
				cell_t vector[2][3];
				Vector * pVec = (Vector *)pNewValue;
				vector[0][0] = sp_ftoc(pVec->x);
				vector[0][1] = sp_ftoc(pVec->y);
				vector[0][2] = sp_ftoc(pVec->z);
				vector[1][0] = sp_ftoc(pInfo->vecLastValue.x);
				vector[1][1] = sp_ftoc(pInfo->vecLastValue.y);
				vector[1][2] = sp_ftoc(pInfo->vecLastValue.z);
				pCallBack->PushCell(pInfo->objectID);
				pCallBack->PushString(pInfo->pVar->GetName());
				pCallBack->PushArray(vector[1], 3);
				pCallBack->PushArray(vector[0], 3);
				pCallBack->PushCell(pInfo->Element);
				pCallBack->Execute(0);
			}
			break;

			}
		}
		break;

		}
	}
}

void CallChangeGamerulesCallbacks(PropChangeHookGamerules * pInfo, void * pOldValue, void * pNewValue)
{
	for (int i = 0; i < pInfo->vCallbacksInfo->Count(); i++)
	{
		auto sCallback = (*pInfo->vCallbacksInfo)[i];
		switch (sCallback.iCallbackType)
		{
		case CallBackType::Callback_CPPCallbackInterface:
		{
			ISendProxyChangeCallbacks * pCallbacks = (ISendProxyChangeCallbacks *)sCallback.pCallback;
			pCallbacks->OnGamerulesPropChange(pInfo->pVar, pNewValue, pOldValue, pInfo->PropType, pInfo->Element);
		}
		break;

		case CallBackType::Callback_PluginFunction:
		{
			IPluginFunction * pCallBack = (IPluginFunction *)sCallback.pCallback;
			switch (pInfo->PropType)
			{
			case PropType::Prop_Int:
			{
				pCallBack->PushString(pInfo->pVar->GetName());
				pCallBack->PushCell(pInfo->iLastValue);
				pCallBack->PushCell(*(int *)pNewValue);
				pCallBack->PushCell(pInfo->Element);
				pCallBack->Execute(0);
			}
			break;

			case PropType::Prop_Float:
			{
				pCallBack->PushString(pInfo->pVar->GetName());
				pCallBack->PushFloat(pInfo->flLastValue);
				pCallBack->PushFloat(*(float *)pNewValue);
				pCallBack->PushCell(pInfo->Element);
				pCallBack->Execute(0);
			}
			break;

			case PropType::Prop_String:
			{
				pCallBack->PushString(pInfo->pVar->GetName());
				pCallBack->PushString(pInfo->cLastValue);
				pCallBack->PushString((char *)pNewValue);
				pCallBack->PushCell(pInfo->Element);
				pCallBack->Execute(0);
			}
			break;

			case PropType::Prop_Vector:
			{
				cell_t vector[2][3];
				Vector * pVec = (Vector *)pNewValue;
				vector[0][0] = sp_ftoc(pVec->x);
				vector[0][1] = sp_ftoc(pVec->y);
				vector[0][2] = sp_ftoc(pVec->z);
				vector[1][0] = sp_ftoc(pInfo->vecLastValue.x);
				vector[1][1] = sp_ftoc(pInfo->vecLastValue.y);
				vector[1][2] = sp_ftoc(pInfo->vecLastValue.z);
				pCallBack->PushString(pInfo->pVar->GetName());
				pCallBack->PushArray(vector[1], 3);
				pCallBack->PushArray(vector[0], 3);
				pCallBack->PushCell(pInfo->Element);
				pCallBack->Execute(0);
			}
			break;

			}
		}
		break;

		}
	}
}

//Proxy

bool CallInt(SendPropHook &hook, int *ret, int iElement)
{
	if (!g_bSVComputePacksDone)
		return false;
	
	AUTO_LOCK_FM(g_WorkMutex);

	if (!hook.pVar->IsInsideArray())
		iElement = hook.Element;

	switch (hook.sCallbackInfo.iCallbackType)
	{
		case CallBackType::Callback_PluginFunction:
		{
			IPluginFunction *callback = (IPluginFunction *)hook.sCallbackInfo.pCallback;
			cell_t value = *ret;
			cell_t result = Pl_Continue;
			callback->PushCell(hook.objectID);
			callback->PushString(hook.pVar->GetName());
			callback->PushCellByRef(&value);
			callback->PushCell(iElement);
			callback->PushCell(g_iCurrentClientIndexInLoop + 1);
			callback->Execute(&result);
			if (result == Pl_Changed)
			{
				*ret = value;
				return true;
			}

			break;
		}

		case CallBackType::Callback_CPPCallbackInterface:
		{
			ISendProxyCallbacks * pCallbacks = (ISendProxyCallbacks *)hook.sCallbackInfo.pCallback;
			int iValue = *ret;
			bool bChange = pCallbacks->OnEntityPropProxyFunctionCalls(gameents->EdictToBaseEntity(hook.pEnt), hook.pVar, (CBasePlayer *)gamehelpers->ReferenceToEntity(g_iCurrentClientIndexInLoop + 1), (void *)&iValue, hook.PropType, iElement);
			if (bChange)
			{
				*ret = iValue;
				return true;
			}

			break;
		}
	}
	return false;
}

bool CallIntGamerules(SendPropHookGamerules &hook, int *ret, int iElement)
{
	if (!g_bSVComputePacksDone)
		return false;
	
	AUTO_LOCK_FM(g_WorkMutex);

	if (!hook.pVar->IsInsideArray())
		iElement = hook.Element;

	switch (hook.sCallbackInfo.iCallbackType)
	{
		case CallBackType::Callback_PluginFunction:
		{
			IPluginFunction *callback = (IPluginFunction *)hook.sCallbackInfo.pCallback;
			cell_t value = *ret;
			cell_t result = Pl_Continue;
			callback->PushString(hook.pVar->GetName());
			callback->PushCellByRef(&value);
			callback->PushCell(iElement);
			callback->PushCell(g_iCurrentClientIndexInLoop + 1);
			callback->Execute(&result);
			if (result == Pl_Changed)
			{
				*ret = value;
				return true;
			}

			break;
		}

		case CallBackType::Callback_CPPCallbackInterface:
		{
			ISendProxyCallbacks * pCallbacks = (ISendProxyCallbacks *)hook.sCallbackInfo.pCallback;
			int iValue = *ret;
			bool bChange = pCallbacks->OnGamerulesPropProxyFunctionCalls(hook.pVar, (CBasePlayer *)gamehelpers->ReferenceToEntity(g_iCurrentClientIndexInLoop + 1), (void *)&iValue, hook.PropType, iElement);
			if (bChange)
			{
				*ret = iValue;
				return true;
			}

			break;
		}
	}

	return false;
}

bool CallFloat(SendPropHook &hook, float *ret, int iElement)
{
	if (!g_bSVComputePacksDone)
		return false;
	
	AUTO_LOCK_FM(g_WorkMutex);
	
	if (!hook.pVar->IsInsideArray())
		iElement = hook.Element;

	switch (hook.sCallbackInfo.iCallbackType)
	{
		case CallBackType::Callback_PluginFunction:
		{
			IPluginFunction *callback = (IPluginFunction *)hook.sCallbackInfo.pCallback;
			float value = *ret;
			cell_t result = Pl_Continue;
			callback->PushCell(hook.objectID);
			callback->PushString(hook.pVar->GetName());
			callback->PushFloatByRef(&value);
			callback->PushCell(iElement);
			callback->PushCell(g_iCurrentClientIndexInLoop + 1);
			callback->Execute(&result);
			if (result == Pl_Changed)
			{
				*ret = value;
				return true;
			}

			break;
		}

		case CallBackType::Callback_CPPCallbackInterface:
		{
			ISendProxyCallbacks * pCallbacks = (ISendProxyCallbacks *)hook.sCallbackInfo.pCallback;
			float flValue = *ret;
			bool bChange = pCallbacks->OnEntityPropProxyFunctionCalls(gameents->EdictToBaseEntity(hook.pEnt), hook.pVar, (CBasePlayer *)gamehelpers->ReferenceToEntity(g_iCurrentClientIndexInLoop + 1), (void *)&flValue, hook.PropType, iElement);
			if (bChange)
			{
				*ret = flValue;
				return true;
			}

			break;
		}
	}

	return false;
}

bool CallFloatGamerules(SendPropHookGamerules &hook, float *ret, int iElement)
{
	if (!g_bSVComputePacksDone)
		return false;
	
	AUTO_LOCK_FM(g_WorkMutex);

	if (!hook.pVar->IsInsideArray())
		iElement = hook.Element;

	switch (hook.sCallbackInfo.iCallbackType)
	{
		case CallBackType::Callback_PluginFunction:
		{
			IPluginFunction *callback = (IPluginFunction *)hook.sCallbackInfo.pCallback;
			float value = *ret;
			cell_t result = Pl_Continue;
			callback->PushString(hook.pVar->GetName());
			callback->PushFloatByRef(&value);
			callback->PushCell(iElement);
			callback->PushCell(g_iCurrentClientIndexInLoop + 1);
			callback->Execute(&result);
			if (result == Pl_Changed)
			{
				*ret = value;
				return true;
			}

			break;
		}

		case CallBackType::Callback_CPPCallbackInterface:
		{
			ISendProxyCallbacks * pCallbacks = (ISendProxyCallbacks *)hook.sCallbackInfo.pCallback;
			float flValue = *ret;
			bool bChange = pCallbacks->OnGamerulesPropProxyFunctionCalls(hook.pVar, (CBasePlayer *)gamehelpers->ReferenceToEntity(g_iCurrentClientIndexInLoop + 1), (void *)&flValue, hook.PropType, iElement);
			if (bChange)
			{
				*ret = flValue;
				return true;
			}

			break;
		}
	}

	return false;
}

bool CallString(SendPropHook &hook, char **ret, int iElement)
{
	if (!g_bSVComputePacksDone)
		return false;
	
	AUTO_LOCK_FM(g_WorkMutex);

	if (!hook.pVar->IsInsideArray())
		iElement = hook.Element;

	static char value[4096];
	switch (hook.sCallbackInfo.iCallbackType)
	{
		case CallBackType::Callback_PluginFunction:
		{
			IPluginFunction *callback = (IPluginFunction *)hook.sCallbackInfo.pCallback;
			strncpynull(value, *ret, 4096);
			cell_t result = Pl_Continue;
			callback->PushCell(hook.objectID);
			callback->PushString(hook.pVar->GetName());
			callback->PushStringEx(value, 4096, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
			callback->PushCell(iElement);
			callback->PushCell(g_iCurrentClientIndexInLoop + 1);
			callback->Execute(&result);
			if (result == Pl_Changed)
			{
				*ret = value;
				return true;
			}

			break;
		}

		case CallBackType::Callback_CPPCallbackInterface:
		{
			ISendProxyCallbacks * pCallbacks = (ISendProxyCallbacks *)hook.sCallbackInfo.pCallback;
			strncpynull(value, *ret, 4096);
			bool bChange = pCallbacks->OnEntityPropProxyFunctionCalls(gameents->EdictToBaseEntity(hook.pEnt), hook.pVar, (CBasePlayer *)gamehelpers->ReferenceToEntity(g_iCurrentClientIndexInLoop + 1), (void *)value, hook.PropType, iElement);
			if (bChange)
			{
				*ret = value;
				return true;
			}

			break;
		}
	}

	return false;
}

bool CallStringGamerules(SendPropHookGamerules &hook, char **ret, int iElement)
{
	if (!g_bSVComputePacksDone)
		return false;
	
	AUTO_LOCK_FM(g_WorkMutex);

	if (!hook.pVar->IsInsideArray())
		iElement = hook.Element;

	static char value[4096];
	switch (hook.sCallbackInfo.iCallbackType)
	{
		case CallBackType::Callback_PluginFunction:
		{
			void *pGamerules = g_pSDKTools->GetGameRules();
			if(!pGamerules)
			{
				g_pSM->LogError(myself, "CRITICAL ERROR: Could not get gamerules pointer!");
			}
			
			IPluginFunction *callback = (IPluginFunction *)hook.sCallbackInfo.pCallback;
			strncpynull(value, *ret, 4096);
			cell_t result = Pl_Continue;
			callback->PushString(hook.pVar->GetName());
			callback->PushStringEx(value, 4096, SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
			callback->PushCell(iElement);
			callback->PushCell(g_iCurrentClientIndexInLoop + 1);
			callback->Execute(&result);
			if (result == Pl_Changed)
			{
				*ret = value;
				return true;
			}

			break;
		}

		case CallBackType::Callback_CPPCallbackInterface:
		{
			void * pGamerules = g_pSDKTools->GetGameRules();
			if(!pGamerules)
				return false;
			ISendProxyCallbacks * pCallbacks = (ISendProxyCallbacks *)hook.sCallbackInfo.pCallback;
			strncpynull(value, *ret, 4096);
			bool bChange = pCallbacks->OnGamerulesPropProxyFunctionCalls(hook.pVar, (CBasePlayer *)gamehelpers->ReferenceToEntity(g_iCurrentClientIndexInLoop + 1), (void *)value, hook.PropType, iElement);
			if (bChange)
			{
				*ret = value;
				return true;
			}

			break;
		}
	}

	return false;
}

bool CallVector(SendPropHook &hook, Vector &vec, int iElement)
{
	if (!g_bSVComputePacksDone)
		return false;
	
	AUTO_LOCK_FM(g_WorkMutex);

	if (!hook.pVar->IsInsideArray())
		iElement = hook.Element;

	switch (hook.sCallbackInfo.iCallbackType)
	{
		case CallBackType::Callback_PluginFunction:
		{
			IPluginFunction *callback = (IPluginFunction *)hook.sCallbackInfo.pCallback;

			cell_t vector[3];
			vector[0] = sp_ftoc(vec.x);
			vector[1] = sp_ftoc(vec.y);
			vector[2] = sp_ftoc(vec.z);

			cell_t result = Pl_Continue;
			callback->PushCell(hook.objectID);
			callback->PushString(hook.pVar->GetName());
			callback->PushArray(vector, 3, SM_PARAM_COPYBACK);
			callback->PushCell(iElement);
			callback->PushCell(g_iCurrentClientIndexInLoop + 1);
			callback->Execute(&result);
			if (result == Pl_Changed)
			{
				vec.x = sp_ctof(vector[0]);
				vec.y = sp_ctof(vector[1]);
				vec.z = sp_ctof(vector[2]);
				return true;
			}

			break;
		}

		case CallBackType::Callback_CPPCallbackInterface:
		{
			ISendProxyCallbacks * pCallbacks = (ISendProxyCallbacks *)hook.sCallbackInfo.pCallback;
			Vector vNewVec(vec.x, vec.y, vec.z);
			bool bChange = pCallbacks->OnGamerulesPropProxyFunctionCalls(hook.pVar, (CBasePlayer *)gamehelpers->ReferenceToEntity(g_iCurrentClientIndexInLoop + 1), (void *)&vNewVec, hook.PropType, iElement);
			if (bChange)
			{
				vec.x = vNewVec.x;
				vec.y = vNewVec.y;
				vec.z = vNewVec.z;
				return true;
			}

			break;
		}
	}

	return false;
}

bool CallVectorGamerules(SendPropHookGamerules &hook, Vector &vec, int iElement)
{
	if (!g_bSVComputePacksDone)
		return false;
	
	AUTO_LOCK_FM(g_WorkMutex);

	if (!hook.pVar->IsInsideArray())
		iElement = hook.Element;

	switch (hook.sCallbackInfo.iCallbackType)
	{
		case CallBackType::Callback_PluginFunction:
		{
			IPluginFunction *callback = (IPluginFunction *)hook.sCallbackInfo.pCallback;

			cell_t vector[3];
			vector[0] = sp_ftoc(vec.x);
			vector[1] = sp_ftoc(vec.y);
			vector[2] = sp_ftoc(vec.z);

			cell_t result = Pl_Continue;
			callback->PushString(hook.pVar->GetName());
			callback->PushArray(vector, 3, SM_PARAM_COPYBACK);
			callback->PushCell(iElement);
			callback->PushCell(g_iCurrentClientIndexInLoop + 1);
			callback->Execute(&result);
			if (result == Pl_Changed)
			{
				vec.x = sp_ctof(vector[0]);
				vec.y = sp_ctof(vector[1]);
				vec.z = sp_ctof(vector[2]);
				return true;
			}

			break;
		}

		case CallBackType::Callback_CPPCallbackInterface:
		{
			ISendProxyCallbacks * pCallbacks = (ISendProxyCallbacks *)hook.sCallbackInfo.pCallback;
			Vector vNewVec(vec.x, vec.y, vec.z);
			bool bChange = pCallbacks->OnGamerulesPropProxyFunctionCalls(hook.pVar, (CBasePlayer *)gamehelpers->ReferenceToEntity(g_iCurrentClientIndexInLoop + 1), (void *)&vNewVec, hook.PropType, iElement);
			if (bChange)
			{
				vec.x = vNewVec.x;
				vec.y = vNewVec.y;
				vec.z = vNewVec.z;
				return true;
			}

			break;
		}
	}

	return false;
}

void GlobalProxy(const SendProp *pProp, const void *pStructBase, const void * pData, DVariant *pOut, int iElement, int objectID)
{
	edict_t * pEnt = gamehelpers->EdictOfIndex(objectID);
	bool bHandled = false;
	for (int i = 0; i < g_Hooks.Count(); i++)
	{
		if (g_Hooks[i].objectID == objectID && 
			g_Hooks[i].pVar == pProp && 
			pEnt == g_Hooks[i].pEnt && 
			(!pProp->IsInsideArray() || g_Hooks[i].Element == iElement))
		{
			switch (g_Hooks[i].PropType)
			{
				case PropType::Prop_Int:
				{
					int result = *(int *)pData;

					if (CallInt(g_Hooks[i], &result, iElement))
					{
						long data = result;
						g_Hooks[i].pRealProxy(pProp, pStructBase, &data, pOut, iElement, objectID);
						return; // If somebody already handled this call, do not call other hooks for this entity & prop
					}
					else
					{
						g_Hooks[i].pRealProxy(pProp, pStructBase, pData, pOut, iElement, objectID);
					}

					bHandled = true;
					continue;
				}

				case PropType::Prop_Float:
				{
					float result = *(float *)pData;

					if (CallFloat(g_Hooks[i], &result, iElement))
					{
						g_Hooks[i].pRealProxy(pProp, pStructBase, &result, pOut, iElement, objectID);
						return; // If somebody already handled this call, do not call other hooks for this entity & prop
					}
					else
					{
						g_Hooks[i].pRealProxy(pProp, pStructBase, pData, pOut, iElement, objectID);
					}

					bHandled = true;
					continue;
				}

				case PropType::Prop_String:
				{
					const char * result = (char*)pData;
					if (!result) //there can be null;
						result = "";

					if (CallString(g_Hooks[i], const_cast<char **>(&result), iElement))
					{
						g_Hooks[i].pRealProxy(pProp, pStructBase, result, pOut, iElement, objectID);
						return; // If somebody already handled this call, do not call other hooks for this entity & prop
					}
					else
					{
						g_Hooks[i].pRealProxy(pProp, pStructBase, pData, pOut, iElement, objectID);
					}

					bHandled = true;
					continue;
				}

				case PropType::Prop_Vector:
				{
					Vector result = *(Vector *)pData;

					if (CallVector(g_Hooks[i], result, iElement))
					{
						g_Hooks[i].pRealProxy(pProp, pStructBase, &result, pOut, iElement, objectID);
						return; // If somebody already handled this call, do not call other hooks for this entity & prop
					}
					else
					{
						g_Hooks[i].pRealProxy(pProp, pStructBase, pData, pOut, iElement, objectID);
					}

					bHandled = true;
					continue;
				}

				default: rootconsole->ConsolePrint("%s: SendProxy report: Unknown prop type (%s).", __func__, g_Hooks[i].pVar->GetName());
			}
		}
	}

	if (!bHandled)
	{
		//perhaps we aren't hooked, but we can still find the real proxy for this prop
		for (int i = 0; i < g_Hooks.Count(); i++)
		{
			if (g_Hooks[i].pVar == pProp)
			{
				g_Hooks[i].pRealProxy(pProp, pStructBase, pData, pOut, iElement, objectID);
				return;
			}
		}

		g_pSM->LogError(myself, "CRITICAL: Proxy for unmanaged entity %d called for prop %s", objectID, pProp->GetName());
	}
}

void GlobalProxyGamerules(const SendProp *pProp, const void *pStructBase, const void * pData, DVariant *pOut, int iElement, int objectID)
{
	if (!g_bShouldChangeGameRulesState)
		g_bShouldChangeGameRulesState = true; //If this called once, so, the props wants to be sent at this time, and we should do this for all clients!
	
	bool bHandled = false;
	for (int i = 0; i < g_HooksGamerules.Count(); i++)
	{
		if (g_HooksGamerules[i].pVar == pProp && (!pProp->IsInsideArray() || g_HooksGamerules[i].Element == iElement))
		{
			switch (g_HooksGamerules[i].PropType)
			{
				case PropType::Prop_Int:
				{
					int result = *(int *)pData;

					if (CallIntGamerules(g_HooksGamerules[i], &result, iElement))
					{
						long data = result;
						g_HooksGamerules[i].pRealProxy(pProp, pStructBase, &data, pOut, iElement, objectID);
						return; // If somebody already handled this call, do not call other hooks for this entity & prop
					}
					else
					{
						g_HooksGamerules[i].pRealProxy(pProp, pStructBase, pData, pOut, iElement, objectID);
					}

					bHandled = true;
					continue;
				}

				case PropType::Prop_Float:
				{
					float result = *(float *)pData;

					if (CallFloatGamerules(g_HooksGamerules[i], &result, iElement))
					{
						g_HooksGamerules[i].pRealProxy(pProp, pStructBase, &result, pOut, iElement, objectID);
						return; // If somebody already handled this call, do not call other hooks for this entity & prop
					} 
					else
					{
						g_HooksGamerules[i].pRealProxy(pProp, pStructBase, pData, pOut, iElement, objectID);
					}

					bHandled = true;
					continue;
				}

				case PropType::Prop_String:
				{
					const char *result = (char*)pData; //We need to use const because of C++11 restriction
					if (!result) //there can be null;
						result = "";

					if (CallStringGamerules(g_HooksGamerules[i], const_cast<char **>(&result), iElement))
					{
						g_HooksGamerules[i].pRealProxy(pProp, pStructBase, result, pOut, iElement, objectID);
						return; // If somebody already handled this call, do not call other hooks for this entity & prop
					}
					else
					{
						g_HooksGamerules[i].pRealProxy(pProp, pStructBase, pData, pOut, iElement, objectID);
					}

					bHandled = true;
					continue;
				}

				case PropType::Prop_Vector:
				{
					Vector result = *(Vector *)pData;

					if (CallVectorGamerules(g_HooksGamerules[i], result, iElement))
					{
						g_HooksGamerules[i].pRealProxy(pProp, pStructBase, &result, pOut, iElement, objectID);
						return; // If somebody already handled this call, do not call other hooks for this entity & prop
					}
					else
					{
						g_HooksGamerules[i].pRealProxy(pProp, pStructBase, pData, pOut, iElement, objectID);
					}

					bHandled = true;
					continue;
				}

				default: rootconsole->ConsolePrint("%s: SendProxy report: Unknown prop type (%s).", __func__, g_HooksGamerules[i].pVar->GetName());
			}
		}
	}

	if (!bHandled)
	{
		//perhaps we aren't hooked, but we can still find the real proxy for this prop
		for (int i = 0; i < g_HooksGamerules.Count(); i++)
		{
			if (g_HooksGamerules[i].pVar == pProp)
			{
				g_HooksGamerules[i].pRealProxy(pProp, pStructBase, pData, pOut, iElement, objectID);
				return;
			}
		}
		
		g_pSM->LogError(myself, "CRITICAL: Proxy for unmanaged gamerules called for prop %s", pProp->GetName());
	}
}

//help

CBaseEntity * FindEntityByServerClassname(int iStart, const char * pServerClassName)
{
	if (iStart >= g_iEdictCount)
		return nullptr;

	for (int i = iStart; i < g_iEdictCount; i++)
	{
		CBaseEntity * pEnt = gamehelpers->ReferenceToEntity(i);
		if (!pEnt)
			continue;

		IServerNetworkable * pNetworkable = ((IServerUnknown *)pEnt)->GetNetworkable();
		if (!pNetworkable)
			continue;

		const char * pName = pNetworkable->GetServerClass()->GetName();
		if (pName && !strcmp(pName, pServerClassName))
			return pEnt;
	}

	return nullptr;
}

bool IsPropValid(SendProp * pProp, PropType iType)
{
	switch (iType)
	{
		case PropType::Prop_Int:
		{
			if (pProp->GetType() != DPT_Int)
				return false;

			return true;
		}

		case PropType::Prop_Float:
		{
			if (pProp->GetType() != DPT_Float)
				return false;

			return true;
		}

		case PropType::Prop_String:
		{
			if (pProp->GetType() != DPT_String)
				return false;

			return true;
		}

		case PropType::Prop_Vector:
		{
			if (pProp->GetType() != DPT_Vector)
				return false;

			return true;
		}
	}
	return false;
}

char * strncpynull(char * pDestination, const char * pSource, size_t szCount)
{
	strncpy(pDestination, pSource, szCount);
	pDestination[szCount - 1] = 0;
	return pDestination;
}

void DestroyDetours(CDetour *pDetour)
{
	if (pDetour)
	{
		if (pDetour->IsEnabled())
		{
			pDetour->DisableDetour();
		}

		pDetour->Destroy();
		pDetour = nullptr;
	}
}