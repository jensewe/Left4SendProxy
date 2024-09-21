#ifndef _WRAPPERS_H_INC_
#define _WRAPPERS_H_INC_

#include <extensions/IBinTools.h>
#include "tier0/threadtools.h"
#include "tier1/utlvector.h"
#include "tier1/utllinkedlist.h"
#include "tier1/mempool.h"

class ServerClass;
class ClientClass;
class CFrameSnapshotEntry;
class CHLTVEntityData;
class CEventInfo;

class CFrameSnapshot
{
public:
	static void *s_pfnReleaseReference;
	static ICallWrapper *s_callReleaseReference;
	void ReleaseReference()
	{
		struct {
			CFrameSnapshot *pThis;
		} stack{ this };

		s_callReleaseReference->Execute(&stack, NULL);
	}

	// Index info CFrameSnapshotManager::m_FrameSnapshots.
	CInterlockedInt			m_ListIndex;	

	// Associated frame. 
	int						m_nTickCount; // = sv.tickcount
	
	// State information
	CFrameSnapshotEntry		*m_pEntities;	
	int						m_nNumEntities; // = sv.num_edicts

	// This list holds the entities that are in use and that also aren't entities for inactive clients.
	unsigned short			*m_pValidEntities; 
	int						m_nValidEntities;

	// Additional HLTV info
	CHLTVEntityData			*m_pHLTVEntityData; // is NULL if not in HLTV mode or array of m_pValidEntities entries

	CEventInfo				**m_pTempEntities; // temp entities
	int						m_nTempEntities;

	CUtlVector<int>			m_iExplicitDeleteSlots;

	// Snapshots auto-delete themselves when their refcount goes to zero.
	CInterlockedInt			m_nReferences;
};

class PackedEntity;

#define INVALID_PACKED_ENTITY_HANDLE (0)
typedef int PackedEntityHandle_t;

struct UnpackedDataCache_t;
class CFrameSnapshotManager
{
public:
	virtual ~CFrameSnapshotManager( void );

	static void* s_pfnTakeTickSnapshot;
	static ICallWrapper* s_callTakeTickSnapshot;

	CFrameSnapshot* TakeTickSnapshot(int tickcount)
	{
		struct {
			CFrameSnapshotManager *pThis;
			int tickcount;
		} stack{ this, tickcount };

		CFrameSnapshot *ret;
		s_callTakeTickSnapshot->Execute(&stack, &ret);
		return ret;
	}

public:
	int pad[21];

	CUtlFixedLinkedList<PackedEntity *>					m_PackedEntities;

	// FIXME: Update CUtlFixedLinkedList in hl2sdk-l4d2
	int pad2;

	int								m_nPackedEntityCacheCounter;  // increase with every cache access
	CUtlVector<UnpackedDataCache_t>	m_PackedEntityCache;	// cache for uncompressed packed entities

	// The most recently sent packets for each entity
	PackedEntityHandle_t	m_pLastPackedData[ MAX_EDICTS ];
	int						m_pSerialNumber[ MAX_EDICTS ];

	CThreadFastMutex		m_WriteMutex;

	CUtlVector<int>			m_iExplicitDeleteSlots;
};

class CGameClient;

extern CFrameSnapshotManager* framesnapshotmanager;

#endif