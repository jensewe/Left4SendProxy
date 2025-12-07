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

SendProxyManager g_SendProxyManager;
SMEXT_LINK(&g_SendProxyManager);

static SendPropHookManager s_SendPropHookManager;
SendPropHookManager *g_pSendPropHookManager = &s_SendPropHookManager;

std::string g_szGameRulesProxy;
IServerGameEnts * gameents = nullptr;
IServerGameClients * gameclients = nullptr;
CGlobalVars *gpGlobals = nullptr;
IBinTools* bintools = nullptr;
ISDKHooks * sdkhooks = nullptr;
ConVar *sv_parallel_packentities = nullptr;

CFrameSnapshotManager* framesnapshotmanager = nullptr;
void* CFrameSnapshotManager::s_pfnTakeTickSnapshot = nullptr;
ICallWrapper* CFrameSnapshotManager::s_callTakeTickSnapshot = nullptr;
void* CFrameSnapshotManager::s_pfnRemoveEntityReference = nullptr;
ICallWrapper* CFrameSnapshotManager::s_callRemoveEntityReference = nullptr;
void* CFrameSnapshot::s_pfnReleaseReference = nullptr;
ICallWrapper *CFrameSnapshot::s_callReleaseReference = nullptr;

bool SendProxyManager::SDK_OnLoad(char *error, size_t maxlen, bool late)
{
	auto gc_sdktools = *AutoGameConfig::Load("sdktools.games");
	auto gc = *AutoGameConfig::Load("sendproxy");
	if (!gc_sdktools || !gc)
		return false;

	g_szGameRulesProxy = gc_sdktools->GetKeyValue("GameRulesProxy");

	GAMECONF_GETSIGNATURE(gc, "CFrameSnapshotManager::TakeTickSnapshot", &CFrameSnapshotManager::s_pfnTakeTickSnapshot);
	GAMECONF_GETSIGNATURE(gc, "CFrameSnapshotManager::RemoveEntityReference", &CFrameSnapshotManager::s_pfnRemoveEntityReference);
	GAMECONF_GETSIGNATURE(gc, "CFrameSnapshot::ReleaseReference", &CFrameSnapshot::s_pfnReleaseReference);
	GAMECONF_GETADDRESS(gc, "framesnapshotmanager", &framesnapshotmanager);

	if (!ClientPacksDetour::Init(gc))
		return false;

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

		CFrameSnapshotManager::s_callTakeTickSnapshot = bintools->CreateCall(CFrameSnapshotManager::s_pfnTakeTickSnapshot, CallConv_ThisCall, &params[2], &params[0], 2);
		if (CFrameSnapshotManager::s_callTakeTickSnapshot == NULL) {
			smutils->LogError(myself, "Unable to create ICallWrapper for \"CFrameSnapshotManager::TakeTickSnapshot\"!");
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
	std::string_view name = pInterface->GetInterfaceName();
	if (name == SMINTERFACE_SDKHOOKS_NAME || name == SMINTERFACE_BINTOOLS_NAME)
		return false;

	return true;
}

void SendProxyManager::NotifyInterfaceDrop(SMInterface* pInterface)
{
	std::string_view name = pInterface->GetInterfaceName();

	if (name == SMINTERFACE_SDKHOOKS_NAME)
	{
		sdkhooks = nullptr;
	}
	else if (name == SMINTERFACE_BINTOOLS_NAME)
	{
		bintools = nullptr;
	}
	
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
	g_pSendPropHookManager->Clear();
	ClientPacksDetour::Shutdown();

	plsys->RemovePluginsListener(this);
	playerhelpers->RemoveClientListener(this);

	if (sdkhooks)
	{
		sdkhooks->RemoveEntityListener(this);
	}

	ConVar_Unregister();

	if (CFrameSnapshot::s_callReleaseReference != nullptr)
	{
		CFrameSnapshot::s_callReleaseReference->Destroy();
		CFrameSnapshot::s_callReleaseReference = nullptr;
	}

	if (CFrameSnapshotManager::s_callTakeTickSnapshot != nullptr)
	{
		CFrameSnapshotManager::s_callTakeTickSnapshot->Destroy();
		CFrameSnapshotManager::s_callTakeTickSnapshot = nullptr;
	}

	if (CFrameSnapshotManager::s_callRemoveEntityReference != nullptr)
	{
		CFrameSnapshotManager::s_callRemoveEntityReference->Destroy();
		CFrameSnapshotManager::s_callRemoveEntityReference = nullptr;
	}
}

void SendProxyManager::OnEntityDestroyed(CBaseEntity* pEnt)
{
	int index = gamehelpers->EntityToBCompatRef(pEnt);
	g_pSendPropHookManager->UnhookEntityAll(index);
}

void SendProxyManager::OnClientDisconnected(int client)
{
	g_pSendPropHookManager->UnhookEntityAll(client);
	ClientPacksDetour::OnClientDisconnect(client);
}

void SendProxyManager::OnCoreMapEnd()
{
	g_pSendPropHookManager->Clear();
}

bool SendProxyManager::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	if (!engine->IsDedicatedServer())
	{
		ke::SafeStrcpy(error, maxlen, "Local server support is deprecated.");
		return false;
	}

	GET_V_IFACE_ANY(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	gpGlobals = ismm->GetCGlobals();
	
	GET_CONVAR(sv_parallel_packentities);

	return true;
}

void SendProxyManager::OnPluginUnloaded(IPlugin * plugin)
{
	g_pSendPropHookManager->OnPluginUnloaded(plugin);
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
