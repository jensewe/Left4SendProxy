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

#include "extension.h"
#include "natives.h"
#include "clientpacks_detours.h"
#include "sendprop_hookmanager.h"
#include "util.h"
#include <optional>
#include <string>

SendProxyManager g_SendProxyManager;
SMEXT_LINK(&g_SendProxyManager);

static SendPropHookManager s_SendPropHookManager;
SendPropHookManager *g_pSendPropHookManager = &s_SendPropHookManager;

std::string g_szGameRulesProxy;
IServerGameEnts * gameents = nullptr;
IServerGameClients * gameclients = nullptr;
CGlobalVars *gpGlobals = NULL;
IBinTools* bintools = NULL;
ISDKHooks * sdkhooks = nullptr;
ConVar *sv_parallel_packentities = nullptr;

class AutoSMGameConfig
{
public:
	static std::optional<AutoSMGameConfig> Load(const char* name)
	{
		char error[256];
		IGameConfig *gc = nullptr;
		if (!gameconfs->LoadGameConfigFile(name, &gc, error, sizeof(error)))
		{
			smutils->LogError(myself, "Could not read config file sdktools.games.txt: %s", error);
			return {};
		}
		return gc;
	}

public:
	AutoSMGameConfig() noexcept: gc_(nullptr) { }
	AutoSMGameConfig(IGameConfig *gc) noexcept: gc_(gc) { }
	~AutoSMGameConfig()
	{
		if (gc_ != nullptr)
			gameconfs->CloseGameConfigFile(gc_);
	}
	operator IGameConfig*() const noexcept
	{
		return gc_;
	}
	IGameConfig* operator->() const noexcept
	{
		return gc_;
	}

private:
	IGameConfig *gc_;
};

CFrameSnapshotManager* framesnapshotmanager = nullptr;
void* CFrameSnapshotManager::s_pfnCreateEmptySnapshot = nullptr;
ICallWrapper* CFrameSnapshotManager::s_callCreateEmptySnapshot = nullptr;
void* CFrameSnapshotManager::s_pfnRemoveEntityReference = nullptr;
ICallWrapper* CFrameSnapshotManager::s_callRemoveEntityReference = nullptr;
void* CFrameSnapshot::s_pfnReleaseReference = nullptr;
ICallWrapper *CFrameSnapshot::s_callReleaseReference = nullptr;

bool SendProxyManager::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	auto gc_sdktools = AutoSMGameConfig::Load("sdktools.games");
	auto gc = AutoSMGameConfig::Load("sendproxy");
	if (!gc_sdktools || !gc)
		return false;

	g_szGameRulesProxy = gc_sdktools.value()->GetKeyValue("GameRulesProxy");

	if (!gc.value()->GetMemSig("CFrameSnapshotManager::CreateEmptySnapshot", &CFrameSnapshotManager::s_pfnCreateEmptySnapshot))
	{
		ke::SafeSprintf(error, maxlength, "Unable to find signature address ""\"CFrameSnapshotManager::CreateEmptySnapshot\"""");
		return false;
	}

	if (!gc.value()->GetMemSig("CFrameSnapshotManager::RemoveEntityReference", &CFrameSnapshotManager::s_pfnRemoveEntityReference))
	{
		ke::SafeSprintf(error, maxlength, "Unable to find signature address ""\"CFrameSnapshotManager::RemoveEntityReference\"""");
		return false;
	}

	if (!gc.value()->GetMemSig("CFrameSnapshot::ReleaseReference", &CFrameSnapshot::s_pfnReleaseReference))
	{
		ke::SafeSprintf(error, maxlength, "Unable to find signature address ""\"CFrameSnapshot::ReleaseReference\"""");
		return false;
	}

	if (!gc.value()->GetAddress("framesnapshotmanager", (void**)&framesnapshotmanager))
	{
		ke::SafeSprintf(error, maxlength, "Unable to find address (framesnapshotmanager)");
		return false;
	}

	if (!ClientPacksDetour::Init(gc.value()))
	{
		return false;
	}

	if (late) //if we loaded late, we need manually to call that
		OnCoreMapStart(nullptr, 0, 0);
	
	sharesys->AddDependency(myself, "sdkhooks.ext", true, true);
	sharesys->AddDependency(myself, "bintools.ext", true, true);
	
	sharesys->RegisterLibrary(myself, "sendproxy2");

	plsys->AddPluginsListener(this);
	playerhelpers->AddClientListener(this);
	ConVar_Register(0, this);

	return true;
}

void SendProxyManager::SDK_OnAllLoaded()
{
	SM_GET_LATE_IFACE(SDKHOOKS, sdkhooks);
	SM_GET_LATE_IFACE(BINTOOLS, bintools);

	if (sdkhooks)
	{
		sdkhooks->AddEntityListener(this);
	}

	if (bintools)
	{
		SourceMod::PassInfo params[] = {
			{ PassType_Basic, PASSFLAG_BYVAL, sizeof(int), NULL, 0 },
			{ PassType_Basic, PASSFLAG_BYVAL, sizeof(int), NULL, 0 },
			{ PassType_Basic, PASSFLAG_BYVAL, sizeof(CFrameSnapshot*), NULL, 0 },
			{ PassType_Basic, PASSFLAG_BYVAL, sizeof(PackedEntityHandle_t), NULL, 0 }
		};

		CFrameSnapshot::s_callReleaseReference = bintools->CreateCall(CFrameSnapshot::s_pfnReleaseReference, CallConv_ThisCall, NULL, params, 0);
		if (CFrameSnapshot::s_callReleaseReference == NULL) {
			smutils->LogError(myself, "Unable to create ICallWrapper for \"CFrameSnapshot::ReleaseReference\"!");
			return;
		}

		CFrameSnapshotManager::s_callCreateEmptySnapshot = bintools->CreateCall(CFrameSnapshotManager::s_pfnCreateEmptySnapshot, CallConv_ThisCall, &params[2], &params[0], 2);
		if (CFrameSnapshotManager::s_callCreateEmptySnapshot == NULL) {
			smutils->LogError(myself, "Unable to create ICallWrapper for \"CFrameSnapshotManager::CreateEmptySnapshot\"!");
			return;
		}

		CFrameSnapshotManager::s_callRemoveEntityReference = bintools->CreateCall(CFrameSnapshotManager::s_pfnRemoveEntityReference, CallConv_ThisCall, NULL, &params[3], 1);
		if (CFrameSnapshotManager::s_callRemoveEntityReference == NULL) {
			smutils->LogError(myself, "Unable to create ICallWrapper for \"CFrameSnapshotManager::RemoveEntityReference\"!");
			return;
		}
	}
	
	sharesys->AddNatives(myself, g_MyNatives);
}

bool SendProxyManager::QueryInterfaceDrop(SMInterface* pInterface)
{
	if (pInterface == sdkhooks || pInterface == bintools)
		return false;

	return true;
}

void SendProxyManager::NotifyInterfaceDrop(SMInterface* pInterface)
{
	SDK_OnUnload();
}

bool SendProxyManager::QueryRunning(char* error, size_t maxlength)
{
	SM_CHECK_IFACE(SDKHOOKS, sdkhooks);
	SM_CHECK_IFACE(BINTOOLS, bintools);

	return true;
}

bool SendProxyManager::RegisterConCommandBase(ConCommandBase* pVar)
{
	// Notify metamod of ownership
	return META_REGCVAR(pVar);
}

void SendProxyManager::SDK_OnUnload()
{
	plsys->RemovePluginsListener(this);
	playerhelpers->RemoveClientListener(this);

	if (sdkhooks)
	{
		sdkhooks->RemoveEntityListener(this);
	}

	ConVar_Unregister();
}

void SendProxyManager::OnEntityDestroyed(CBaseEntity* pEnt)
{
	int index = gamehelpers->EntityToBCompatRef(pEnt);
	g_pSendPropHookManager->UnhookEntityAll(index);
}

void SendProxyManager::OnClientDisconnected(int client)
{
	g_pSendPropHookManager->UnhookEntityAll(client);
}

void SendProxyManager::OnCoreMapStart(edict_t * pEdictList, int edictCount, int clientMax)
{
}

void SendProxyManager::OnCoreMapEnd()
{
	g_pSendPropHookManager->Clear();
}

bool SendProxyManager::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_ANY(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);

	gpGlobals = ismm->GetCGlobals();
	
	sv_parallel_packentities = cvar->FindVar("sv_parallel_packentities");
	if (sv_parallel_packentities == nullptr)
		return false;

	return true;
}

void SendProxyManager::OnPluginUnloaded(IPlugin * plugin)
{
	g_pSendPropHookManager->OnPluginUnloaded(plugin);
}

static CBaseEntity *FindEntityByNetClass(int start, const char *classname)
{
	int maxEntities = gpGlobals->maxEntities;
	for (int i = start; i < maxEntities; i++)
	{
		CBaseEntity *pEntity = gamehelpers->ReferenceToEntity(i);
		if (pEntity == nullptr)
		{
			continue;
		}

		IServerNetworkable* pNetwork = ((IServerUnknown *)pEntity)->GetNetworkable();
		if (pNetwork == nullptr)
		{
			continue;
		}

		ServerClass *pServerClass = pNetwork->GetServerClass();
		if (pServerClass == nullptr)
		{
			continue;
		}

		const char *name = pServerClass->GetName();
		if (!strcmp(name, classname))
		{
			return pEntity;
		}
	}

	return nullptr;
}

CBaseEntity* GetGameRulesProxyEnt()
{
	static cell_t proxyEntRef = -1;
	CBaseEntity *pProxy;
	if (proxyEntRef == -1 || (pProxy = gamehelpers->ReferenceToEntity(proxyEntRef)) == NULL)
	{
		pProxy = FindEntityByNetClass(playerhelpers->GetMaxClients(), g_szGameRulesProxy.c_str());
		if (pProxy)
			proxyEntRef = gamehelpers->EntityToReference(pProxy);
	}
	
	return pProxy;
}
