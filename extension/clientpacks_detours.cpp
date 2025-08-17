#include "clientpacks_detours.h"
#include "sendprop_hookmanager.h"
#include "CDetour/detours.h"
#include "wrappers.h"
#include "iclient.h"
#include "util.h"
#include <unordered_map>
#include <array>

DECL_DETOUR(CFrameSnapshotManager_UsePreviouslySentPacket);
DECL_DETOUR(CFrameSnapshotManager_GetPreviouslySentPacket);
DECL_DETOUR(CFrameSnapshotManager_CreatePackedEntity);
DECL_DETOUR(PackEntities_Normal);
DECL_DETOUR(SV_ComputeClientPacks);

volatile int g_iCurrentClientIndexInLoop = -1; //used for optimization

using PackedEntityArray = std::array<PackedEntityHandle_t, MAXPLAYERS>;
std::unordered_map<int, PackedEntityArray> g_EntityPackMap;

ConVar ext_sendproxy_frame_callback("ext_sendproxy_frame_callback", "0", FCVAR_NONE, "Invoke hooked proxy every frame.");

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

DETOUR_DECL_MEMBER3(CFrameSnapshotManager_UsePreviouslySentPacket, bool, CFrameSnapshot*, pSnapshot, int, entity, int, entSerialNumber)
{
	if (g_iCurrentClientIndexInLoop == -1)
		return DETOUR_MEMBER_CALL(CFrameSnapshotManager_UsePreviouslySentPacket)(pSnapshot, entity, entSerialNumber);

	if (g_EntityPackMap[entity][g_iCurrentClientIndexInLoop] == INVALID_PACKED_ENTITY_HANDLE)
		return false;

	if (framesnapshotmanager->m_pLastPackedData[entity] != INVALID_PACKED_ENTITY_HANDLE)
	{
		PackedEntity *newpe = framesnapshotmanager->m_PackedEntities[ framesnapshotmanager->m_pLastPackedData[entity] ];
		PackedEntity *oldpe = framesnapshotmanager->m_PackedEntities[ g_EntityPackMap[entity][g_iCurrentClientIndexInLoop] ];

		if (newpe && oldpe && newpe->m_nSnapshotCreationTick > oldpe->m_nSnapshotCreationTick)
		{
			gamehelpers->EdictOfIndex(entity)->m_fStateFlags |= FL_EDICT_CHANGED;
			return false;
		}
	}

	framesnapshotmanager->m_pLastPackedData[entity] = g_EntityPackMap[entity][g_iCurrentClientIndexInLoop];
	return DETOUR_MEMBER_CALL(CFrameSnapshotManager_UsePreviouslySentPacket)(pSnapshot, entity, entSerialNumber);
}

DETOUR_DECL_MEMBER2(CFrameSnapshotManager_GetPreviouslySentPacket, PackedEntity*, int, entity, int, entSerialNumber)
{
	if (g_iCurrentClientIndexInLoop == -1)
	{
		return DETOUR_MEMBER_CALL(CFrameSnapshotManager_GetPreviouslySentPacket)(entity, entSerialNumber);
	}

	framesnapshotmanager->m_pLastPackedData[entity] = g_EntityPackMap[entity][g_iCurrentClientIndexInLoop];
	return DETOUR_MEMBER_CALL(CFrameSnapshotManager_GetPreviouslySentPacket)(entity, entSerialNumber);
}

DETOUR_DECL_MEMBER2(CFrameSnapshotManager_CreatePackedEntity, PackedEntity*, CFrameSnapshot*, pSnapshot, int, entity)
{
	if (g_iCurrentClientIndexInLoop == -1)
	{
		return DETOUR_MEMBER_CALL(CFrameSnapshotManager_CreatePackedEntity)(pSnapshot, entity);
	}

	PackedEntityHandle_t origHandle = framesnapshotmanager->m_pLastPackedData[entity];

	if (g_EntityPackMap[entity][g_iCurrentClientIndexInLoop] != INVALID_PACKED_ENTITY_HANDLE)
	{
		framesnapshotmanager->m_pLastPackedData[entity] = g_EntityPackMap[entity][g_iCurrentClientIndexInLoop];
	}

	PackedEntity *result = DETOUR_MEMBER_CALL(CFrameSnapshotManager_CreatePackedEntity)(pSnapshot, entity);

	g_EntityPackMap[entity][g_iCurrentClientIndexInLoop] = framesnapshotmanager->m_pLastPackedData[entity];

	return result;
}

static void CopyFrameSnapshot(CFrameSnapshot *dest, const CFrameSnapshot *src);
static void CopyPackedEntities(CFrameSnapshot *dest, const CFrameSnapshot *src);
static bool g_bSetupClientPacks = false;

DETOUR_DECL_STATIC3(PackEntities_Normal, void, int, iClientCount, CGameClient **, pClients, CFrameSnapshot *, pSnapShot)
{
	if (g_bSetupClientPacks)
		return;

	return DETOUR_STATIC_CALL(PackEntities_Normal)(iClientCount, pClients, pSnapShot);
}

DETOUR_DECL_STATIC3(SV_ComputeClientPacks, void, int, iClientCount, CGameClient **, pClients, CFrameSnapshot *, pSnapShot)
{
	if (playerhelpers->GetMaxClients() <= 1
#ifndef DEBUG
	 || iClientCount <= 1
#endif
	 || !g_pSendPropHookManager->IsAnyEntityHooked())
	{
		return DETOUR_STATIC_CALL(SV_ComputeClientPacks)(iClientCount, pClients, pSnapShot);
	}

	const int numEntities = pSnapShot->m_nValidEntities;
	int numHooked = 0;

	// Move all hooked entities to the back
	for (int i = numEntities-1; i >= 0; --i)
	{
		const auto entindex = pSnapShot->m_pValidEntities[i];
		if (g_pSendPropHookManager->IsEntityHooked(entindex))
		{
			const auto tail = pSnapShot->m_nValidEntities - numHooked - 1;
			std::swap(pSnapShot->m_pValidEntities[i], pSnapShot->m_pValidEntities[tail]);
			numHooked++;
		}
	}

	// Make snapshots for each client
	CUtlVector<CFrameSnapshot *> clientSnapshots(0, iClientCount);
	clientSnapshots[0] = pSnapShot;
	for (int i = 1; i < iClientCount; ++i)
	{
		clientSnapshots[i] = framesnapshotmanager->CreateEmptySnapshot(pSnapShot->m_nTickCount, pSnapShot->m_nNumEntities);
		CopyFrameSnapshot(clientSnapshots[i], pSnapShot);
	}

	// Setup transmit infos
	g_bSetupClientPacks = true;
	for (int i = 0; i < iClientCount; ++i)
	{
		CGameClient *client = pClients[i];
		CFrameSnapshot *snapshot = clientSnapshots[i];

		DETOUR_STATIC_CALL(SV_ComputeClientPacks)(1, &client, snapshot);
	}
	g_bSetupClientPacks = false;

	// Pack all unhooked entities
	{
		g_iCurrentClientIndexInLoop = -1;

		pSnapShot->m_nValidEntities = numEntities - numHooked;
		DETOUR_STATIC_CALL(PackEntities_Normal)(iClientCount, pClients, pSnapShot);
		pSnapShot->m_nValidEntities = numEntities;

		for (int i = 1; i < iClientCount; ++i)
		{
			CopyPackedEntities(clientSnapshots[i], pSnapShot);
		}
	}

	// Pack hooked entities for each client
	{
		ConVarSaveSet linearpack(sv_parallel_packentities, "0");

		for (int i = 0; i < iClientCount; ++i)
		{
			CGameClient *client = pClients[i];
			CFrameSnapshot *snapshot = clientSnapshots[i];

			g_iCurrentClientIndexInLoop = client->GetPlayerSlot();

			snapshot->m_pValidEntities += numEntities - numHooked;
			snapshot->m_nValidEntities = numHooked;

			if (ext_sendproxy_frame_callback.GetBool())
				std::for_each_n( snapshot->m_pValidEntities, snapshot->m_nValidEntities, [](int edictidx){ gamehelpers->EdictOfIndex(edictidx)->m_fStateFlags |= FL_EDICT_CHANGED; });
			
			DETOUR_STATIC_CALL(PackEntities_Normal)(1, &client, snapshot);

			snapshot->m_nValidEntities = numEntities;
			snapshot->m_pValidEntities -= numEntities - numHooked;
		}
	}

	g_iCurrentClientIndexInLoop = -1;
}

int ClientPacksDetour::GetCurrentClientIndex()
{
	if (g_iCurrentClientIndexInLoop == -1)
		return -1;

	return g_iCurrentClientIndexInLoop + 1;
}

void ClientPacksDetour::OnEntityHooked(int index)
{
	if (g_EntityPackMap.find(index) == g_EntityPackMap.end())
	{
		PackedEntityArray arr;
		arr.fill(INVALID_PACKED_ENTITY_HANDLE);
		g_EntityPackMap.emplace(index, std::move(arr));
	}
}

void ClientPacksDetour::OnEntityUnhooked(int index)
{
	if (g_EntityPackMap.find(index) == g_EntityPackMap.end())
		return;

	for (PackedEntityHandle_t pack : g_EntityPackMap[index])
	{
		if (pack != INVALID_PACKED_ENTITY_HANDLE
		 && pack != framesnapshotmanager->m_pLastPackedData[index])
		{
			framesnapshotmanager->RemoveEntityReference(pack);
		}
	}
	
	g_EntityPackMap.erase(index);
}

bool ClientPacksDetour::Init(IGameConfig *gc)
{
	CDetourManager::Init(smutils->GetScriptingEngine(), gc);
	
	bool bDetoursInited = true;
	CREATE_DETOUR(CFrameSnapshotManager_UsePreviouslySentPacket, "CFrameSnapshotManager::UsePreviouslySentPacket", bDetoursInited);
	CREATE_DETOUR(CFrameSnapshotManager_GetPreviouslySentPacket, "CFrameSnapshotManager::GetPreviouslySentPacket", bDetoursInited);
	CREATE_DETOUR(CFrameSnapshotManager_CreatePackedEntity, "CFrameSnapshotManager::CreatePackedEntity", bDetoursInited);
	CREATE_DETOUR_STATIC(PackEntities_Normal, "PackEntities_Normal", bDetoursInited);
	CREATE_DETOUR_STATIC(SV_ComputeClientPacks, "SV_ComputeClientPacks", bDetoursInited);
	
	if (!bDetoursInited)
		return false;

	return true;
}

void ClientPacksDetour::Shutdown()
{
	DESTROY_DETOUR(CFrameSnapshotManager_UsePreviouslySentPacket);
	DESTROY_DETOUR(CFrameSnapshotManager_GetPreviouslySentPacket);
	DESTROY_DETOUR(CFrameSnapshotManager_CreatePackedEntity);
	DESTROY_DETOUR(PackEntities_Normal);
	DESTROY_DETOUR(SV_ComputeClientPacks);
}

void ClientPacksDetour::Clear()
{
	g_EntityPackMap.clear();
}

static void CopyFrameSnapshot(CFrameSnapshot *dest, const CFrameSnapshot *src)
{
	Assert(dest->m_nNumEntities == src->m_nNumEntities);
	Q_memcpy(dest->m_pEntities, src->m_pEntities, dest->m_nNumEntities * sizeof(CFrameSnapshotEntry));

	dest->m_nValidEntities = src->m_nValidEntities;
	dest->m_pValidEntities = new unsigned short[dest->m_nValidEntities];
	Q_memcpy(dest->m_pValidEntities, src->m_pValidEntities, dest->m_nValidEntities * sizeof(unsigned short));

	if (src->m_pHLTVEntityData != NULL)
	{
		Assert(dest->m_pHLTVEntityData == NULL);
		dest->m_pHLTVEntityData = new CHLTVEntityData[dest->m_nValidEntities];
		Q_memset( dest->m_pHLTVEntityData, 0, dest->m_nValidEntities * sizeof(CHLTVEntityData) );
	}

	dest->m_iExplicitDeleteSlots = src->m_iExplicitDeleteSlots;

	// FIXME: Copy temp entity data
}

static void CopyPackedEntities(CFrameSnapshot *dest, const CFrameSnapshot *src)
{
	int numEntities = dest->m_nNumEntities;
	for (int i = 0; i < numEntities; ++i)
	{
		auto data = src->m_pEntities[i].m_pPackedData;
		dest->m_pEntities[i].m_pPackedData = data;
		framesnapshotmanager->AddEntityReference(data);
	}

	if (dest->m_pHLTVEntityData && src->m_pHLTVEntityData)
	{
		Q_memcpy( dest->m_pHLTVEntityData, src->m_pHLTVEntityData, dest->m_nValidEntities * sizeof(CHLTVEntityData) );
	}
}
